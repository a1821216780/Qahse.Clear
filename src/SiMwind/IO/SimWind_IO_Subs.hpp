//**********************************************************************************************************************************
// 许可证说明
// 版权所有(C) 2021, 2025  赵子祯
//
// 根据 Boost 软件许可证 - 版本 1.0 - 2003年8月17日
// 您不得使用此文件，除非符合许可证。
// 您可以在以下网址获得许可证副本
//
//     http://www.hawtc.cn/licenses.txt
//
// 该软件按"原样"提供，不提供任何明示或暗示的保证，包括但不限于适销性、特定用途的适用性、所有权和非侵权。在任何情况下，版权持有人或任何分
// 发软件的人都不对任何索赔、损害或其他责任负责，无论是在合同诉讼、侵权诉讼或其他诉讼中，还是在软件使用或其他交易中产生的。
//
//**********************************************************************************************************************************

// ───────────────────────────────── File Info ─────────────────────────────────
//
// 该文件提供 SimWind 模块的 IO 子系统，包括 YAML 序列化辅助函数、QWD 文件解析、
// SimWindInput 验证和输出路径推导。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../SimWind_Type.hpp"
#include "../../IO/Serializer.hpp"
#include "../../IO/Yaml.hpp"
#include "../../IO/ZFile.hpp"
#include "../../IO/ZPath.hpp"
#include "../../IO/ZString.hpp"
#include "../../IO/LocaleString.hpp"

namespace simwind_io_detail
{
/** @brief YAML 文档根键名称常量 "Qahse.SimWind"，所有 YAML 键均以此为前缀 */
	inline constexpr const char *kYamlRoot = "Qahse.SimWind";

	/**
 * @brief 构造带根键前缀的完整 YAML 键名
 * @param key 不带前缀的键名
 * @return 完整 YAML 键名，格式为 "Qahse.SimWind.{key}"
 * @note 确保 YAML 输出中的键名在 Qahse.SimWind 命名空间下
 */
inline std::string SimWindYamlKey(const std::string &key)
	{
		return std::string(kYamlRoot) + "." + key;
	}

	/**
 * @brief 判断给定路径是否为 YAML 文件
 * @param path 文件路径
 * @return 扩展名为 .yml 或 .yaml 时返回 true（不区分大小写）
 * @note 用于在 Read/Write 函数中自动选择文本或 YAML 解析器
 */
inline bool IsYamlPath(const std::string &path)
	{
		std::string ext = ZString::ToUpper(std::filesystem::path(path).extension().string());
		return ext == ".YML" || ext == ".YAML";
	}

	/**
 * @brief 检查文件是否存在，不存在则抛出异常
 * @param path 要检查的文件路径
 * @throws std::runtime_error 当文件不存在时，错误信息包含文件路径
 */
inline void RequireFile(const std::string &path)
	{
		if (!ZFile::Exists(path))
			throw std::runtime_error(std::string(L_IO_FileNotFound) + ": " + path);
	}

	/**
 * @brief 根据序列化格式返回带前缀或不带前缀的键名
 * @param format 序列化格式枚举（Yaml 或 Text）
 * @param key 原始键名
 * @return YAML 格式返回 "Qahse.SimWind.key"，文本格式返回原始 key
 */
inline std::string KeyForFormat(Serializer::Format format, const std::string &key)
	{
		return format == Serializer::Format::Yaml ? SimWindYamlKey(key) : key;
	}

	/**
 * @brief 将相干模型枚举值转换为文本标识
 * @param value CohModel 枚举值
 * @return 对应的文本标识："IEC"、"GENERAL"、"NONE"、"API" 或 "default"
 * @note DEFAULT_COH 和未识别的值均返回 "default"
 */
inline std::string CohModelToText(CohModel value)
	{
		switch (value)
		{
		case CohModel::IEC: return "IEC";
		case CohModel::GENERAL: return "GENERAL";
		case CohModel::NONE: return "NONE";
		case CohModel::API: return "API";
		case CohModel::DEFAULT_COH: return "default";
		default: return "default";
		}
	}

	/**
 * @brief 从文本令牌解析相干模型枚举值
 * @param token 待解析字符串，支持名称（不区分大小写）和数字编码：
 *              "DEFAULT"/"DEFAULT_COH"/"0"→IEC, "1"→GENERAL, "3"→NONE, "4"→API
 * @param defaultValue 解析失败或令牌为空时的默认返回值（默认为 CohModel::DEFAULT_COH）
 * @return 解析得到的 CohModel 枚举值
 * @note 解析逻辑：先匹配已知别名，再尝试 ZString::StringToEnum 通用转换，
 *       全部失败时回退到 defaultValue
 */
inline CohModel ParseCohModel(std::string token, CohModel defaultValue = CohModel::DEFAULT_COH)
	{
		token = ZString::Trim(token);
		if (token.empty())
			return defaultValue;

		const std::string upper = ZString::ToUpper(std::string(token));
		if (upper == "DEFAULT" || upper == "DEFAULT_COH")
			return CohModel::DEFAULT_COH;
		if (upper == "IEC" || token == "0")
			return CohModel::IEC;
		if (upper == "GENERAL" || token == "1")
			return CohModel::GENERAL;
		if (upper == "NONE" || token == "3")
			return CohModel::NONE;
		if (upper == "API" || token == "4")
			return CohModel::API;

		try { return ZString::StringToEnum<CohModel>(token, true); }
		catch (...) { return defaultValue; }
	}

	/** @brief 对输入数据应用派生字段计算（当前为空实现，预留扩展点） */
inline void ApplyDerivedFields(SimWindInput &input)
	{
	}

	/**
 * @brief 若路径非空，将其相对于基准路径解析为绝对路径
 * @param baseFilePath 基准文件路径
 * @param path 待解析的路径（输入输出参数，非空时原地修改为绝对路径）
 * @note 使用 ZPath::ResolvePath 进行路径拼接，空路径不做处理
 */
inline void ResolveIfSet(const std::string &baseFilePath, std::string &path)
	{
		if (!path.empty())
			path = ZPath::ResolvePath(baseFilePath, path);
	}

