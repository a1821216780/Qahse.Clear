#include "WindL/Batch/WindLBatch.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IO/MSExcel.h"
#include "IO/ZFile.hpp"
#include "IO/ZString.hpp"
#include "WindL/IO/WindL_IO_Subs.hpp"
#include "IO/LocaleString_WindL.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
/** @brief 批量算例规格，描述 Excel 工作簿中单行的批次执行条目（算例名称、开关、输出目录、参数覆盖及元数据） */
struct BatchCaseSpec
{
	int rowIndex = -1;                                                ///< 在 Excel 表格中的行索引（1-based，不含表头），-1 表示无效
	std::string caseName;                                            ///< 算例名称（用作输出子目录名的基础）
	bool enabled = true;                                             ///< 该算例是否启用（false 则跳过执行）
	std::string outputSubdir;                                        ///< 算例输出子目录名（相对于批次根目录）
	std::unordered_map<std::string, std::string> overrides;          ///< 参数覆盖映射（键为参数路径如 "Turbine.HubHeight"，值为覆盖值）
	std::unordered_map<std::string, std::string> metadata;           ///< 元数据键值对（如作者、备注等，不参与仿真逻辑）
};

/** @brief 已解析的批量工作簿，包含所有算例规格向量及其列头映射 */
struct BatchWorkbook
{
	std::vector<BatchCaseSpec> cases;    ///< 所有解析出的算例规格列表
	std::vector<std::string> headers;    ///< 表格列头字符串，用于索引列名到列号的映射
};

/** @brief 批量执行运行时上下文，封装模板输入、路径配置、线程策略、校验模式及进度回调 */
struct BatchContext
{
	WindLInput input;                              ///< 模板输入参数（作为覆盖应用的基础）
	std::string templateQwdPath;                   ///< 模板 .qwd 文件路径（用于派生各算例的输入文件）
	std::filesystem::path batchRoot;               ///< 批次输出根目录
	std::string launcher;                          ///< 可执行文件启动器路径（可为空，直接调用 exe）
	int threadCount = 1;                           ///< 并行执行线程数
	bool validateOnly = false;                     ///< 仅校验模式（只解析覆盖，不实际运行仿真）
	std::string executablePath;                    ///< 仿真可执行文件完整路径
	WindLBatchProgressCallback progress;           ///< 批次进度回调函数
};

using OverrideApplier = std::function<void(WindLInput &, const std::string &)>;

constexpr const char *kSheetCases = "Cases";
constexpr const char *kColCaseName = "CaseName";
constexpr const char *kColEnabled = "Enabled";
constexpr const char *kColOutputSubdir = "OutputSubdir";
constexpr const char *kOverridePrefix = "Override.";
constexpr const char *kMetaPrefix = "Meta.";

/** @brief 对字符串中的特殊字符进行JSON转义处理，将不可打印字符转换为\uXXXX十六进制转义序列。 */
std::string JsonEscape(const std::string &value)
{
	std::ostringstream stream;
	for (const char ch : value)
	{
		switch (ch)
		{
		case '\\': stream << "\\\\"; break;
		case '\"': stream << "\\\""; break;
		case '\n': stream << "\\n"; break;
		case '\r': stream << "\\r"; break;
		case '\t': stream << "\\t"; break;
		default:
			if (static_cast<unsigned char>(ch) < 0x20)
			{
				stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
				       << static_cast<int>(static_cast<unsigned char>(ch))
				       << std::dec << std::setfill(' ');
			}
			else
			{
				stream << ch;
			}
			break;
		}
	}
	return stream.str();
}

/** @brief 对CSV字段值进行转义处理，含逗号、引号或换行等特殊字符时用双引号包裹并转义内部引号。 */
std::string CsvEscape(const std::string &value)
{
	if (value.find_first_of(",\"\n\r") == std::string::npos)
		return value;

	std::string escaped = value;
	std::size_t pos = 0;
	while ((pos = escaped.find('"', pos)) != std::string::npos)
	{
		escaped.insert(pos, 1, '"');
		pos += 2;
	}
	return "\"" + escaped + "\"";
}

/** @brief 清洗字符串以用作文件路径组件，将非法字符和不可打印字符替换为下划线。 */
std::string SanitizePathComponent(std::string value)
{
	if (value.empty())
		return "case";

	for (char &ch : value)
	{
		switch (ch)
		{
		case '<':
		case '>':
		case ':':
		case '"':
		case '/':
		case '\\':
		case '|':
		case '?':
		case '*':
			ch = '_';
			break;
		default:
			if (static_cast<unsigned char>(ch) < 0x20)
				ch = '_';
			break;
		}
	}
	return value;
}

/** @brief 严格解析布尔值令牌，支持1/TRUE/YES/ON（真）和0/FALSE/NO/OFF（假）形式，解析失败抛出异常。 */
bool ParseBoolStrict(const std::string &token)
{
	const std::string text = ZString::ToUpper(ZString::Trim(token));
	if (text == "1" || text == "TRUE" || text == "YES" || text == "Y" || text == "ON")
		return true;
	if (text == "0" || text == "FALSE" || text == "NO" || text == "N" || text == "OFF")
		return false;
	throw std::runtime_error(std::string(L_BATCH_InvalidBool) + ": " + token);
}

/** @brief 严格解析枚举值令牌，先尝试名称匹配，失败后尝试整数值匹配，均失败则抛出异常。 */
template <typename E>
E ParseEnumStrict(const std::string &token)
{
	static_assert(std::is_enum_v<E>, "ParseEnumStrict requires enum type.");

	try
	{
		return ZString::StringToEnum<E>(token, true);
	}
	catch (...)
	{
	}

	try
	{
		const auto numeric = ZString::StringTo<std::underlying_type_t<E>>(token);
		const auto parsed = magic_enum::enum_cast<E>(numeric);
		if (parsed.has_value())
			return *parsed;
	}
	catch (...)
	{
	}

	throw std::runtime_error(std::string(L_BATCH_InvalidEnum) + ": " + token);
}

