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

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
struct BatchCaseSpec
{
	int rowIndex = -1;
	std::string caseName;
	bool enabled = true;
	std::string outputSubdir;
	std::unordered_map<std::string, std::string> overrides;
	std::unordered_map<std::string, std::string> metadata;
};

struct BatchWorkbook
{
	std::vector<BatchCaseSpec> cases;
	std::vector<std::string> headers;
};

struct BatchContext
{
	WindLInput input;
	std::string templateQwdPath;
	std::filesystem::path batchRoot;
	std::string launcher;
	int threadCount = 1;
	bool validateOnly = false;
	std::string executablePath;
	WindLBatchProgressCallback progress;
};

using OverrideApplier = std::function<void(WindLInput &, const std::string &)>;

constexpr const char *kSheetCases = "Cases";
constexpr const char *kColCaseName = "CaseName";
constexpr const char *kColEnabled = "Enabled";
constexpr const char *kColOutputSubdir = "OutputSubdir";
constexpr const char *kOverridePrefix = "Override.";
constexpr const char *kMetaPrefix = "Meta.";

std::string Upper(std::string value)
{
	return ZString::ToUpper(std::move(value));
}

std::string Trimmed(const std::string &value)
{
	return ZString::Trim(value);
}

std::string FormatDouble(double value)
{
	std::ostringstream stream;
	stream.imbue(std::locale::classic());
	stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
	return stream.str();
}

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

bool ParseBoolStrict(const std::string &token)
{
	const std::string text = Upper(Trimmed(token));
	if (text == "1" || text == "TRUE" || text == "YES" || text == "Y" || text == "ON")
		return true;
	if (text == "0" || text == "FALSE" || text == "NO" || text == "N" || text == "OFF")
		return false;
	throw std::runtime_error("Invalid boolean token: " + token);
}

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

	throw std::runtime_error("Invalid enum token: " + token);
}

template <typename T>
T ParseNumberStrict(const std::string &token)
{
	try
	{
		return ZString::StringTo<T>(token);
	}
	catch (...)
	{
		throw std::runtime_error("Invalid numeric token: " + token);
	}
}

template <typename T>
std::string ToText(const T &value)
{
	if constexpr (std::is_same_v<T, bool>)
		return value ? "true" : "false";
	else if constexpr (std::is_arithmetic_v<T>)
		return FormatDouble(static_cast<double>(value));
	else
		return value;
}

std::string NormalizeBatchLauncher(const std::string &value)
{
	const std::string upper = Upper(Trimmed(value));
	if (upper.empty() || upper == "SUBPROCESS" || upper == "CMD" || upper == "POWERSHELL")
		return "subprocess";
	if (upper == "INPROC")
		return "inproc";
	throw std::runtime_error("Unsupported BatchLauncher: " + value);
}

template <typename T>
void SetNumber(T &field, const std::string &token)
{
	field = ParseNumberStrict<T>(token);
}

template <typename E>
void SetEnum(E &field, const std::string &token)
{
	field = ParseEnumStrict<E>(token);
}

void SetText(std::string &field, const std::string &token)
{
	field = Trimmed(token);
}

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

std::string CellString(const OpenXLSX::XLWorksheet &sheet, int rowIndex1, int columnIndex1)
{
	const auto cell = sheet.cell(rowIndex1, columnIndex1);
	const auto type = cell.value().type();
	switch (type)
	{
	case OpenXLSX::XLValueType::Empty: return "";
	case OpenXLSX::XLValueType::Boolean: return cell.value().get<bool>() ? "true" : "false";
	case OpenXLSX::XLValueType::Integer: return std::to_string(cell.value().get<int64_t>());
	case OpenXLSX::XLValueType::Float: return FormatDouble(cell.value().get<double>());
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

BatchWorkbook ReadBatchWorkbook(const WindLInput &input)
{
	if (input.batchExcelPath.empty())
		throw std::runtime_error("Mode=BATCH requires BatchExcel.");

	MSExcel excel(input.batchExcelPath, "read");
	if (!excel.SheetExist(input.batchSheetName))
		throw std::runtime_error("Batch sheet not found: " + input.batchSheetName);

	const auto sheet = excel.GetSheet(input.batchSheetName);
	const int rowCount = static_cast<int>(sheet.rowCount());
	const int columnCount = static_cast<int>(sheet.columnCount());
	if (rowCount <= 0 || columnCount <= 0)
		throw std::runtime_error("Batch workbook sheet is empty: " + input.batchSheetName);

	BatchWorkbook workbook;
	workbook.headers.reserve(static_cast<std::size_t>(columnCount));
	std::unordered_set<std::string> seenHeaders;
	for (int c = 1; c <= columnCount; ++c)
	{
		std::string header = Trimmed(CellString(sheet, 1, c));
		workbook.headers.push_back(header);
		if (header.empty())
			continue;
		if (!seenHeaders.insert(header).second)
			throw std::runtime_error("Duplicate batch header: " + header);

		const bool fixed = header == kColCaseName || header == kColEnabled || header == kColOutputSubdir;
		const bool prefixed = ZString::StartsWith(header, kOverridePrefix) || ZString::StartsWith(header, kMetaPrefix);
		if (!fixed && !prefixed)
			throw std::runtime_error("Unsupported batch column: " + header);
		if (ZString::StartsWith(header, kOverridePrefix))
		{
			const std::string key = header.substr(std::strlen(kOverridePrefix));
			if (OverrideAppliers().find(key) == OverrideAppliers().end())
				throw std::runtime_error("Unsupported Override column: " + header);
		}
	}

	if (seenHeaders.find(kColCaseName) == seenHeaders.end())
		throw std::runtime_error("Batch workbook requires a CaseName column.");

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
			const std::string value = Trimmed(CellString(sheet, r, c));
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
			throw std::runtime_error("Batch row " + std::to_string(r) + " has data but no CaseName.");
		if (!seenCases.insert(item.caseName).second)
			throw std::runtime_error("Duplicate CaseName in batch workbook: " + item.caseName);
		if (item.outputSubdir.empty())
			item.outputSubdir = item.caseName;
		workbook.cases.push_back(std::move(item));
	}

	return workbook;
}

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
			throw std::runtime_error("Unsupported override key: " + key);
		it->second(derived, value);
	}

	return derived;
}

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
		out << "      \"durationSeconds\": " << FormatDouble(item.durationSeconds) << "\n";
		out << "    }";
		if (i + 1 != result.cases.size())
			out << ",";
		out << "\n";
	}
	out << "  ]\n";
	out << "}\n";
}

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
		    << FormatDouble(item.durationSeconds) << ','
		    << item.rowIndex << '\n';
	}
}

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