	/**
 * @brief 解析 SimWindInput 中所有文件路径并确保输出目录存在
 * @param input SimWindInput 结构体（输入输出参数）
 * @param baseFilePath 基准文件路径（通常是 .qwd 文件所在路径）
 * @note 解析的路径包括：userShearFile、userTurbFile、wndFilePath、batchExcelPath
 *       自动创建 savePath 目录，若无 batchOutputDir 且模式为 BATCH 则默认使用 "batch_output"
 */
inline void ResolvePaths(SimWindInput &input, const std::string &baseFilePath)
	{
		ResolveIfSet(baseFilePath, input.userShearFile);
		ResolveIfSet(baseFilePath, input.userTurbFile);
		ResolveIfSet(baseFilePath, input.wndFilePath);
		ResolveIfSet(baseFilePath, input.batchExcelPath);

		if (!input.savePath.empty())
		{
			input.savePath = ZPath::ResolvePath(baseFilePath, input.savePath);
			std::filesystem::create_directories(input.savePath);
		}

		if (!input.batchOutputDir.empty())
			input.batchOutputDir = ZPath::ResolvePath(baseFilePath, input.batchOutputDir);
		else if (input.mode == Mode::BATCH)
			input.batchOutputDir = (std::filesystem::path(baseFilePath).parent_path() / "batch_output").string();
	}

/**
 * @brief 序列化字段引用辅助结构体，将关键字字符串与数据成员引用配对用于序列化
 * @tparam T 字段值的类型
 * @details 成员说明：
 *          - \c key：C 字符串字面量，作为序列化键名（如 "Mode"、"TurbModel"）
 *          - \c value：数据成员的左值引用，序列化器通过此引用直接读写 SimWindInput 字段
 *          配合 Field() 便捷工厂函数和 SimWindInputSerializer::Fields() 折叠表达式使用，
 *          在编译期将关键字与数据成员绑定，避免运行时字符串查找开销。
 * @note 用于 SimWindInputSerializer，将键名字符串与数据成员引用绑定
 */
	template <typename T>
	struct FieldRef
	{
		const char *key;
		T &value;
	};

/**
 * @brief 创建 FieldRef 的便捷工厂函数模板，将键名和数据成员引用配对
 * @tparam T 字段值类型（由 value 参数自动推导）
 * @param key 字段键名（C 字符串字面量，如 "Mode"、"TurbModel"）
 * @param value 数据成员的左值引用
 * @return 包含键名和值引用的 FieldRef<T> 对象
 * @note 类型推导避免了显式指定模板参数，简化 SimWindInputSerializer::Fields 调用中的字段绑定语法。
 *       返回的 FieldRef 在 Fold 表达式中依次传递给 One() 完成 ReadOrWrite 操作。
 * @code
 * Field("Mode", data.mode)  // 返回 FieldRef<int>{key="Mode", value=data.mode}
 * @endcode
 */
template <typename T>
FieldRef<T> Field(const char *key, T &value)
	{
		return {key, value};
	}

/**
	 * @class SimWindInputSerializer
	 * @brief SimWindInput 序列化器，负责 .qwd 文本和 YAML 格式的序列化/反序列化
	 *
	 * 继承自 Serializer 基类，将 SimWindInput 数据作为公开成员 data 直接管理
	 * （外部可通过 serializer.data 访问解析/待写入的数据）。
	 * 序列化时采用 ValueFirst 模式（值在前、关键字在后），以兼容传统 .qwd 文本文件格式。
	 * 普通枚举字段通过 magic_enum 自动转换，但相干模型 CohModel 字段
	 * （CohMod1、CohMod2、CohMod3）需通过 CohField() 方法使用自定义的
	 * CohModelToText / ParseCohModel 函数进行文本转换，以匹配 .qwd 格式中
	 * "IEC" / "GENERAL" / "NONE" / "API" 等文本标识（magic_enum 无法处理此映射）。
	 * 在 YAML 输出中，所有字段键名均通过 KeyForFormat 自动添加 "Qahse.SimWind." 根键前缀。
	 */
	class SimWindInputSerializer : public Serializer
	{
	public:
		SimWindInput data;

/** @brief 默认构造函数，初始化空数据成员并设置 ValueFirst 值优先输出模式
		 * @return 无（构造函数）
		 * @note 设置 ValueFirst=true 使文本输出格式为"值在前、关键字在后"，兼容 .qwd 文本格式。
		 *       data 成员保持默认初始化状态，适用于 ReadXxxFile 读取场景。
		 *       写入时需使用带参构造函数或先填充 data 成员。
		 */
		SimWindInputSerializer()
		{
			SetValueFirst(true);
		}

/**
		 * @brief 从已有 SimWindInput 数据构造序列化器，用于后续写入操作
		 * @param input 要序列化输出的 SimWindInput 数据的副本
		 * @return 无（构造函数）
		 * @note 设置 ValueFirst=true 使文本输出格式为"值在前、关键字在后"，兼容 .qwd 文本格式。
		 *       构造后可直接调用 WriteTextFile / WriteYamlFile 将 data 写入文件。
		 */
		explicit SimWindInputSerializer(const SimWindInput &input)
			: data(input)
		{
			SetValueFirst(true);
		}

	protected:
/**
		 * @brief 重写 Serializer::SerializeFields，定义所有字段的关键字-数据成员映射关系
		 * @return 无
		 * @note 通过 Fields() 折叠表达式批量注册约 80 个字段的 Field() 映射，将 .qwd 关键字
		 *       直接绑定到 SimWindInput 数据成员。普通枚举字段由 magic_enum 自动处理序列化/反序列化，
		 *       但 CohMod1/CohMod2/CohMod3 三个相干模型字段使用 CohField() 单独注册，
		 *       通过 CohModelToText / ParseCohModel 进行自定义文本转换（magic_enum 无法
		 *       匹配 .qwd 格式中 "IEC"/"GENERAL"/"NONE"/"API" 等文本标识）。
		 *       这是 Serializer 框架的入口点：基类的 ReadFile/WriteFile 流程最终调用此方法完成所有字段的读写。
		 */
		void SerializeFields() override
		{
			Fields(
				Field("Mode", data.mode),
				Field("TurbModel", data.turbModel),
				Field("WindModel", data.windModel),
				Field("CalWu", data.calWu),
				Field("CalWv", data.calWv),
				Field("CalWw", data.calWw),
				Field("WrBlwnd", data.wrBlwnd),
				Field("WrTrbts", data.wrTrbts),
				Field("WrTrwnd", data.wrTrwnd),
				Field("IECStandard", data.iecEdition),
				Field("IECClass", data.turbineClass),
				Field("TurbulenceClass", data.turbClass),
				Field("VRef", data.vRef),
				Field("RotorDiameter", data.rotorDiameter),
				Field("MeanWindSpeed", data.meanWindSpeed),
				Field("HubHt", data.hubHeight),
				Field("RefHt", data.refHeight),
				Field("ShearType", data.shearType),
				Field("WindProfileType", data.windProfileType),
				Field("PLExp", data.shearExp),
				Field("Z0", data.roughness),
				Field("Roughness", data.roughness),
				Field("HFlowAng", data.horAngle),
				Field("VFlowAng", data.vertAngle),
				Field("UserShearFile", data.userShearFile),
				Field("TurbIntensity", data.turbIntensity),
				Field("RandSeed", data.turbSeed),
				Field("NumPointY", data.gridPtsY),
				Field("NumPointZ", data.gridPtsZ),
				Field("LenWidthY", data.fieldDimY),
				Field("LenHeightZ", data.fieldDimZ),
				Field("WindDuration", data.simTime),
				Field("TimeStep", data.timeStep),
				Field("CycleWind", data.cycleWind),
				Field("GenMethod", data.genMethod),
				Field("UseFFT", data.useFFT),
				Field("InterpMethod", data.interpMethod),
				Field("UserTurbFile", data.userTurbFile),
				Field("UseIECSimmga", data.useIECSimmga),
				Field("ScaleIEC", data.scaleIEC),
				Field("ETMc", data.etmC),
				Field("UsableTime", data.usableTime),
				Field("AnalysisTime", data.analysisTime),
				Field("Rich_No", data.richardson),
				Field("UStar", data.uStar),
				Field("ZL", data.zOverL),
				Field("ZI", data.mixingLayerDepth),
				Field("PC_UW", data.reynoldsUW),
				Field("PC_UV", data.reynoldsUV),
				Field("PC_VW", data.reynoldsVW),
				Field("VxLu", data.vkLu),
				Field("VxLv", data.vkLv),
				Field("VxLw", data.vkLw),
				Field("VyLu", data.vyLu),
				Field("VyLv", data.vyLv),
				Field("VyLw", data.vyLw),
				Field("VzLu", data.vzLu),
				Field("VzLv", data.vzLv),
				Field("VzLw", data.vzLw),
				Field("Latitude", data.latitude),
				Field("TI_u", data.tiU),
				Field("TI_v", data.tiV),
				Field("TI_w", data.tiW),
				Field("MannAlphaEps", data.mannAlphaEps),
				Field("MannScalelength", data.mannLength),
				Field("MannGamma", data.mannGamma),
				Field("MannMaxL", data.mannMaxL),
				Field("MannNx", data.mannNx),
				Field("MannNy", data.mannNy),
				Field("MannNz", data.mannNz));

			CohField("CohMod1", data.cohMod1);
			CohField("CohMod2", data.cohMod2);
			CohField("CohMod3", data.cohMod3);

		Fields(
			Field("CohDecayU", data.cohDecayU),
			Field("CohDecayV", data.cohDecayV),
				Field("CohDecayW", data.cohDecayW),
				Field("CohScaleB", data.cohScaleB),
				Field("CohExp", data.cohExp),
				Field("AllowCohApprox", data.allowCohApprox),
				Field("EWMType", data.ewmType),
				Field("GustPeriod", data.gustPeriod),
				Field("EventStart", data.eventStart),
				Field("EventSign", data.eventSign),
				Field("EcdVcog", data.ecdVcog),
				Field("TurWindFile", data.wndFilePath),
				Field("WndFormat", data.wndFormat),
				Field("WrWndPath", data.savePath),
				Field("WrWndName", data.saveName),
				Field("SumPrint", data.sumPrint),
				Field("BatchExcel", data.batchExcelPath),
				Field("BatchSheet", data.batchSheetName),
				Field("BatchOutputDir", data.batchOutputDir),
				Field("BatchThreads", data.batchThreads),
				Field("BatchLauncher", data.batchLauncher),
				Field("BatchValidateOnly", data.batchValidateOnly));
		}