/** @brief 严格解析数值令牌，解析失败时抛出异常。 */
template <typename T>
T ParseNumberStrict(const std::string &token)
{
	try
	{
		return ZString::StringTo<T>(token);
	}
	catch (...)
	{
		throw std::runtime_error(std::string(L_BATCH_InvalidNumeric) + ": " + token);
	}
}

/** @brief 将值转换为字符串表示，布尔值输出"true"/"false"，算术类型使用格式化双精度输出。 */
template <typename T>
std::string ToText(const T &value)
{
	if constexpr (std::is_same_v<T, bool>)
		return value ? "true" : "false";
	else if constexpr (std::is_arithmetic_v<T>)
		return ZString::FormatDouble(static_cast<double>(value));
	else
		return value;
}

/** @brief 规范化批处理启动器配置字符串，支持subprocess/cmd/powershell（统一为subprocess）和inproc，其他值抛出异常。 */
std::string NormalizeBatchLauncher(const std::string &value)
{
	const std::string upper = ZString::ToUpper(ZString::Trim(value));
	if (upper.empty() || upper == "SUBPROCESS" || upper == "CMD" || upper == "POWERSHELL")
		return "subprocess";
	if (upper == "INPROC")
		return "inproc";
	throw std::runtime_error(std::string(L_BATCH_UnsupportedLauncher) + ": " + value);
}

/** @brief 将字符串令牌解析为数值并赋值给指定字段。 */
template <typename T>
void SetNumber(T &field, const std::string &token)
{
	field = ParseNumberStrict<T>(token);
}

/** @brief 将字符串令牌解析为枚举值并赋值给指定字段。 */
template <typename E>
void SetEnum(E &field, const std::string &token)
{
	field = ParseEnumStrict<E>(token);
}

/** @brief 将字符串令牌去除首尾空白后赋值给指定字符串字段。 */
void SetText(std::string &field, const std::string &token)
{
	field = ZString::Trim(token);
}