std::filesystem::path CaseOutputDir(const BatchContext &ctx, const BatchCaseSpec &spec)
{
	return std::filesystem::absolute(ctx.batchRoot / SanitizePathComponent(spec.outputSubdir.empty() ? spec.caseName : spec.outputSubdir));
}

void WriteLogLine(const std::string &path, const std::string &line)
{
	std::ofstream log(path, std::ios::app | std::ios::binary);
	log << line << "\n";
}

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
		throw std::runtime_error("Failed to create batch log file: " + logPath);

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
		throw std::runtime_error("Failed to launch subprocess for case qwd: " + qwdPath);

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

WindLBatchCaseResult ExecuteSingleCase(const BatchContext &ctx, const BatchCaseSpec &spec)
{
	WindLBatchCaseResult result;
	result.caseName = spec.caseName;
	result.rowIndex = spec.rowIndex;
	result.status = "failed";
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
			result.status = "validated";
			result.message = "Pre-validation passed.";
		}
		else if (ctx.launcher == "inproc")
		{
			WriteLogLine(result.logPath, "Running in-process WindL batch case.");
			const auto simResult = SimWind::Generate(derived, [&](const std::string &message) {
				WriteLogLine(result.logPath, message);
			});
			result.status = "success";
			result.message = "Wind field generated successfully.";
			result.btsPath = simResult.btsPath;
			result.bladedWndPath = simResult.bladedWndPath;
			result.turbsimWndPath = simResult.turbsimWndPath;
			result.sumPath = simResult.sumPath;
			result.exitCode = 0;
		}
		else
		{
			WriteLogLine(result.logPath, "Running subprocess WindL batch case.");
			result.exitCode = RunSubprocessQwd(ctx.executablePath,
			                                  result.derivedQwdPath,
			                                  outputDir.string(),
			                                  result.logPath);
			result.status = result.exitCode == 0 ? "success" : "failed";
			result.message = result.exitCode == 0 ? "Wind field generated successfully."
			                                      : "Subprocess exited with code " + std::to_string(result.exitCode) + ".";

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
		result.status = (!validatedInput || ctx.validateOnly) ? "invalid" : "failed";
		result.message = ex.what();
		if (!result.logPath.empty())
			WriteLogLine(result.logPath, ex.what());
	}

	result.durationSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	return result;
}

void TallyResult(const WindLBatchCaseResult &item, WindLBatchResult &result)
{
	if (item.status == "success")
		++result.succeeded;
	else if (item.status == "failed")
		++result.failed;
	else if (item.status == "invalid")
		++result.invalid;
	else if (item.status == "skipped")
		++result.skipped;
	else if (item.status == "validated")
		++result.validated;
}
} // namespace

WindLBatchResult WindLBatch::RunFromFile(const std::string &qwdPath,
                                         const std::string &executablePath,
                                         WindLBatchProgressCallback progress)
{
	BatchContext ctx;
	ctx.templateQwdPath = std::filesystem::absolute(qwdPath).string();
	ctx.input = ReadWindLInput(qwdPath);
	if (ctx.input.mode != Mode::BATCH)
		throw std::runtime_error("WindL batch runner requires Mode=BATCH in the input .qwd.");

	ctx.launcher = NormalizeBatchLauncher(ctx.input.batchLauncher);
	ctx.validateOnly = ctx.input.batchValidateOnly;
	ctx.executablePath = executablePath.empty() ? std::string() : std::filesystem::absolute(executablePath).string();
	ctx.progress = std::move(progress);
	ctx.batchRoot = std::filesystem::absolute(ctx.input.batchOutputDir.empty()
	                                              ? (std::filesystem::path(ctx.templateQwdPath).parent_path() / "batch_output")
	                                              : std::filesystem::path(ctx.input.batchOutputDir));
	std::filesystem::create_directories(ctx.batchRoot);

	if (ctx.progress)
		ctx.progress(" Reading batch Excel workbook \"" + ctx.input.batchExcelPath + "\".");
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
			skipped.status = "skipped";
			skipped.message = "Case disabled by Enabled=false.";
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
		ctx.progress(" Batch workbook loaded: " + std::to_string(workbook.cases.size()) +
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
						ctx.progress(" Starting batch case \"" + spec.caseName + "\".");
				}

				auto caseResult = ExecuteSingleCase(ctx, spec);
				{
					std::lock_guard<std::mutex> lock(progressMutex);
					if (ctx.progress)
						ctx.progress(" Batch case \"" + spec.caseName + "\" finished with status " + caseResult.status + ".");
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