	private:
/**
		 * @brief 读写单个序列化字段，调用基类 ReadOrWrite 完成底层 I/O 操作
		 * @tparam T 字段值类型，由 FieldRef<T> 自动推导
		 * @param field 字段引用对象，包含键名字符串和数据成员左值引用
		 * @return 无
		 * @note 内部调用 ReadOrWrite(KeyForFormat(GetFormat(), field.key), field.value)。
		 *       KeyForFormat 根据当前序列化格式适配键名：YAML 格式自动添加 "Qahse.SimWind." 前缀
		 *       （如 "Mode" → "Qahse.SimWind.Mode"），文本格式保持原始键名不变以匹配 .qwd 格式。
		 *       ReadOrWrite 继承自 Serializer 基类，根据当前读/写模式自动选择读取或写入路径，
		 *       对普通类型直接读写，对枚举类型自动通过 magic_enum 进行字符串转换。
		 */
		template <typename T>
		void One(FieldRef<T> field)
		{
			ReadOrWrite(KeyForFormat(GetFormat(), field.key), field.value);
		}

/**
		 * @brief 批量读写多个字段，通过 C++17 折叠表达式依次调用 One()
		 * @tparam FieldsT 字段引用参数包类型，每个参数为 FieldRef<T> 实例
		 * @param fields 可变数量的 FieldRef 对象，由 Field() 工厂函数创建
		 * @return 无
		 * @note 使用折叠表达式 (One(fields), ...) 将参数包展开为逗号分隔的 One() 调用序列，
		 *       从左到右依次对每个字段执行 ReadOrWrite 操作。该编译期展开使得在 SerializeFields 中
		 *       可以以 Fields(Field("k1", v1), Field("k2", v2), ...) 的简洁语法批量注册所有字段，
		 *       无需手动逐一调用 One()，同时避免了运行时循环和字符串查找开销。
		 */
		template <typename... FieldsT>
		void Fields(FieldsT... fields)
		{
			(One(fields), ...);
		}

/**
		 * @brief 以文本形式读写相干模型 CohModel 枚举字段，不走 magic_enum 通用枚举转换
		 * @param key 字段键名（如 "CohMod1"、"CohMod2"、"CohMod3"）
		 * @param value 相干模型枚举值的左值引用，读取完成后被 ParseCohModel 的结果覆盖
		 * @return 无
		 * @note CohModel 不能用 magic_enum 处理（其枚举值（如 0/1/3/4）与 .qwd 格式中的
		 *       "IEC"/"GENERAL"/"NONE"/"API" 文本标识不存在一一对应关系），因此需要专用处理流程：
		 *       写入时通过 CohModelToText 将枚举值转为文本字符串（如 CohModel::IEC → "IEC"），
		 *       读取时先通过 ReadOrWrite 将文本读入临时字符串，再调用 ParseCohModel
		 *       将文本还原为 CohModel 枚举值并回写至 value 引用。
		 *       ReadOrWrite 的读写切换由基类 Serializer 的 IsReadMode() 判断。
		 */
		void CohField(const char *key, CohModel &value)
		{
			std::string text = CohModelToText(value);
			ReadOrWrite(KeyForFormat(GetFormat(), key), text);
			if (IsReadMode())
				value = ParseCohModel(text, value);
		}
	};

	/**
 * @brief 向 YAML 文档添加带 "Qahse.SimWind." 根键前缀的字符串节点
 * @param yaml YML 对象
 * @param key 不带前缀的键名
 * @param value 字符串值
 * @note 内部调用 yaml.AddNode(SimWindYamlKey(key), value)
 */
inline void AddYamlNode(YML &yaml, const std::string &key, const std::string &value)
	{
		yaml.AddNode(SimWindYamlKey(key), value);
	}

/**
 * @brief 将任意类型值转换为 YAML 字符串后添加为带根键前缀的节点
 * @tparam T 值类型（需支持 YML::ToYmlValueString 转换）
 * @param yaml YML 对象
 * @param key 不带前缀的键名
 * @param value 要写入的值
 * @note 内部调用 AddYamlNode，自动添加 "Qahse.SimWind." 根键前缀
 */
template <typename T>
void AddYamlValue(YML &yaml, const std::string &key, const T &value)
	{
		AddYamlNode(yaml, key, YML::ToYmlValueString(value));
	}

	/**
 * @brief 从 YAML 文档读取整数值
 * @param yaml YML 对象
 * @param key 不带根键前缀的键名
 * @param defaultValue 键不存在或为空时的默认返回值
 * @return 读取的整数值
 * @note 通过 SimWindYamlKey 自动添加 "Qahse.SimWind." 前缀查找
 */
inline int ReadYamlInt(const YML &yaml, const std::string &key, int defaultValue = 0)
	{
		const std::string value = yaml.read(SimWindYamlKey(key));
		return ZString::Trim(value).empty() ? defaultValue : YML::YmlToInt(value);
	}