/** @brief 返回覆盖键到应用函数的映射表，支持对WindLInput各参数字段通过字符串键进行批量覆盖设置。 */
const std::unordered_map<std::string, OverrideApplier> &OverrideAppliers()
{
	static const std::unordered_map<std::string, OverrideApplier> appliers{
	    {"TurbModel", [](WindLInput &in, const std::string &v) { SetEnum(in.turbModel, v); }},
	    {"WindModel", [](WindLInput &in, const std::string &v) { SetEnum(in.windModel, v); }},
	    {"CalWu", [](WindLInput &in, const std::string &v) { in.calWu = ParseBoolStrict(v); }},
	    {"CalWv", [](WindLInput &in, const std::string &v) { in.calWv = ParseBoolStrict(v); }},
	    {"CalWw", [](WindLInput &in, const std::string &v) { in.calWw = ParseBoolStrict(v); }},
	    {"WrBlwnd", [](WindLInput &in, const std::string &v) { in.wrBlwnd = ParseBoolStrict(v); }},
	    {"WrTrbts", [](WindLInput &in, const std::string &v) { in.wrTrbts = ParseBoolStrict(v); }},
	    {"WrTrwnd", [](WindLInput &in, const std::string &v) { in.wrTrwnd = ParseBoolStrict(v); }},
	    {"IECStandard", [](WindLInput &in, const std::string &v) { SetEnum(in.iecEdition, v); }},
	    {"IECClass", [](WindLInput &in, const std::string &v) { SetEnum(in.turbineClass, v); }},
	    {"TurbulenceClass", [](WindLInput &in, const std::string &v) { SetEnum(in.turbClass, v); }},
	    {"VRef", [](WindLInput &in, const std::string &v) { SetNumber(in.vRef, v); }},
	    {"RotorDiameter", [](WindLInput &in, const std::string &v) { SetNumber(in.rotorDiameter, v); }},
	    {"MeanWindSpeed", [](WindLInput &in, const std::string &v) { SetNumber(in.meanWindSpeed, v); }},
	    {"HubHt", [](WindLInput &in, const std::string &v) { SetNumber(in.hubHeight, v); }},
	    {"RefHt", [](WindLInput &in, const std::string &v) { SetNumber(in.refHeight, v); }},
	    {"ShearType", [](WindLInput &in, const std::string &v) { SetEnum(in.shearType, v); }},
	    {"WindProfileType", [](WindLInput &in, const std::string &v) { SetEnum(in.windProfileType, v); }},
	    {"PLExp", [](WindLInput &in, const std::string &v) { SetNumber(in.shearExp, v); }},
	    {"Z0", [](WindLInput &in, const std::string &v) { SetNumber(in.roughness, v); }},
	    {"Roughness", [](WindLInput &in, const std::string &v) { SetNumber(in.roughness, v); }},
	    {"HFlowAng", [](WindLInput &in, const std::string &v) { SetNumber(in.horAngle, v); }},
	    {"VFlowAng", [](WindLInput &in, const std::string &v) { SetNumber(in.vertAngle, v); }},
	    {"UserShearFile", [](WindLInput &in, const std::string &v) { SetText(in.userShearFile, v); }},
	    {"TurbIntensity", [](WindLInput &in, const std::string &v) { SetNumber(in.turbIntensity, v); }},
	    {"RandSeed", [](WindLInput &in, const std::string &v) { SetNumber(in.turbSeed, v); }},
	    {"NumPointY", [](WindLInput &in, const std::string &v) { SetNumber(in.gridPtsY, v); }},
	    {"NumPointZ", [](WindLInput &in, const std::string &v) { SetNumber(in.gridPtsZ, v); }},
	    {"LenWidthY", [](WindLInput &in, const std::string &v) { SetNumber(in.fieldDimY, v); }},
	    {"LenHeightZ", [](WindLInput &in, const std::string &v) { SetNumber(in.fieldDimZ, v); }},
	    {"WindDuration", [](WindLInput &in, const std::string &v) { SetNumber(in.simTime, v); }},
	    {"TimeStep", [](WindLInput &in, const std::string &v) { SetNumber(in.timeStep, v); }},
	    {"UserTurbFile", [](WindLInput &in, const std::string &v) { SetText(in.userTurbFile, v); }},
	    {"UseIECSimmga", [](WindLInput &in, const std::string &v) { in.useIECSimmga = ParseBoolStrict(v); }},
	    {"ScaleIEC", [](WindLInput &in, const std::string &v) { SetNumber(in.scaleIEC, v); }},
	    {"ETMc", [](WindLInput &in, const std::string &v) { SetNumber(in.etmC, v); }},
	    {"AnalysisTime", [](WindLInput &in, const std::string &v) { SetNumber(in.analysisTime, v); }},
	    {"Rich_No", [](WindLInput &in, const std::string &v) { SetNumber(in.richardson, v); }},
	    {"UStar", [](WindLInput &in, const std::string &v) { SetNumber(in.uStar, v); }},
	    {"ZL", [](WindLInput &in, const std::string &v) { SetNumber(in.zOverL, v); }},
	    {"ZI", [](WindLInput &in, const std::string &v) { SetNumber(in.mixingLayerDepth, v); }},
	    {"PC_UW", [](WindLInput &in, const std::string &v) { SetNumber(in.reynoldsUW, v); }},
	    {"PC_UV", [](WindLInput &in, const std::string &v) { SetNumber(in.reynoldsUV, v); }},
	    {"PC_VW", [](WindLInput &in, const std::string &v) { SetNumber(in.reynoldsVW, v); }},
	    {"VxLu", [](WindLInput &in, const std::string &v) { SetNumber(in.vkLu, v); }},
	    {"VxLv", [](WindLInput &in, const std::string &v) { SetNumber(in.vkLv, v); }},
	    {"VxLw", [](WindLInput &in, const std::string &v) { SetNumber(in.vkLw, v); }},
	    {"VyLu", [](WindLInput &in, const std::string &v) { SetNumber(in.vyLu, v); }},
	    {"VyLv", [](WindLInput &in, const std::string &v) { SetNumber(in.vyLv, v); }},
	    {"VyLw", [](WindLInput &in, const std::string &v) { SetNumber(in.vyLw, v); }},
	    {"VzLu", [](WindLInput &in, const std::string &v) { SetNumber(in.vzLu, v); }},
	    {"VzLv", [](WindLInput &in, const std::string &v) { SetNumber(in.vzLv, v); }},
	    {"VzLw", [](WindLInput &in, const std::string &v) { SetNumber(in.vzLw, v); }},
	    {"Latitude", [](WindLInput &in, const std::string &v) { SetNumber(in.latitude, v); }},
	    {"TI_u", [](WindLInput &in, const std::string &v) { SetNumber(in.tiU, v); }},
	    {"TI_v", [](WindLInput &in, const std::string &v) { SetNumber(in.tiV, v); }},
	    {"TI_w", [](WindLInput &in, const std::string &v) { SetNumber(in.tiW, v); }},
	    {"MannAlphaEps", [](WindLInput &in, const std::string &v) { SetNumber(in.mannAlphaEps, v); }},
	    {"MannScalelength", [](WindLInput &in, const std::string &v) { SetNumber(in.mannLength, v); }},
	    {"MannGamma", [](WindLInput &in, const std::string &v) { SetNumber(in.mannGamma, v); }},
	    {"MannMaxL", [](WindLInput &in, const std::string &v) { SetNumber(in.mannMaxL, v); }},
	    {"MannNx", [](WindLInput &in, const std::string &v) { SetNumber(in.mannNx, v); }},
	    {"MannNy", [](WindLInput &in, const std::string &v) { SetNumber(in.mannNy, v); }},
	    {"MannNz", [](WindLInput &in, const std::string &v) { SetNumber(in.mannNz, v); }},
	    {"CohMod1", [](WindLInput &in, const std::string &v) { in.cohMod1 = windl_io_detail::ParseCohModel(v, in.cohMod1); }},
	    {"CohMod2", [](WindLInput &in, const std::string &v) { in.cohMod2 = windl_io_detail::ParseCohModel(v, in.cohMod2); }},
	    {"CohMod3", [](WindLInput &in, const std::string &v) { in.cohMod3 = windl_io_detail::ParseCohModel(v, in.cohMod3); }},
	    {"CohDecayU", [](WindLInput &in, const std::string &v) { SetNumber(in.cohDecayU, v); }},
	    {"CohDecayV", [](WindLInput &in, const std::string &v) { SetNumber(in.cohDecayV, v); }},
	    {"CohDecayW", [](WindLInput &in, const std::string &v) { SetNumber(in.cohDecayW, v); }},
	    {"CohScaleB", [](WindLInput &in, const std::string &v) { SetNumber(in.cohScaleB, v); }},
	    {"CohExp", [](WindLInput &in, const std::string &v) { SetNumber(in.cohExp, v); }},
	    {"AllowCohApprox", [](WindLInput &in, const std::string &v) { in.allowCohApprox = ParseBoolStrict(v); }},
	    {"EWMType", [](WindLInput &in, const std::string &v) { SetEnum(in.ewmType, v); }},
	    {"GustPeriod", [](WindLInput &in, const std::string &v) { SetNumber(in.gustPeriod, v); }},
	    {"EventStart", [](WindLInput &in, const std::string &v) { SetNumber(in.eventStart, v); }},
	    {"EventSign", [](WindLInput &in, const std::string &v) { SetEnum(in.eventSign, v); }},
	    {"EcdVcog", [](WindLInput &in, const std::string &v) { SetNumber(in.ecdVcog, v); }},
	    {"WrWndName", [](WindLInput &in, const std::string &v) { SetText(in.saveName, v); }},
	    {"SumPrint", [](WindLInput &in, const std::string &v) { in.sumPrint = ParseBoolStrict(v); }}};
	return appliers;
}