	/**
 * @brief 从 YAML 文档读取二维双精度矩阵
 * @param yaml YML 对象
 * @param key 不带根键前缀的键名
 * @return 二维 double 数组，键不存在时返回空数组
 * @note 通过 SimWindYamlKey 自动添加 "Qahse.SimWind." 前缀
 *       内部调用 YML::YmlTo2DDoubleArray 解析嵌套列表格式
 */
inline std::vector<std::vector<double>> ReadYamlMatrix(const YML &yaml, const std::string &key)
	{
		const std::string value = yaml.read(SimWindYamlKey(key));
		if (ZString::Trim(value).empty())
			return {};
		return YML::YmlTo2DDoubleArray(value).data;
	}

	/**
 * @brief 将双精度数组以空格分隔拼接为字符串
 * @param values 双精度数值序列
 * @return 空格分隔的数字字符串
 * @note 使用 std::setprecision(max_digits10) 保证完全精度往返（文本写入后读回不丢精度）
 *       使用 C locale 避免区域设置影响小数点格式
 * @code
 * JoinNumbers({1.5, 2.3, 3.7}); // "1.5 2.3 3.7"
 * @endcode
 */
inline std::string JoinNumbers(const std::vector<double> &values)
	{
		std::ostringstream stream;
		stream.imbue(std::locale::classic());
		stream << std::setprecision(std::numeric_limits<double>::max_digits10);
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			if (i != 0)
				stream << ' ';
			stream << values[i];
		}
		return stream.str();
	}

	/**
 * @brief 尝试将一行文本解析为浮点数序列
 * @param line 待解析的文本行
 * @param row 输出参数，解析到的 double 序列
 * @return 解析到至少一个数字时返回 true
 * @note 跳过空行、以 "--" 或 "!" 开头的注释行
 *       使用 C locale 确保小数点解析一致性
 */
inline bool TryParseNumberRow(const std::string &line, std::vector<double> &row)
	{
		const std::string text = ZString::Trim(line);
		if (text.empty() || ZString::StartsWith(text, "--") || ZString::StartsWith(text, "!"))
			return false;

		std::istringstream stream(text);
		stream.imbue(std::locale::classic());
		double value = 0.0;
		while (stream >> value)
			row.push_back(value);
		return !row.empty();
	}

	/**
 * @brief 解析 "值 关键字" 格式的文本行
 * @param line 待解析的文本行
 * @param key 输出参数，解析到的关键字（放在值之后）
 * @param value 输出参数，解析到的值（放在关键字之前）
 * @return 解析成功返回 true
 * @note 跳过空行、以 "--"、"#"、"!" 开头的注释行
 *       "值" 不能为 "-"（表示缺失）
 *       格式示例："42.0 NumUSRz" → key="NumUSRz", value="42.0"
 */
inline bool TryParseKeywordLine(const std::string &line, std::string &key, std::string &value)
	{
		const std::string text = ZString::TrimStart(line);
		if (text.empty() || ZString::StartsWith(text, "--") || ZString::StartsWith(text, "#") ||
		    ZString::StartsWith(text, "!"))
		{
			return false;
		}

		std::istringstream stream(text);
		if (!(stream >> value >> key))
			return false;

		return !key.empty() && value != "-";
	}

/**
 * @brief 在文本行列表中查找指定关键字并解析其值
 * @tparam T 返回值类型（需支持 ZString::StringTo<T> 转换）
 * @param lines 文本行列表（关键字-值格式：值在前，关键字在后）
 * @param targetKey 目标关键字（不区分大小写匹配）
 * @param defaultValue 未找到或值为 "DEFAULT" 时的默认返回值
 * @return 解析得到的值或默认值
 * @note 逐行调用 TryParseKeywordLine 解析，关键字比较不区分大小写
 *       若解析异常则返回默认值
 */
template <typename T>
T ReadKeywordValue(const std::vector<std::string> &lines, const std::string &targetKey, T defaultValue)
	{
		const std::string target = ZString::ToUpper(targetKey);
		for (const auto &line : lines)
		{
			std::string key;
			std::string value;
			if (!TryParseKeywordLine(line, key, value))
				continue;
			if (ZString::ToUpper(std::string(key)) != target)
				continue;

			if (ZString::ToUpper(ZString::Trim(value)) == "DEFAULT")
				return defaultValue;

			try { return ZString::StringTo<T>(value); }
			catch (...) { return defaultValue; }
		}
		return defaultValue;
	}

/**
 * @brief 读取 !Begin 标记之后的数据行
 * @param lines 所有输入文本行
 * @param expectedCols 期望的列数（0 表示不限制列数）
 * @param maxRows 最大行数限制（0 表示不限制）
 * @return 二维双精度数组
 * @note 从第一个包含 "!BEGIN"（不区分大小写）的行之后开始解析
 *       忽略空行、注释行（"--"、"!"开头）和列数不足的行
 *       如果行数据超过 expectedCols 则截断至该列数
 *       通常用于读取 .dat 文件中结构化表格数据
 * @code
 * auto rows = ReadRowsAfterBegin(lines, 5, 100); // 5列，最多100行
 * @endcode
 */
inline std::vector<std::vector<double>> ReadRowsAfterBegin(const std::vector<std::string> &lines,
                                                           std::size_t expectedCols,
                                                           std::size_t maxRows = 0)
	{
		std::size_t start = 0;
		for (std::size_t i = 0; i < lines.size(); ++i)
		{
			const std::string text = ZString::ToUpper(ZString::Trim(lines[i]));
			if (ZString::StartsWith(text, "!BEGIN"))
			{
				start = i + 1;
				break;
			}
		}

		std::vector<std::vector<double>> rows;
		for (std::size_t i = start; i < lines.size(); ++i)
		{
			std::vector<double> row;
			if (!TryParseNumberRow(lines[i], row))
				continue;
			if (expectedCols != 0 && row.size() < expectedCols)
				continue;

			if (expectedCols != 0 && row.size() > expectedCols)
				row.resize(expectedCols);

			rows.push_back(std::move(row));
			if (maxRows != 0 && rows.size() >= maxRows)
				break;
		}
		return rows;
	}

/**
 * @brief 计算多个容器的最小尺寸
 * @tparam Columns 容器类型（可变参数包，需提供 .size() 方法）
 * @param columns 可变数量的容器引用
 * @return 所有容器中的最小元素数
 * @note 用于列转行时确定安全行数，避免越界访问
 */
template <typename... Columns>
std::size_t MinColumnSize(const Columns &...columns)
	{
		return std::min({columns.size()...});
	}

/**
 * @brief 将多个列向量转换为行优先的二维向量
 * @tparam Columns 列容器类型（可变参数包）
 * @param columns 可变数量的列向量
 * @return 行优先二维向量，行数为所有列的最小长度
 * @note 超出最小行长的元素被丢弃，使用 MinColumnSize 确定行数
 * @code
 * std::vector<double> h = {10,20,30}, ws = {5,6,7};
 * auto rows = ColumnsToRows(h, ws); // {{10,5}, {20,6}, {30,7}}
 * @endcode
 */
template <typename... Columns>
std::vector<std::vector<double>> ColumnsToRows(const Columns &...columns)
	{
		const std::size_t rowCount = MinColumnSize(columns...);
		std::vector<std::vector<double>> rows;
		rows.reserve(rowCount);
		for (std::size_t i = 0; i < rowCount; ++i)
			rows.push_back({columns[i]...});
		return rows;
	}

/**
 * @brief 将单行数据的各元素按顺序追加到对应列向量末尾
 * @tparam Columns 列容器类型（可变参数包）
 * @param row 单行数据，各元素依次对应各列
 * @param columns 可变数量的列向量引用
 * @note 使用折叠表达式按索引递增顺序将 row[index++] 分别 push_back 到各列
 */
template <typename... Columns>
void AppendColumns(const std::vector<double> &row, Columns &...columns)
	{
		std::size_t index = 0;
		((columns.push_back(row[index++])), ...);
	}

/**
 * @brief 将行优先二维数据按列加载到各列向量中
 * @tparam Columns 列容器类型（可变参数包）
 * @param rows 行优先的二维输入数据
 * @param columns 可变数量的列向量引用（函数内部会先清空）
 * @note 跳过列数不足 sizeof...(Columns) 的行，每一行的元素按顺序分配到对应列
 * @code
 * std::vector<double> freq, u, v, w;
 * LoadColumns(rows, freq, u, v, w); // 4列
 * @endcode
 */
template <typename... Columns>
void LoadColumns(const std::vector<std::vector<double>> &rows, Columns &...columns)
	{
		constexpr std::size_t count = sizeof...(Columns);
		((columns.clear()), ...);
		for (const auto &row : rows)
		{
			if (row.size() >= count)
				AppendColumns(row, columns...);
		}
	}

	/**
 * @brief 将二维行数据格式化为空格分隔的数字字符串并追加到行向量
 * @param lines 目标行向量（输出参数）
 * @param rows 二维双精度数据输入
 * @note 调用 JoinNumbers 将每行转换为字符串，使用最大精度输出以保证数值精度不丢失
 */
inline void AddRows(std::vector<std::string> &lines, const std::vector<std::vector<double>> &rows)
	{
		for (const auto &row : rows)
			lines.push_back(JoinNumbers(row));
	}

	/**
 * @brief 将用户风速数据的测点列表转换为 (y, z) 行格式
 * @param data 用户风速数据
 * @return 二维向量，每行为 {point.y, point.z}
 * @note 用于文本格式输出中 "Pointyi Pointzi" 区域的数据生成
 */
inline std::vector<std::vector<double>> PointRows(const UserWindSpeedData &data)
	{
		std::vector<std::vector<double>> rows;
		rows.reserve(data.points.size());
		for (const auto &point : data.points)
			rows.push_back({point.y, point.z});
		return rows;
	}

	/**
 * @brief 获取有效风速分量数
 * @param data 用户风速数据
 * @return 有效分量数（优先取 nComp 字段，其次取第一个点的分量向量大小，最后返回 0）
 */
inline int EffectiveComponentCount(const UserWindSpeedData &data)
	{
		if (data.nComp > 0)
			return data.nComp;
		if (!data.components.empty())
			return static_cast<int>(data.components.front().size());
		return 0;
	}

	/**
 * @brief 获取有效测点数
 * @param data 用户风速数据
 * @return 有效测点数（优先取 nPoints 字段，其次取 points 向量大小，最后取 components 向量大小）
 */
inline int EffectivePointCount(const UserWindSpeedData &data)
	{
		if (data.nPoints > 0)
			return data.nPoints;
		if (!data.points.empty())
			return static_cast<int>(data.points.size());
		return static_cast<int>(data.components.size());
	}

	/**
 * @brief 将用户风速数据（时间序列+多点多分量）展平为逐行格式
 * @param data 用户风速时间序列数据
 * @return 二维向量，每行首列为时间值，随后按 p=0..nPoints-1, c=0..nComp-1 的顺序排列各点各分量风速
 * @note 使用 EffectivePointCount / EffectiveComponentCount 确定维度
 *       对于超出数据范围的元素（点索引越界、分量索引越界或时间步越界）用 0.0 填充
 *       输出行数等于 data.time 的长度
 */
inline std::vector<std::vector<double>> WindSpeedRows(const UserWindSpeedData &data)
	{
		const int nPoints = EffectivePointCount(data);
		const int nComp = EffectiveComponentCount(data);
		std::vector<std::vector<double>> rows;
		rows.reserve(data.time.size());

		for (std::size_t t = 0; t < data.time.size(); ++t)
		{
			std::vector<double> row;
			row.reserve(static_cast<std::size_t>(1 + nPoints * nComp));
			row.push_back(data.time[t]);
			for (int p = 0; p < nPoints; ++p)
			{
				for (int c = 0; c < nComp; ++c)
				{
					double value = 0.0;
					const std::size_t ps = static_cast<std::size_t>(p);
					const std::size_t cs = static_cast<std::size_t>(c);
					if (ps < data.components.size() &&
					    cs < data.components[ps].size() &&
					    t < data.components[ps][cs].size())
					{
						value = data.components[ps][cs][t];
					}
					row.push_back(value);
				}
			}
			rows.push_back(std::move(row));
		}
		return rows;
	}

	/**
 * @brief 从文本行中读取风速测点坐标行（在 "Pointyi Pointzi" 与 "Time Series" 之间）
 * @param lines 所有文本行
 * @param nPoints 期望读取的测点数量（用于提前终止和缓冲区预留）
 * @return 二维向量，每行为 {yi, zi}（仅保留前两列）
 * @note 从包含 "Pointyi" 且 "Pointzi" 的行之后开始读取，遇到 "Time Series" 或达到 nPoints 时停止
 *       跳过空行、注释行（"--"）和包含括号的行（如单位行）
 */
inline std::vector<std::vector<double>> ReadPointRows(const std::vector<std::string> &lines, int nPoints)
	{
		bool inPointBlock = false;
		std::vector<std::vector<double>> rows;
		rows.reserve(static_cast<std::size_t>(std::max(nPoints, 0)));

		for (const auto &line : lines)
		{
			const std::string text = ZString::Trim(line);
			if (!inPointBlock)
			{
				if (text.find("Pointyi") != std::string::npos && text.find("Pointzi") != std::string::npos)
					inPointBlock = true;
				continue;
			}

			if (text.find("Time Series") != std::string::npos)
				break;
			if (text.empty() || ZString::StartsWith(text, "--") || text.find('(') != std::string::npos)
				continue;

			std::vector<double> row;
			if (TryParseNumberRow(text, row) && row.size() >= 2)
			{
				row.resize(2);
				rows.push_back(std::move(row));
				if (nPoints > 0 && rows.size() >= static_cast<std::size_t>(nPoints))
					break;
			}
		}
		return rows;
	}

	/**
 * @brief 将分量索引转换为小写字母名称
 * @param index 分量索引（0→'u', 1→'v', 2→'w', 3+→从 'a' 开始偏移）
 * @return 对应的字符标识
 * @note 用于生成风速时间序列列标题中的 PointXXu/PointXXv/PointXXw 后缀
 * @code
 * ComponentName(0); // returns 'u'
 * ComponentName(2); // returns 'w'
 * @endcode
 */