/** @brief 从Excel工作表指定单元格中读取值并转换为字符串返回，支持空、布尔、整数、浮点和字符串类型。 */
std::string CellString(const OpenXLSX::XLWorksheet &sheet, int rowIndex1, int columnIndex1)
{
	const auto cell = sheet.cell(rowIndex1, columnIndex1);
	const auto type = cell.value().type();
	switch (type)
	{
	case OpenXLSX::XLValueType::Empty: return "";
	case OpenXLSX::XLValueType::Boolean: return cell.value().get<bool>() ? "true" : "false";
	case OpenXLSX::XLValueType::Integer: return std::to_string(cell.value().get<int64_t>());
	case OpenXLSX::XLValueType::Float: return ZString::FormatDouble(cell.value().get<double>());
	case OpenXLSX::XLValueType::String: return cell.value().get<std::string>();
	default:
		try
		{
			return cell.value().get<std::string>();
		}
		catch (...)
		{
			return "";
		}
	}
}

/**
 * @brief 从WindLInput指定的Excel工作簿中读取批处理案例定义。
 *
 * 读取指定工作表的列头（CaseName/Enabled/OutputSubdir及Override.Meta.*列），
 * 逐行解析每个案例的配置信息和覆盖参数，构建BatchWorkbook结构返回。
 *
 * @param input 包含batchExcelPath和batchSheetName的WindLInput配置
 * @return 解析完成的BatchWorkbook，包含所有案例规范和列头信息
 * @note 列头必须为CaseName/Enabled/OutputSubdir或以Override./Meta.为前缀；
 *       重复列头和不支持的Override键将抛出异常；
 *       空白行自动跳过，有数据但CaseName为空的行抛出异常；
 *       重复CaseName抛出异常。
 * @code
 *   WindLInput input = ReadWindLInput("template.qwd");
 *   BatchWorkbook workbook = ReadBatchWorkbook(input);
 *   for (const auto &spec : workbook.cases)
 *       std::cout << spec.caseName << "\n";
 * @endcode
 */
BatchWorkbook ReadBatchWorkbook(const WindLInput &input)
{
	if (input.batchExcelPath.empty())
		throw std::runtime_error(L_BATCH_RequiresBatchExcel);

	MSExcel excel(input.batchExcelPath, "read");
	if (!excel.SheetExist(input.batchSheetName))
		throw std::runtime_error(std::string(L_BATCH_SheetNotFound) + ": " + input.batchSheetName);

	const auto sheet = excel.GetSheet(input.batchSheetName);
	const int rowCount = static_cast<int>(sheet.rowCount());
	const int columnCount = static_cast<int>(sheet.columnCount());
	if (rowCount <= 0 || columnCount <= 0)
		throw std::runtime_error(std::string(L_BATCH_SheetEmpty) + ": " + input.batchSheetName);

	BatchWorkbook workbook;
	workbook.headers.reserve(static_cast<std::size_t>(columnCount));
	std::unordered_set<std::string> seenHeaders;
	for (int c = 1; c <= columnCount; ++c)
	{
		std::string header = ZString::Trim(CellString(sheet, 1, c));
		workbook.headers.push_back(header);
		if (header.empty())
			continue;
		if (!seenHeaders.insert(header).second)
			throw std::runtime_error(std::string(L_BATCH_DuplicateHeader) + ": " + header);

		const bool fixed = header == kColCaseName || header == kColEnabled || header == kColOutputSubdir;
		const bool prefixed = ZString::StartsWith(header, kOverridePrefix) || ZString::StartsWith(header, kMetaPrefix);
		if (!fixed && !prefixed)
			throw std::runtime_error(std::string(L_BATCH_UnsupportedColumn) + ": " + header);
		if (ZString::StartsWith(header, kOverridePrefix))
		{
			const std::string key = header.substr(std::strlen(kOverridePrefix));
			if (OverrideAppliers().find(key) == OverrideAppliers().end())
				throw std::runtime_error(std::string(L_BATCH_UnsupportedOverrideCol) + ": " + header);
		}
	}

	if (seenHeaders.find(kColCaseName) == seenHeaders.end())
		throw std::runtime_error(L_BATCH_RequiresCaseNameCol);

	std::unordered_set<std::string> seenCases;
	for (int r = 2; r <= rowCount; ++r)
	{
		bool hasAnyData = false;
		BatchCaseSpec item;
		item.rowIndex = r;
		item.enabled = true;

		for (int c = 1; c <= columnCount; ++c)
		{
			const std::string header = workbook.headers[static_cast<std::size_t>(c - 1)];
			if (header.empty())
				continue;
			const std::string value = ZString::Trim(CellString(sheet, r, c));
			if (!value.empty())
				hasAnyData = true;
			if (header == kColCaseName)
			{
				item.caseName = value;
			}
			else if (header == kColEnabled)
			{
				if (!value.empty())
					item.enabled = ParseBoolStrict(value);
			}
			else if (header == kColOutputSubdir)
			{
				item.outputSubdir = value;
			}
			else if (ZString::StartsWith(header, kOverridePrefix))
			{
				if (!value.empty())
					item.overrides.emplace(header.substr(std::strlen(kOverridePrefix)), value);
			}
			else if (ZString::StartsWith(header, kMetaPrefix))
			{
				if (!value.empty())
					item.metadata.emplace(header.substr(std::strlen(kMetaPrefix)), value);
			}
		}

		if (!hasAnyData)
			continue;
		if (item.caseName.empty())
			throw std::runtime_error(std::string(L_BATCH_RowNoCaseName) + ": " + std::to_string(r));
		if (!seenCases.insert(item.caseName).second)
			throw std::runtime_error(std::string(L_BATCH_DuplicateCaseName) + ": " + item.caseName);
		if (item.outputSubdir.empty())
			item.outputSubdir = item.caseName;
		workbook.cases.push_back(std::move(item));
	}

	return workbook;
}