inline char ComponentName(int index)
	{
		if (index == 0) return 'u';
		if (index == 1) return 'v';
		if (index == 2) return 'w';
		return static_cast<char>('a' + index);
	}

	/**
 * @brief 从 YAML 文档读取双精度浮点数，不存在或为空时返回默认值
 * @param yaml YML 对象
 * @param key 不带根键前缀的键名
 * @param defaultValue 键不存在或值为空时的默认返回值
 * @return 读取的双精度值
 * @note 通过 SimWindYamlKey 自动添加 "Qahse.SimWind." 前缀查找
 */
inline double ReadYamlDoubleOr(const YML &yaml, const std::string &key, double defaultValue)
	{
		const std::string value = yaml.read(SimWindYamlKey(key));
		return ZString::Trim(value).empty() ? defaultValue : YML::YmlToDouble(value);
	}
}

/**
 * @brief 从 .qwd 或 .yml 文件读取 SimWindInput 主输入数据
 * @param path 输入文件路径（.qwd 为文本关键字-值格式，.yml/.yaml 为 YAML 格式）
 * @return 解析得到的 SimWindInput 结构体，包含所有风场模拟参数
 * @note 自动根据扩展名选择解析器：YAML 路径使用 SimWindInputSerializer::ReadYamlFile，
 *       否则使用 ReadTextFile。读取完成后调用 ResolvePaths 将文件内部相对路径
 *       解析为绝对路径，并调用 ApplyDerivedFields 计算派生字段
 * @code
 * auto input = ReadSimWindInput("project.qwd");
 * @endcode
 */
inline SimWindInput ReadSimWindInput(const std::string &path)
{
	simwind_io_detail::RequireFile(path);

	simwind_io_detail::SimWindInputSerializer serializer;
	if (simwind_io_detail::IsYamlPath(path))
		serializer.ReadYamlFile(path);
	else
		serializer.ReadTextFile(path);

	simwind_io_detail::ResolvePaths(serializer.data, path);
	simwind_io_detail::ApplyDerivedFields(serializer.data);
	return serializer.data;
}

/**
 * @brief 将 SimWindInput 主输入数据写入文件
 * @param input 要写入的 SimWindInput 数据，包含所有风场计算参数
 * @param path 输出文件路径（支持 .qwd 文本格式和 .yml/.yaml YAML 格式）
 * @param templatePath 文本格式模板文件路径（可选，仅对非 YAML 输出有效）
 * @note YAML 输出直接写入；文本输出若提供有效模板则先读入模板再覆盖数据后写出，保留模板中的注释和其他行
 *       文本格式采用"值在前 关键字在后"的对齐风格，数据以关键字-值对组织
 * @code
 * WriteSimWindInput(input, "output.qwd", "template.qwd");
 * @endcode
 */
inline void WriteSimWindInput(const SimWindInput &input,
                            const std::string &path,
                            const std::string &templatePath = "")
{
	simwind_io_detail::SimWindInputSerializer serializer(input);
	if (simwind_io_detail::IsYamlPath(path))
	{
		serializer.WriteYamlFile(path);
		return;
	}

	if (!templatePath.empty() && ZFile::Exists(templatePath))
	{
		serializer.ReadTextFile(templatePath);
		serializer.data = input;
	}
	serializer.WriteTextFile(path);
}

/**
 * @brief 转换 SimWindInput 主输入文件格式
 * @param inputPath 输入文件路径（支持 .qwd 文本格式和 .yml/.yaml YAML 格式）
 * @param outputPath 输出文件路径
 * @param templatePath 文本输出模板路径（可选，仅对非 YAML 输出有效）
 * @note 内部调用 ReadSimWindInput → WriteSimWindInput，支持 .qwd ↔ .yml 互转
 * @code
 * ConvertSimWindInput("input.qwd", "output.yml");
 * @endcode
 */
inline void ConvertSimWindInput(const std::string &inputPath,
                              const std::string &outputPath,
                              const std::string &templatePath = "")
{
	WriteSimWindInput(ReadSimWindInput(inputPath), outputPath, templatePath);
}

/**
 * @brief 从文件读取用户自定义风切变数据
 * @param path 输入文件路径（支持 .dat 文本格式和 .yml/.yaml YAML 格式）
 * @return 解析得到的 UserShearData 结构体
 * @note 文本格式期望：NumUSRz/StdScale1-3 关键字行 → 列标题 → !Begin → 数据行（5列：Height, WindSpeed, WindDirection, StandardDeviation, LengthScale）
 *       YAML 格式读取 NumUSRz、StdScale1-3 标量及 Data 二维数组
 *       若 NumUSRz 为 0 则自动根据读取行数回填
 * @code
 * auto shear = ReadUserShear("shear.dat");
 * @endcode
 */
inline UserShearData ReadUserShear(const std::string &path)
{
	using namespace simwind_io_detail;
	if (IsYamlPath(path))
	{
		RequireFile(path);
		YML yaml(path, false);
		UserShearData data;
		data.numHeights = ReadYamlInt(yaml, "NumUSRz", 0);
		data.stdScale1 = ReadYamlDoubleOr(yaml, "StdScale1", data.stdScale1);
		data.stdScale2 = ReadYamlDoubleOr(yaml, "StdScale2", data.stdScale2);
		data.stdScale3 = ReadYamlDoubleOr(yaml, "StdScale3", data.stdScale3);
		LoadColumns(ReadYamlMatrix(yaml, "Data"),
		            data.heights,
		            data.windSpeeds,
		            data.windDirections,
		            data.standardDeviations,
		            data.lengthScales);
		if (data.numHeights == 0)
			data.numHeights = static_cast<int>(data.heights.size());
		return data;
	}

	RequireFile(path);
	const auto lines = ZFile::ReadAllLines(path);
	UserShearData data;
	data.numHeights = ReadKeywordValue<int>(lines, "NumUSRz", 0);
	data.stdScale1 = ReadKeywordValue<double>(lines, "StdScale1", data.stdScale1);
	data.stdScale2 = ReadKeywordValue<double>(lines, "StdScale2", data.stdScale2);
	data.stdScale3 = ReadKeywordValue<double>(lines, "StdScale3", data.stdScale3);
	LoadColumns(ReadRowsAfterBegin(lines, 5, static_cast<std::size_t>(std::max(data.numHeights, 0))),
	            data.heights,
	            data.windSpeeds,
	            data.windDirections,
	            data.standardDeviations,
	            data.lengthScales);
	if (data.numHeights == 0)
		data.numHeights = static_cast<int>(data.heights.size());
	return data;
}