/**
 * @brief 基于基础输入和案例覆盖规范生成派生WindLInput。
 *
 * 复制基础输入，清除批处理相关字段，将mode设为GENERATE，然后逐一应用案例特定的覆盖参数。
 *
 * @param base 基础WindLInput配置模板
 * @param spec 包含覆盖键值对的BatchCaseSpec案例规范
 * @return 应用覆盖后的派生WindLInput，mode已设置为GENERATE
 * @note 覆盖键通过OverrideAppliers()映射表查找对应应用函数；不支持的覆盖键将抛出异常。
 * @code
 *   WindLInput derived = ApplyOverrides(baseInput, caseSpec);
 *   WriteWindLInput(derived, "case_output.qwd");
 * @endcode
 */
WindLInput ApplyOverrides(const WindLInput &base, const BatchCaseSpec &spec)
{
	WindLInput derived = base;
	derived.mode = Mode::GENERATE;
	derived.batchExcelPath.clear();
	derived.batchSheetName = kSheetCases;
	derived.batchOutputDir.clear();
	derived.batchThreads = 0;
	derived.batchLauncher = "subprocess";
	derived.batchValidateOnly = false;

	for (const auto &[key, value] : spec.overrides)
	{
		const auto it = OverrideAppliers().find(key);
		if (it == OverrideAppliers().end())
			throw std::runtime_error(std::string(L_BATCH_UnsupportedOverrideKey) + ": " + key);
		it->second(derived, value);
	}

	return derived;
}

/**
 * @brief 将批处理运行结果写入JSON清单文件。
 *
 * 输出总体统计信息（totalCases/succeeded/failed/invalid/skipped/validated）
 * 以及每个案例的详细结果（名称、状态、消息、输出路径、退出码、耗时等）。
 *
 * @param result 批处理运行结果，包含统计数据和各案例结果
 * @note 输出文件路径由result.manifestPath指定；案例字符串值通过JsonEscape转义。
 * @code
 *   WriteBatchManifest(result);
 * @endcode
 */
void WriteBatchManifest(const WindLBatchResult &result)
{
	std::ofstream out(result.manifestPath, std::ios::binary);
	out << "{\n";
	out << "  \"totalCases\": " << result.totalCases << ",\n";
	out << "  \"succeeded\": " << result.succeeded << ",\n";
	out << "  \"failed\": " << result.failed << ",\n";
	out << "  \"invalid\": " << result.invalid << ",\n";
	out << "  \"skipped\": " << result.skipped << ",\n";
	out << "  \"validated\": " << result.validated << ",\n";
	out << "  \"cases\": [\n";
	for (std::size_t i = 0; i < result.cases.size(); ++i)
	{
		const auto &item = result.cases[i];
		out << "    {\n";
		out << "      \"caseName\": \"" << JsonEscape(item.caseName) << "\",\n";
		out << "      \"status\": \"" << JsonEscape(item.status) << "\",\n";
		out << "      \"message\": \"" << JsonEscape(item.message) << "\",\n";
		out << "      \"outputDir\": \"" << JsonEscape(item.outputDir) << "\",\n";
		out << "      \"derivedQwdPath\": \"" << JsonEscape(item.derivedQwdPath) << "\",\n";
		out << "      \"logPath\": \"" << JsonEscape(item.logPath) << "\",\n";
		out << "      \"btsPath\": \"" << JsonEscape(item.btsPath) << "\",\n";
		out << "      \"bladedWndPath\": \"" << JsonEscape(item.bladedWndPath) << "\",\n";
		out << "      \"turbsimWndPath\": \"" << JsonEscape(item.turbsimWndPath) << "\",\n";
		out << "      \"sumPath\": \"" << JsonEscape(item.sumPath) << "\",\n";
		out << "      \"rowIndex\": " << item.rowIndex << ",\n";
		out << "      \"exitCode\": " << item.exitCode << ",\n";
		out << "      \"durationSeconds\": " << ZString::FormatDouble(item.durationSeconds) << "\n";
		out << "    }";
		if (i + 1 != result.cases.size())
			out << ",";
		out << "\n";
	}
	out << "  ]\n";
	out << "}\n";
}

/**
 * @brief 将批处理运行结果写入CSV状态文件。
 *
 * 输出每行包含案例名称、状态、消息、输出目录、派生QWD路径、日志路径、退出码、耗时、行索引。
 *
 * @param result 批处理运行结果，包含各案例的详细输出信息
 * @note 输出文件路径由result.csvPath指定；CSV字段使用CsvEscape进行转义。
 * @code
 *   WriteBatchCsv(result);
 * @endcode
 */
void WriteBatchCsv(const WindLBatchResult &result)
{
	std::ofstream out(result.csvPath, std::ios::binary);
	out << "CaseName,Status,Message,OutputDir,DerivedQwd,LogPath,ExitCode,DurationSeconds,RowIndex\n";
	for (const auto &item : result.cases)
	{
		out << CsvEscape(item.caseName) << ','
		    << CsvEscape(item.status) << ','
		    << CsvEscape(item.message) << ','
		    << CsvEscape(item.outputDir) << ','
		    << CsvEscape(item.derivedQwdPath) << ','
		    << CsvEscape(item.logPath) << ','
		    << item.exitCode << ','
		    << ZString::FormatDouble(item.durationSeconds) << ','
		    << item.rowIndex << '\n';
	}
}

/**
 * @brief 将批处理运行结果写入文本摘要文件。
 *
 * 输出运行模式标题（验证或生成）、总体统计信息，并按案例逐行输出状态和消息。
 *
 * @param result 批处理运行结果，包含统计数据和各案例状态
 * @param validateOnly 是否仅验证模式，影响摘要标题文本
 * @note 输出文件路径由result.summaryPath指定。
 * @code
 *   WriteBatchSummary(result, false);
 * @endcode
 */
void WriteBatchSummary(const WindLBatchResult &result, bool validateOnly)
{
	std::ofstream out(result.summaryPath, std::ios::binary);
	out << "WindL batch " << (validateOnly ? "validation" : "generation") << " summary\n";
	out << "Total cases: " << result.totalCases << "\n";
	out << "Succeeded: " << result.succeeded << "\n";
	out << "Validated: " << result.validated << "\n";
	out << "Failed: " << result.failed << "\n";
	out << "Invalid: " << result.invalid << "\n";
	out << "Skipped: " << result.skipped << "\n";
	out << "\n";
	for (const auto &item : result.cases)
		out << "[" << item.status << "] " << item.caseName << " - " << item.message << "\n";
}

/** @brief 根据配置值和任务数计算有效线程数，配置为0时自动检测硬件并发数，上限为任务数。 */
int EffectiveThreadCount(int configured, std::size_t taskCount)
{
	if (taskCount == 0)
		return 1;
	if (configured > 0)
		return std::max(1, std::min(configured, static_cast<int>(taskCount)));
	const unsigned int hw = std::thread::hardware_concurrency();
	const int fallback = hw == 0 ? 1 : static_cast<int>(hw);
	return std::max(1, std::min(fallback, static_cast<int>(taskCount)));
}

/** @brief 计算案例的输出目录绝对路径，基于批处理根目录和案例输出子目录名（默认使用案例名称）。 */
std::filesystem::path CaseOutputDir(const BatchContext &ctx, const BatchCaseSpec &spec)
{
	return std::filesystem::absolute(ctx.batchRoot / SanitizePathComponent(spec.outputSubdir.empty() ? spec.caseName : spec.outputSubdir));
}

/** @brief 以追加模式向指定日志文件写入一行文本。 */
void WriteLogLine(const std::string &path, const std::string &line)
{
	std::ofstream log(path, std::ios::app | std::ios::binary);
	log << line << "\n";
}

/**
 * @brief 通过子进程运行QWD文件生成风场。
 *
 * Windows平台使用CreateProcess创建无窗口子进程，重定向stdout/stderr到日志文件；
 * 其他平台使用system()调用命令行并重定向输出。
 *
 * @param executablePath 可执行文件路径
 * @param qwdPath QWD输入文件路径
 * @param workingDir 子进程工作目录
 * @param logPath 日志输出文件路径
 * @return 子进程退出码，成功为0
 * @note Windows版本通过SECURITY_ATTRIBUTES继承日志句柄实现重定向；
 *       非Windows版本将输出重定向到日志文件并合并stderr。
 * @code
 *   int exitCode = RunSubprocessQwd("/path/to/windl.exe", "case.qwd", ".", "output.log");
 * @endcode
 */
#ifdef _WIN32
int RunSubprocessQwd(const std::string &executablePath,
                     const std::string &qwdPath,
                     const std::string &workingDir,
                     const std::string &logPath)
{
	std::filesystem::create_directories(std::filesystem::path(logPath).parent_path());
	SECURITY_ATTRIBUTES security{};
	security.nLength = sizeof(security);
	security.bInheritHandle = TRUE;

	const std::wstring logWide = std::filesystem::path(logPath).wstring();
	HANDLE logHandle = CreateFileW(logWide.c_str(),
	                               FILE_APPEND_DATA,
	                               FILE_SHARE_READ | FILE_SHARE_WRITE,
	                               &security,
	                               OPEN_ALWAYS,
	                               FILE_ATTRIBUTE_NORMAL,
	                               nullptr);
	if (logHandle == INVALID_HANDLE_VALUE)
		throw std::runtime_error(std::string(L_BATCH_FailCreateLog) + ": " + logPath);

	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdOutput = logHandle;
	startup.hStdError = logHandle;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	PROCESS_INFORMATION process{};
	const std::wstring exeWide = std::filesystem::path(executablePath).wstring();
	const std::wstring qwdWide = std::filesystem::path(qwdPath).wstring();
	std::wstring command = L"\"" + exeWide + L"\" --qwd \"" + qwdWide + L"\"";
	std::vector<wchar_t> buffer(command.begin(), command.end());
	buffer.push_back(L'\0');

	const std::wstring workWide = std::filesystem::path(workingDir).wstring();
	const BOOL created = CreateProcessW(nullptr,
	                                    buffer.data(),
	                                    nullptr,
	                                    nullptr,
	                                    TRUE,
	                                    CREATE_NO_WINDOW,
	                                    nullptr,
	                                    workWide.c_str(),
	                                    &startup,
	                                    &process);
	CloseHandle(logHandle);
	if (!created)
		throw std::runtime_error(std::string(L_BATCH_FailLaunchSubprocess) + ": " + qwdPath);

	WaitForSingleObject(process.hProcess, INFINITE);
	DWORD exitCode = 1;
	GetExitCodeProcess(process.hProcess, &exitCode);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return static_cast<int>(exitCode);
}
#else
int RunSubprocessQwd(const std::string &executablePath,
                     const std::string &qwdPath,
                     const std::string & /*workingDir*/,
                     const std::string &logPath)
{
	const std::string command = "\"" + executablePath + "\" --qwd \"" + qwdPath + "\" > \"" + logPath + "\" 2>&1";
	return std::system(command.c_str());
}
#endif