/**
 * @brief 将用户风切变数据写入文件
 * @param data 用户风切变数据，包含高度、风速、风向、标准差、长度尺度和缩放因子
 * @param path 输出文件路径（支持 .dat 文本格式和 .yml/.yaml YAML 格式）
 * @note 文本格式输出：注释头 → NumUSRz/StdScale1-3 关键字行 → "Height WindSpeed WindDirection StandardDeviation LengthScale" 列标题 → !Begin → 5列数据行
 *       YAML 格式输出 NumUSRz、StdScale1-3 标量及 Data 矩阵节点（5列）
 * @code
 * WriteUserShear(data, "shear.dat");
 * @endcode
 */
inline void WriteUserShear(const UserShearData &data, const std::string &path)
{
	using namespace simwind_io_detail;
	const auto rows = ColumnsToRows(data.heights,
	                                data.windSpeeds,
	                                data.windDirections,
	                                data.standardDeviations,
	                                data.lengthScales);

	if (IsYamlPath(path))
	{
		YML yaml;
		AddYamlValue(yaml, "NumUSRz", data.numHeights > 0 ? data.numHeights : static_cast<int>(rows.size()));
		AddYamlValue(yaml, "StdScale1", data.stdScale1);
		AddYamlValue(yaml, "StdScale2", data.stdScale2);
		AddYamlValue(yaml, "StdScale3", data.stdScale3);
		AddYamlNode(yaml, "Data", YML::ToYmlValueString(rows, 2));
		yaml.save(path);
		return;
	}

	std::vector<std::string> lines;
	lines.reserve(rows.size() + 10);
	lines.push_back("-- Qahse.SimWind user shear table");
	lines.push_back(std::to_string(data.numHeights > 0 ? data.numHeights : static_cast<int>(rows.size())) +
	                " NumUSRz - Number of heights");
	lines.push_back(ZString::FormatDouble(data.stdScale1) + " StdScale1 - u standard deviation scale");
	lines.push_back(ZString::FormatDouble(data.stdScale2) + " StdScale2 - v standard deviation scale");
	lines.push_back(ZString::FormatDouble(data.stdScale3) + " StdScale3 - w standard deviation scale");
	lines.push_back("Height WindSpeed WindDirection StandardDeviation LengthScale");
	lines.push_back("!Begin");
	AddRows(lines, rows);
	ZFile::WriteAllLines(path, lines);
}

/** @brief 转换用户风切变数据文件格式，支持 .dat ↔ .yml 互转 */
inline void ConvertUserShear(const std::string &inputPath, const std::string &outputPath)
{
	WriteUserShear(ReadUserShear(inputPath), outputPath);
}

/**
 * @brief 从文件读取用户自定义功率谱数据
 * @param path 输入文件路径（支持 .dat 文本格式和 .yml/.yaml YAML 格式）
 * @return 解析得到的 UserSpectraData 结构体
 * @note 文本格式期望：NumUSRf/SpecScale1-3 关键字行 → 列标题 → !Begin → 数据行（4列：Frequency, uPSD, vPSD, wPSD）
 *       YAML 格式读取 NumUSRf、SpecScale1-3 标量及 Data 二维数组
 *       若 NumUSRf 为 0 则自动根据读取行数回填
 * @code
 * auto spectra = ReadUserSpectra("spectra.dat");
 * @endcode
 */
inline UserSpectraData ReadUserSpectra(const std::string &path)
{
	using namespace simwind_io_detail;
	if (IsYamlPath(path))
	{
		RequireFile(path);
		YML yaml(path, false);
		UserSpectraData data;
		data.numFrequencies = ReadYamlInt(yaml, "NumUSRf", 0);
		data.specScale1 = ReadYamlDoubleOr(yaml, "SpecScale1", data.specScale1);
		data.specScale2 = ReadYamlDoubleOr(yaml, "SpecScale2", data.specScale2);
		data.specScale3 = ReadYamlDoubleOr(yaml, "SpecScale3", data.specScale3);
		LoadColumns(ReadYamlMatrix(yaml, "Data"), data.frequencies, data.uPsd, data.vPsd, data.wPsd);
		if (data.numFrequencies == 0)
			data.numFrequencies = static_cast<int>(data.frequencies.size());
		return data;
	}

	RequireFile(path);
	const auto lines = ZFile::ReadAllLines(path);
	UserSpectraData data;
	data.numFrequencies = ReadKeywordValue<int>(lines, "NumUSRf", 0);
	data.specScale1 = ReadKeywordValue<double>(lines, "SpecScale1", data.specScale1);
	data.specScale2 = ReadKeywordValue<double>(lines, "SpecScale2", data.specScale2);
	data.specScale3 = ReadKeywordValue<double>(lines, "SpecScale3", data.specScale3);
	LoadColumns(ReadRowsAfterBegin(lines, 4, static_cast<std::size_t>(std::max(data.numFrequencies, 0))),
	            data.frequencies,
	            data.uPsd,
	            data.vPsd,
	            data.wPsd);
	if (data.numFrequencies == 0)
		data.numFrequencies = static_cast<int>(data.frequencies.size());
	return data;
}

/**
 * @brief 将用户功率谱数据写入文件
 * @param data 用户功率谱数据，包含 frequencies、uPsd、vPsd、wPsd 及缩放因子
 * @param path 输出文件路径（支持 .dat 文本格式和 .yml/.yaml YAML 格式）
 * @note 文本格式输出：注释头 → NumUSRf/SpecScale1-3 关键字行 → "Frequency uPSD vPSD wPSD" 列标题 → !Begin → 数据行
 *       YAML 格式输出 NumUSRf、SpecScale1-3 标量及 Data 矩阵节点（4列）
 * @code
 * WriteUserSpectra(data, "spectra.dat");
 * @endcode
 */
inline void WriteUserSpectra(const UserSpectraData &data, const std::string &path)
{
	using namespace simwind_io_detail;
	const auto rows = ColumnsToRows(data.frequencies, data.uPsd, data.vPsd, data.wPsd);

	if (IsYamlPath(path))
	{
		YML yaml;
		AddYamlValue(yaml, "NumUSRf", data.numFrequencies > 0 ? data.numFrequencies : static_cast<int>(rows.size()));
		AddYamlValue(yaml, "SpecScale1", data.specScale1);
		AddYamlValue(yaml, "SpecScale2", data.specScale2);
		AddYamlValue(yaml, "SpecScale3", data.specScale3);
		AddYamlNode(yaml, "Data", YML::ToYmlValueString(rows, 2));
		yaml.save(path);
		return;
	}

	std::vector<std::string> lines;
	lines.reserve(rows.size() + 5);
	lines.push_back("-- Qahse.SimWind user spectra table");
	lines.push_back(std::to_string(data.numFrequencies > 0 ? data.numFrequencies : static_cast<int>(rows.size())) +
	                " NumUSRf - Number of frequencies");
	lines.push_back(ZString::FormatDouble(data.specScale1) + " SpecScale1 - u spectrum scale");
	lines.push_back(ZString::FormatDouble(data.specScale2) + " SpecScale2 - v spectrum scale");
	lines.push_back(ZString::FormatDouble(data.specScale3) + " SpecScale3 - w spectrum scale");
	lines.push_back("Frequency uPSD vPSD wPSD");
	lines.push_back("!Begin");
	AddRows(lines, rows);
	ZFile::WriteAllLines(path, lines);
}

/** @brief 转换用户功率谱数据文件格式，支持 .dat ↔ .yml 互转 */
inline void ConvertUserSpectra(const std::string &inputPath, const std::string &outputPath)
{
	WriteUserSpectra(ReadUserSpectra(inputPath), outputPath);
}

/**
 * @brief 从文件读取用户自定义风速时间序列数据
 * @param path 输入文件路径（支持 .dat 文本格式和 .yml/.yaml YAML 格式）
 * @return 解析得到的 UserWindSpeedData 结构体，包含测点坐标、风速分量时间序列
 * @note 文本格式期望结构：
 *       - 关键字行：nComp、nPoints、RefPtID（值在前，关键字在后）
 *       - 测点坐标区："Pointyi Pointzi" 标题行 → 坐标数据行
 *       - 时间序列区："Time Series" 标记行 → 列标题 → !Begin → 数据行（首列时间，其后按点序/分量序排列）
 *       自动根据 nComp/nPoints 确定数据维度
 * @code
 * auto data = ReadUserWindSpeed("windspeed.dat");
 * @endcode
 */
inline UserWindSpeedData ReadUserWindSpeed(const std::string &path)
{
	using namespace simwind_io_detail;
	RequireFile(path);

	UserWindSpeedData data;
	std::vector<std::vector<double>> pointRows;
	std::vector<std::vector<double>> seriesRows;

	if (IsYamlPath(path))
	{
		YML yaml(path, false);
		data.nComp = ReadYamlInt(yaml, "nComp", 0);
		data.nPoints = ReadYamlInt(yaml, "nPoints", 0);
		data.refPtID = ReadYamlInt(yaml, "RefPtID", 0);
		pointRows = ReadYamlMatrix(yaml, "Points");
		seriesRows = ReadYamlMatrix(yaml, "TimeSeries");
	}
	else
	{
		const auto lines = ZFile::ReadAllLines(path);
		data.nComp = ReadKeywordValue<int>(lines, "nComp", 0);
		data.nPoints = ReadKeywordValue<int>(lines, "nPoints", 0);
		data.refPtID = ReadKeywordValue<int>(lines, "RefPtID", 0);
		pointRows = ReadPointRows(lines, data.nPoints);
		const int expectedCols = data.nPoints > 0 && data.nComp > 0 ? 1 + data.nPoints * data.nComp : 0;
		seriesRows = ReadRowsAfterBegin(lines, static_cast<std::size_t>(expectedCols));
	}

	for (const auto &row : pointRows)
	{
		if (row.size() < 2)
			continue;
		WindPoint point;
		point.y = row[0];
		point.z = row[1];
		data.points.push_back(point);
	}
	if (data.nPoints == 0)
		data.nPoints = static_cast<int>(data.points.size());

	data.components.resize(static_cast<std::size_t>(std::max(data.nPoints, 0)));
	for (auto &pointComponents : data.components)
		pointComponents.resize(static_cast<std::size_t>(std::max(data.nComp, 0)));

	const int expectedCols = data.nPoints > 0 && data.nComp > 0 ? 1 + data.nPoints * data.nComp : 0;
	for (const auto &row : seriesRows)
	{
		if (expectedCols != 0 && static_cast<int>(row.size()) < expectedCols)
			continue;

		data.time.push_back(row[0]);
		int col = 1;
		for (int p = 0; p < data.nPoints; ++p)
		{
			for (int c = 0; c < data.nComp; ++c)
			{
				data.components[static_cast<std::size_t>(p)][static_cast<std::size_t>(c)].push_back(row[static_cast<std::size_t>(col)]);
				++col;
			}
		}
	}

	return data;
}

/**
 * @brief 将用户风速时间序列数据写入文件
 * @param data 用户风速时间序列数据，包含 time、points、components 等成员
 * @param path 输出文件路径（支持 .dat 文本格式和 .yml/.yaml YAML 格式）
 * @note 文本格式输出结构：
 *       - 注释行（文件描述）
 *       - 关键字行：nComp、nPoints、RefPtID（值在前，关键字在后）
 *       - 测点坐标区：Pointyi Pointzi 列标题 + 单位行 + 坐标行
 *       - 时间序列头 + 列标题（Elapsed Time + PointXXu/v/w）+ !Begin + 数据行
 *       YAML 格式输出 nComp、nPoints、RefPtID 标量及 Points、TimeSeries 矩阵节点
 * @code
 * WriteUserWindSpeed(data, "windspeed.dat");
 * @endcode
 */
inline void WriteUserWindSpeed(const UserWindSpeedData &data, const std::string &path)
{
	using namespace simwind_io_detail;
	const int nComp = EffectiveComponentCount(data);
	const int nPoints = EffectivePointCount(data);
	const auto pointRows = PointRows(data);
	const auto seriesRows = WindSpeedRows(data);

	if (IsYamlPath(path))
	{
		YML yaml;
		AddYamlValue(yaml, "nComp", nComp);
		AddYamlValue(yaml, "nPoints", nPoints);
		AddYamlValue(yaml, "RefPtID", data.refPtID);
		AddYamlNode(yaml, "Points", YML::ToYmlValueString(pointRows, 2));
		AddYamlNode(yaml, "TimeSeries", YML::ToYmlValueString(seriesRows, 2));
		yaml.save(path);
		return;
	}

	std::vector<std::string> lines;
	lines.reserve(seriesRows.size() + pointRows.size() + 12);
	lines.push_back("-- Qahse.SimWind user wind speed table");
	lines.push_back(std::to_string(nComp) + " nComp - Number of velocity components");
	lines.push_back(std::to_string(nPoints) + " nPoints - Number of points");
	lines.push_back(std::to_string(data.refPtID) + " RefPtID - Reference point id");
	lines.push_back("Pointyi Pointzi");
	lines.push_back("(m) (m)");
	AddRows(lines, pointRows);
	lines.push_back("--------Time Series------------------------------------------------------------");

	std::ostringstream header;
	header << "Elapsed Time";
	for (int p = 0; p < nPoints; ++p)
	{
		for (int c = 0; c < nComp; ++c)
		{
			header << " Point" << std::setw(2) << std::setfill('0') << (p + 1) << ComponentName(c);
			header << std::setfill(' ');
		}
	}
	lines.push_back(header.str());
	lines.push_back("!Begin");
	AddRows(lines, seriesRows);
	ZFile::WriteAllLines(path, lines);
}

/**
 * @brief 转换用户风速时间序列数据文件格式
 * @param inputPath 输入文件路径（支持 .dat 文本格式和 .yml/.yaml YAML 格式）
 * @param outputPath 输出文件路径
 * @note 内部调用 ReadUserWindSpeed → WriteUserWindSpeed，支持 .dat ↔ .yml 互转
 * @code
 * ConvertUserWindSpeed("windspeed.dat", "windspeed.yml");
 * @endcode
 */
inline void ConvertUserWindSpeed(const std::string &inputPath, const std::string &outputPath)
{
	WriteUserWindSpeed(ReadUserWindSpeed(inputPath), outputPath);
}