/**
 * @brief 执行单个批处理案例的完整流程。
 *
 * 依次执行：应用覆盖参数生成派生输入 → 创建输出目录 → 写入派生QWD → 验证输入 →
 * 根据launcher模式（subprocess/inproc）运行风场生成。validateOnly模式下仅验证不生成。
 * 异常情况下标记为invalid或failed。
 *
 * @param ctx 批处理上下文，包含基础输入、启动器模式、验证模式等全局配置
 * @param spec 当前案例的规范，包含案例名称、覆盖参数等
 * @return WindLBatchCaseResult 包含执行状态、耗时、输出路径等完整结果
 * @note subprocess模式通过子进程运行并检测输出文件存在性确认结果；
 *       inproc模式在进程内直接调用SimWind::Generate；
 *       validateOnly模式仅执行SimWind::ValidateInputOnly，状态标记为validated；
 *       输入验证失败在validateOnly模式下标记为invalid，否则标记为failed。
 * @code
 *   BatchContext ctx;
 *   ctx.input = baseInput;
 *   ctx.launcher = "subprocess";
 *   BatchCaseSpec spec{"Case1", ...};
 *   WindLBatchCaseResult result = ExecuteSingleCase(ctx, spec);
 * @endcode
 */
WindLBatchCaseResult ExecuteSingleCase(const BatchContext &ctx, const BatchCaseSpec &spec)
{
	WindLBatchCaseResult result;
	result.caseName = spec.caseName;
	result.rowIndex = spec.rowIndex;
	result.status = L_STATUS_Failed;
	const auto start = std::chrono::steady_clock::now();
	bool validatedInput = false;

	try
	{
		auto derived = ApplyOverrides(ctx.input, spec);
		const auto outputDir = CaseOutputDir(ctx, spec);
		std::filesystem::create_directories(outputDir);
		derived.savePath = outputDir.string();
		derived.saveName = SanitizePathComponent(spec.caseName);
		derived.mode = Mode::GENERATE;
		result.outputDir = outputDir.string();
		result.derivedQwdPath = (outputDir / (SanitizePathComponent(spec.caseName) + ".qwd")).string();
		result.logPath = (outputDir / (SanitizePathComponent(spec.caseName) + ".log")).string();

		WriteWindLInput(derived, result.derivedQwdPath);
		SimWind::ValidateInputOnly(derived);
		validatedInput = true;

		if (ctx.validateOnly)
		{
			result.status = L_STATUS_Validated;
			result.message = L_CASE_PreValidationPassed;
		}
		else if (ctx.launcher == "inproc")
		{
			WriteLogLine(result.logPath, L_CASE_RunningInproc);
			const auto simResult = SimWind::Generate(derived, [&](const std::string &message) {
				WriteLogLine(result.logPath, message);
			});
			result.status = L_STATUS_Success;
			result.message = L_CASE_WindFieldGenerated;
			result.btsPath = simResult.btsPath;
			result.bladedWndPath = simResult.bladedWndPath;
			result.turbsimWndPath = simResult.turbsimWndPath;
			result.sumPath = simResult.sumPath;
			result.exitCode = 0;
		}
		else
		{
			WriteLogLine(result.logPath, L_CASE_RunningSubprocess);
			result.exitCode = RunSubprocessQwd(ctx.executablePath,
			                                  result.derivedQwdPath,
			                                  outputDir.string(),
			                                  result.logPath);
			result.status = result.exitCode == 0 ? L_STATUS_Success : L_STATUS_Failed;
			result.message = result.exitCode == 0 ? L_CASE_WindFieldGenerated
			                                      : std::string(L_CASE_SubprocessExitedWithCode) + std::to_string(result.exitCode) + ".";

			const std::filesystem::path basePath = outputDir / SanitizePathComponent(spec.caseName);
			const auto btsPath = basePath;
			const auto bladedWndPath = basePath;
			const auto tsWndPath = outputDir / (SanitizePathComponent(spec.caseName) + ".ts.wnd");
			const auto sumPath = basePath;
			if (std::filesystem::exists(btsPath.string() + ".bts")) result.btsPath = btsPath.string() + ".bts";
			if (std::filesystem::exists(bladedWndPath.string() + ".wnd")) result.bladedWndPath = bladedWndPath.string() + ".wnd";
			if (std::filesystem::exists(tsWndPath)) result.turbsimWndPath = tsWndPath.string();
			if (std::filesystem::exists(sumPath.string() + ".sum")) result.sumPath = sumPath.string() + ".sum";
		}
	}
	catch (const std::exception &ex)
	{
		result.status = (!validatedInput || ctx.validateOnly) ? L_STATUS_Invalid : L_STATUS_Failed;
		result.message = ex.what();
		if (!result.logPath.empty())
			WriteLogLine(result.logPath, ex.what());
	}

	result.durationSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	return result;
}

/** @brief 根据案例结果状态（success/failed/invalid/skipped/validated）累加计入批次汇总统计。 */
void TallyResult(const WindLBatchCaseResult &item, WindLBatchResult &result)
{
	if (item.status == L_STATUS_Success)
		++result.succeeded;
	else if (item.status == L_STATUS_Failed)
		++result.failed;
	else if (item.status == L_STATUS_Invalid)
		++result.invalid;
	else if (item.status == L_STATUS_Skipped)
		++result.skipped;
	else if (item.status == L_STATUS_Validated)
		++result.validated;
}
} // namespace

/**
 * @brief 从QWD文件运行批处理风场生成任务，这是批处理的主入口函数。
 *
 * 完整流程：读取QWD输入 → 验证Mode=BATCH → 解析Excel批处理工作簿 → 计算线程数 →
 * 标记disabled案例为skipped → 多线程并行执行各enabled案例 → 汇总结果 →
 * 自动写入batch_manifest.json、batch_status.csv和batch_summary.txt。
 *
 * @param qwdPath 模板QWD文件路径，其Mode必须为BATCH
 * @param executablePath 子进程模式下的可执行文件路径（inproc模式可为空）
 * @param progress 进度回调函数，用于报告批处理执行进展
 * @return WindLBatchResult 包含总体统计信息和各案例详细结果的批处理运行结果
 * @note 批处理输出目录默认为QWD文件同级的batch_output目录；
 *       disabled案例自动跳过并标记为skipped；
 *       enabled案例通过原子索引分配任务以多线程并行执行；
 *       执行完毕后自动写入清单、CSV和摘要三个输出文件。
 * @code
 *   WindLBatchResult result = WindLBatch::RunFromFile(
 *       "template.qwd", "windl.exe",
 *       [](const std::string &msg) { std::cout << msg << "\n"; });
 *   std::cout << "Succeeded: " << result.succeeded << "\n";
 * @endcode
 */
WindLBatchResult WindLBatch::RunFromFile(const std::string &qwdPath,
                                         const std::string &executablePath,
                                         WindLBatchProgressCallback progress)
{
	BatchContext ctx;
	ctx.templateQwdPath = std::filesystem::absolute(qwdPath).string();
	ctx.input = ReadWindLInput(qwdPath);
	if (ctx.input.mode != Mode::BATCH)
		throw std::runtime_error(L_BATCH_RequiresModeBatch);

	ctx.launcher = NormalizeBatchLauncher(ctx.input.batchLauncher);
	ctx.validateOnly = ctx.input.batchValidateOnly;
	ctx.executablePath = executablePath.empty() ? std::string() : std::filesystem::absolute(executablePath).string();
	ctx.progress = std::move(progress);
	ctx.batchRoot = std::filesystem::absolute(ctx.input.batchOutputDir.empty()
	                                              ? (std::filesystem::path(ctx.templateQwdPath).parent_path() / "batch_output")
	                                              : std::filesystem::path(ctx.input.batchOutputDir));
	std::filesystem::create_directories(ctx.batchRoot);

	if (ctx.progress)
		ctx.progress(std::string(L_BATCH_ReadingWorkbook) + ctx.input.batchExcelPath + "\".");
	const BatchWorkbook workbook = ReadBatchWorkbook(ctx.input);
	ctx.threadCount = EffectiveThreadCount(ctx.input.batchThreads, workbook.cases.size());

	WindLBatchResult result;
	result.totalCases = static_cast<int>(workbook.cases.size());
	result.manifestPath = (ctx.batchRoot / "batch_manifest.json").string();
	result.csvPath = (ctx.batchRoot / "batch_status.csv").string();
	result.summaryPath = (ctx.batchRoot / "batch_summary.txt").string();
	result.cases.reserve(workbook.cases.size());

	std::vector<BatchCaseSpec> runnable;
	runnable.reserve(workbook.cases.size());
	for (const auto &item : workbook.cases)
	{
		if (!item.enabled)
		{
			WindLBatchCaseResult skipped;
			skipped.caseName = item.caseName;
			skipped.rowIndex = item.rowIndex;
			skipped.status = L_STATUS_Skipped;
			skipped.message = L_CASE_DisabledByEnabled;
			skipped.outputDir = CaseOutputDir(ctx, item).string();
			result.cases.push_back(std::move(skipped));
			continue;
		}
		runnable.push_back(item);
		result.cases.emplace_back();
		result.cases.back().caseName = item.caseName;
		result.cases.back().rowIndex = item.rowIndex;
		result.cases.back().outputDir = CaseOutputDir(ctx, item).string();
	}

	if (ctx.progress)
	{
		ctx.progress(std::string(L_BATCH_WorkbookLoaded) + ": " + std::to_string(workbook.cases.size()) +
		             " rows, runnable cases=" + std::to_string(runnable.size()) +
		             ", launcher=" + ctx.launcher +
		             ", threads=" + std::to_string(ctx.threadCount) +
		             (ctx.validateOnly ? ", validate-only." : "."));
	}

	std::unordered_map<std::string, std::size_t> resultIndexByCase;
	for (std::size_t i = 0; i < result.cases.size(); ++i)
		resultIndexByCase[result.cases[i].caseName] = i;

	std::atomic<std::size_t> nextIndex{0};
	std::mutex progressMutex;
	std::vector<std::thread> workers;
	workers.reserve(static_cast<std::size_t>(ctx.threadCount));

	for (int worker = 0; worker < ctx.threadCount; ++worker)
	{
		workers.emplace_back([&]() {
			for (;;)
			{
				const std::size_t index = nextIndex.fetch_add(1);
				if (index >= runnable.size())
					break;

				const auto &spec = runnable[index];
				{
					std::lock_guard<std::mutex> lock(progressMutex);
					if (ctx.progress)
						ctx.progress(std::string(L_BATCH_StartCase) + spec.caseName + "\".");
				}

				auto caseResult = ExecuteSingleCase(ctx, spec);
				{
					std::lock_guard<std::mutex> lock(progressMutex);
					if (ctx.progress)
						ctx.progress(std::string(L_BATCH_StartCase) + spec.caseName + L_BATCH_CaseFinished + caseResult.status + ".");
				}

				const auto it = resultIndexByCase.find(spec.caseName);
				if (it != resultIndexByCase.end())
					result.cases[it->second] = std::move(caseResult);
			}
		});
	}

	for (auto &worker : workers)
		worker.join();

	for (const auto &item : result.cases)
		TallyResult(item, result);

	WriteBatchManifest(result);
	WriteBatchCsv(result);
	WriteBatchSummary(result, ctx.validateOnly);

	return result;
}
