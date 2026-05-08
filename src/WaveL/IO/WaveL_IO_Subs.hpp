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
// 该文件提供 WaveL 模块的 IO 子系统，包括 YAML 序列化辅助函数、WaveLInput 验证、
// WVC 文件解析和输出路径推导。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../WaveKinematics.hpp"
#include "../WaveL_Type.hpp"
#include "../../IO/LocaleString.hpp"
#include "../../IO/Serializer.hpp"
#include "../../IO/Yaml.hpp"
#include "../../IO/ZFile.hpp"
#include "../../IO/ZPath.hpp"
#include "../../IO/ZString.hpp"
#include "../../Params.h"
#include "LocaleString_WaveL.hpp"

/**
 * @brief 波浪成分文件 (.wvc) 的解析结果数据结构。
 *        Parse result data structure for wave component files (.wvc).
 *
 * 包含从成分文件头解析的统计量（Hs, Tp, fp, m0, waterDepth）
 * 以及完整的波浪成分列表。
 *
 * Contains statistics parsed from the component file header (Hs, Tp, fp, m0, waterDepth)
 * and the complete wave component list.
 */
struct WaveComponentFileData
{
	double significantHeight = 0.0;        ///< 有效波高 Hs [m]。Significant wave height [m].
	double peakPeriod = 0.0;               ///< 谱峰周期 Tp [s]。Peak spectral period [s].
	double peakFrequency = 0.0;            ///< 谱峰频率 fp [Hz]。Peak spectral frequency [Hz].
	double zeroMoment = 0.0;               ///< 零阶谱矩 m0 [m²]。Zero-order spectral moment [m²].
	double waterDepth = 0.0;               ///< 水深 [m]。Water depth [m].
	std::vector<WaveTrain> waveTrains;     ///< 波浪成分列表。Wave component list.
};

/**
 * @brief 波浪缓存文件 (.wfc) 的解析结果数据结构。
 *        Parse result data structure for wave cache files (.wfc).
 *
 * 包含完整的四维运动学缓存和频谱结果统计量。
 *
 * Contains the complete 4D kinematics cache and spectrum result statistics.
 */
struct WaveCacheFileData
{
	wavel_detail::WaveFieldCache cache;    ///< 运动学缓存。Kinematics cache.
	WaveSpectrumResult result;             ///< 频谱统计结果。Spectrum statistics result.
	double waterDepth = 0.0;               ///< 水深 [m]。Water depth [m].
};

/**
 * @brief WaveL I/O 内部辅助命名空间，包含序列化器、路径处理和文件解析器。
 *        WaveL I/O internal helper namespace, containing serializer, path handling, and file parsers.
 */
namespace wavel_io_detail
{
	/** @brief YAML 根键名称。YAML root key name. */
	inline constexpr const char *kYamlRoot = "Qahse.WaveL";

	/** @brief π 常量（高精度）。Pi constant (high precision). */
	inline constexpr double kPi = 3.14159265358979323846;

	/**
	 * @brief 构造带根前缀的 YAML 键名。
	 *        Build a YAML key name with the root prefix.
	 * @param key 相对键名。Relative key name.
	 * @return    完整 YAML 路径 "Qahse.WaveL.key"。Full YAML path "Qahse.WaveL.key".
	 */
	inline std::string WaveLYamlKey(const std::string &key)
	{
		return std::string(kYamlRoot) + "." + key;
	}

	/**
	 * @brief 判断文件路径是否为 YAML 格式（.yaml 或 .yml 扩展名）。
	 *        Check if a file path is in YAML format (.yaml or .yml extension).
	 * @param path 文件路径。File path.
	 * @return     true 表示扩展名为 .yaml 或 .yml。true if extension is .yaml or .yml.
	 */
	inline bool IsYamlPath(const std::string &path)
	{
		const std::string ext = ZString::ToUpper(std::filesystem::path(path).extension().string());
		return ext == ".YAML" || ext == ".YML";
	}

	/**
	 * @brief 根据序列化格式返回正确的键名（YAML 加前缀，文本不加）。
	 *        Return the correct key name based on serialization format (YAML adds prefix, text does not).
	 */
	inline std::string KeyForFormat(Serializer::Format format, const std::string &key)
	{
		return format == Serializer::Format::Yaml ? WaveLYamlKey(key) : key;
	}

	/**
	 * @brief 序列化字段引用模板，将键名字面量与值引用绑定。
	 *        Serialization field reference template, binding a key literal to a value reference.
	 * @tparam T 字段值类型。Field value type.
	 */
	template <typename T>
	struct FieldRef
	{
		const char *key;   ///< 字段键名。Field key name.
		T &value;          ///< 字段值引用。Reference to field value.
	};

	/**
	 * @brief 创建字段引用。
	 *        Create a field reference.
	 * @tparam T 字段值类型（自动推导）。Field value type (auto-deduced).
	 * @param key   键名字面量。Key name literal.
	 * @param value 值引用。Value reference.
	 * @return      FieldRef<T> 实例。FieldRef<T> instance.
	 */
	template <typename T>
	FieldRef<T> Field(const char *key, T &value)
	{
		return {key, value};
	}

	/**
	 * @brief 若 path 为非空，将其解析为基于 baseFilePath 的相对/绝对路径。
	 *        If path is non-empty, resolve it as a relative/absolute path based on baseFilePath.
	 *
	 * 使用 ZPath::ResolvePath 进行路径解析，处理相对路径与绝对路径的组合。
	 *
	 * Uses ZPath::ResolvePath for resolution, handling relative and absolute path combinations.
	 */
	inline void ResolveIfSet(const std::string &baseFilePath, std::string &path)
	{
		if (!path.empty())
			path = ZPath::ResolvePath(baseFilePath, path);
	}

	/**
	 * @brief 若路径非空，确保其父目录存在（必要时递归创建）。
	 *        If the path is non-empty, ensure its parent directory exists (creating recursively if needed).
	 */
	inline void CreateParentIfNeeded(const std::string &path)
	{
		if (path.empty())
			return;
		const auto parent = std::filesystem::path(path).parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent);
	}

	/**
	 * @brief 对输入参数应用派生字段（当前为空实现，预留扩展接口）。
	 *        Apply derived fields to input parameters (currently empty, reserved for extension).
	 */
	inline void ApplyDerivedFields(WaveLInput &input)
	{
	}

	/**
	 * @brief 解析输入中的所有路径字段，相对于 baseFilePath 进行路径解析并创建父目录。
	 *        Resolve all path fields in the input relative to baseFilePath, and create parent directories.
	 *
	 * 处理的路径字段包括：importedSpectrumPath、importedTimeSeriesPath、importedComponentsPath、
	 * importedCachePath、cachePath、metadataPath、componentsPath、timeSeriesPath、kinematicsPath、
	 * summaryPath。对每个输出路径调用 CreateParentIfNeeded 确保父目录存在。
	 *
	 * Processed path fields include all import and output paths. Calls CreateParentIfNeeded
	 * on each output path to ensure parent directories exist.
	 */
	inline void ResolvePaths(WaveLInput &input, const std::string &baseFilePath)
	{
		ResolveIfSet(baseFilePath, input.importedSpectrumPath);
		ResolveIfSet(baseFilePath, input.importedTimeSeriesPath);
		ResolveIfSet(baseFilePath, input.importedComponentsPath);
		ResolveIfSet(baseFilePath, input.importedCachePath);
		ResolveIfSet(baseFilePath, input.cachePath);
		ResolveIfSet(baseFilePath, input.metadataPath);
		ResolveIfSet(baseFilePath, input.componentsPath);
		ResolveIfSet(baseFilePath, input.timeSeriesPath);
		ResolveIfSet(baseFilePath, input.kinematicsPath);
		ResolveIfSet(baseFilePath, input.summaryPath);

		CreateParentIfNeeded(input.cachePath);
		CreateParentIfNeeded(input.metadataPath);
		CreateParentIfNeeded(input.componentsPath);
		CreateParentIfNeeded(input.timeSeriesPath);
		CreateParentIfNeeded(input.summaryPath);
		if (!input.kinematicsPath.empty())
			std::filesystem::create_directories(input.kinematicsPath);
	}

	/**
	 * @brief 从数据文件中读取有效行（跳过空行、注释和区块标记）。
	 *        Read valid data lines from a data file (skipping blank lines, comments, and block markers).
	 *
	 * 解析规则：
	 * - 遇到 "!BEGIN" 后开始收集数据行
	 * - 跳过以 '#' 开头或以 "//" 开头的注释行
	 * - 跳过空行
	 *
	 * Parse rules:
	 * - Start collecting after encountering "!BEGIN"
	 * - Skip comment lines starting with '#' or "//"
	 * - Skip blank lines
	 *
	 * @param path 文件路径。File path.
	 * @return     过滤后的有效行文本列表。Filtered list of valid line texts.
	 */
	inline std::vector<std::string> ReadDataLines(const std::string &path)
	{
		const auto lines = ZFile::ReadAllLines(path);
		std::vector<std::string> filtered;
		filtered.reserve(lines.size());
		bool inBlock = false;
		for (const std::string &raw : lines)
		{
			std::string line = ZString::Trim(raw);
			if (line.empty())
				continue;
			const std::string upper = ZString::ToUpper(std::string(line));
			if (upper == "!BEGIN")
			{
				inBlock = true;
				continue;
			}
			if (!inBlock)
				continue;
			if (line[0] == '#' || line.rfind("//", 0) == 0)
				continue;
			filtered.push_back(line);
		}
		return filtered;
	}

	/**
	 * @brief 将空白分隔的数值字符串解析为 double 向量。
	 *        Parse a whitespace-delimited numeric string into a vector of doubles.
	 * @param line 数值文本行（如 "1.0 2.5 3.7"）。Numeric text line (e.g., "1.0 2.5 3.7").
	 * @return     double 值向量。Vector of double values.
	 */
	inline std::vector<double> ParseDoubles(const std::string &line)
	{
		std::vector<double> values;
		std::istringstream stream(line);
		double value = 0.0;
		while (stream >> value)
			values.push_back(value);
		return values;
	}

	/**
	 * @brief WaveLInput 序列化器，负责 WaveLInput 结构体的读写（文本或 YAML 格式）。
	 *        WaveLInput serializer, responsible for reading/writing WaveLInput structs (text or YAML format).
	 *
	 * 继承自 Serializer 基类，通过 Fields() 折叠表达式批量注册所有字段的序列化逻辑。
	 * 支持从 .qoe（文本）或 .yaml/.yml（YAML）文件读写。
	 *
	 * Inherits from Serializer base class; uses Fields() fold expression to batch-register
	 * serialization logic for all fields. Supports read/write from .qoe (text) or .yaml/.yml (YAML) files.
	 */
	class WaveLInputSerializer : public Serializer
	{
	public:
		WaveLInput data;  ///< 序列化的数据载体。Serialized data carrier.

		WaveLInputSerializer()
		{
			SetValueFirst(true);
		}

		explicit WaveLInputSerializer(const WaveLInput &input)
			: data(input)
		{
			SetValueFirst(true);
		}

	protected:
		/**
		 * @brief 注册所有 WaveLInput 字段的序列化处理。
		 *        Register serialization handling for all WaveLInput fields.
		 *
		 * 通过 C++17 折叠表达式 (One(fields), ...) 将每个字段绑定到其序列化键名，
		 * 键名在 YAML 格式下自动添加 "Qahse.WaveL." 前缀。
		 *
		 * Uses C++17 fold expression (One(fields), ...) to bind each field to its serialization key.
		 * Keys automatically get "Qahse.WaveL." prefix in YAML format.
		 */
		void SerializeFields() override
		{
			Fields(
				Field("Mode", data.mode),
				Field("FreqSpectrum", data.freqSpectrum),
				Field("DirSpectrum", data.dirSpectrum),
				Field("Discretization", data.discretization),
				Field("Stretching", data.stretching),
				Field("Hs", data.Hs),
				Field("Tp", data.Tp),
				Field("TimeOffset", data.timeOffset),
				Field("DirMean", data.dirMean),
				Field("DirMax", data.dirMax),
				Field("DirSpreadExp", data.dirSpreadExp),
				Field("FCutIn", data.fCutIn),
				Field("FCutOut", data.fCutOut),
				Field("DfMax", data.dfMax),
				Field("NumFreqBins", data.numFreqBins),
				Field("NumDirBins", data.numDirBins),
				Field("RandomSeed", data.randomSeed),
				Field("Gamma", data.gamma),
				Field("Sigma1", data.sigma1),
				Field("Sigma2", data.sigma2),
				Field("AutoGamma", data.autoGamma),
				Field("AutoSigma", data.autoSigma),
				Field("AutoFreqRange", data.autoFreqRange),
				Field("Hs1", data.Hs1),
				Field("Hs2", data.Hs2),
				Field("F1", data.f1),
				Field("F2", data.f2),
				Field("Lambda1", data.lambda1),
				Field("Lambda2", data.lambda2),
				Field("AutoOchi", data.autoOchi),
				Field("DoublePeak", data.doublePeak),
				Field("RegularPhase", data.regularPhase),
				Field("TimeStep", data.timeStep),
				Field("SimDuration", data.simDuration),
				Field("WaterDepth", data.waterDepth),
				Field("MCFDiameter", data.mcfDiameter),
				Field("OutputComponents", data.outputComponents),
				Field("OutputTimeSeries", data.outputTimeSeries),
				Field("OutputKinematicsGrid", data.outputKinematicsGrid),
				Field("CachePath", data.cachePath),
				Field("MetadataPath", data.metadataPath),
				Field("ComponentsPath", data.componentsPath),
				Field("TimeSeriesPath", data.timeSeriesPath),
				Field("KinematicsPath", data.kinematicsPath),
				Field("SummaryPath", data.summaryPath),
				Field("GridNX", data.gridNX),
				Field("GridNY", data.gridNY),
				Field("GridNZ", data.gridNZ),
				Field("GridDX", data.gridDX),
				Field("GridDY", data.gridDY),
				Field("GridDZ", data.gridDZ),
				Field("ImportedSpectrumPath", data.importedSpectrumPath),
				Field("ImportedTimeSeriesPath", data.importedTimeSeriesPath),
				Field("ImportedComponentsPath", data.importedComponentsPath),
				Field("ImportedCachePath", data.importedCachePath));
		}

	private:
		/**
		 * @brief 对单个字段执行读写操作。
		 *        Perform read/write operation on a single field.
		 * @note 根据序列化格式自动选择 YAML 键名或纯文本键名。
		 *       Auto-selects YAML key or plain text key based on serialization format.
		 */
		template <typename T>
		void One(const FieldRef<T> &field)
		{
			ReadOrWrite(KeyForFormat(GetFormat(), field.key), field.value);
		}

		/**
		 * @brief C++17 折叠表达式：批量处理多个字段的序列化。
		 *        C++17 fold expression: batch-process serialization of multiple fields.
		 * @param fields 可变数量的 FieldRef 参数。Variadic number of FieldRef args.
		 */
		template <typename... Fs>
		void Fields(const Fs &...fields)
		{
			(One(fields), ...);
		}
	};
}

/**
 * @brief 从 .qoe 或 .yaml 文件读取 WaveLInput。
 *        Read WaveLInput from a .qoe or .yaml file.
 *
 * 自动检测文件格式（按扩展名），使用 WaveLInputSerializer 进行反序列化，
 * 然后调用 ApplyDerivedFields 和 ResolvePaths 处理派生字段和路径。
 *
 * Auto-detects file format (by extension), uses WaveLInputSerializer for deserialization,
 * then calls ApplyDerivedFields and ResolvePaths for derived fields and path resolution.
 *
 * @param path 输入文件路径（.qoe 或 .yaml/.yml）。Input file path (.qoe or .yaml/.yml).
 * @return     反序列化后的 WaveLInput 结构体。Deserialized WaveLInput struct.
 */
inline WaveLInput ReadWaveLInput(const std::string &path)
{
	wavel_io_detail::WaveLInputSerializer serializer;
	if (wavel_io_detail::IsYamlPath(path))
		serializer.ReadYamlFile(path);
	else
		serializer.ReadTextFile(path);
	wavel_io_detail::ApplyDerivedFields(serializer.data);
	wavel_io_detail::ResolvePaths(serializer.data, path);
	return serializer.data;
}

/**
 * @brief 将 WaveLInput 写入 .qoe 或 .yaml 文件。
 *        Write WaveLInput to a .qoe or .yaml file.
 *
 * 自动根据扩展名选择写入格式。
 *
 * Auto-selects write format by extension.
 *
 * @param path  输出文件路径。Output file path.
 * @param input 要写入的 WaveLInput 数据。WaveLInput data to write.
 */
inline void WriteWaveLInput(const std::string &path, const WaveLInput &input)
{
	wavel_io_detail::WaveLInputSerializer serializer(input);
	if (wavel_io_detail::IsYamlPath(path))
		serializer.WriteYamlFile(path);
	else
		serializer.WriteTextFile(path);
}

/**
 * @brief 从用户频谱文件读取 (频率, 谱密度) 数据对。
 *        Read (frequency, spectral density) data pairs from a user spectrum file.
 *
 * 文件格式：每行包含频率和谱密度两个数值（空白分隔），
 * 需至少 2 行有效数据。跳过注释行（# 或 //）。
 *
 * File format: each line contains frequency and spectral density (whitespace-delimited),
 * at least 2 valid data lines required. Skips comments (# or //).
 *
 * @param path 文件路径。File path.
 * @return     包含频率和谱密度向量的 WaveUserSpectrumData。
 *             WaveUserSpectrumData with frequency and spectral density vectors.
 * @throw std::runtime_error 当数据不足或格式无效时抛出。Thrown when data is insufficient or format invalid.
 */
inline WaveUserSpectrumData ReadWaveUserSpectrumData(const std::string &path)
{
	WaveUserSpectrumData data;
	for (const auto &line : wavel_io_detail::ReadDataLines(path))
	{
		const auto values = wavel_io_detail::ParseDoubles(line);
		if (values.size() < 2)
			continue;
		data.frequencies.push_back(values[0]);
		data.spectralDensity.push_back(values[1]);
	}
	if (data.frequencies.size() < 2 || data.frequencies.size() != data.spectralDensity.size())
		throw std::runtime_error(std::string(L_WAVEL_InvalidSpectrumFile) + ": " + path);
	return data;
}

/**
 * @brief 从用户时间序列文件读取 (时间, 高程) 数据对。
 *        Read (time, elevation) data pairs from a user time series file.
 *
 * 文件格式与用户频谱文件相同：每行两个数值（时间 高程），空白分隔。
 *
 * File format same as user spectrum: two values per line (time, elevation), whitespace-delimited.
 *
 * @param path 文件路径。File path.
 * @return     包含时间和自由表面高程序列的 WaveTimeSeriesData。
 *             WaveTimeSeriesData with time and free surface elevation sequences.
 * @throw std::runtime_error 当数据不足或格式无效时抛出。Thrown when data is insufficient or format invalid.
 */
inline WaveTimeSeriesData ReadWaveTimeSeriesData(const std::string &path)
{
	WaveTimeSeriesData data;
	for (const auto &line : wavel_io_detail::ReadDataLines(path))
	{
		const auto values = wavel_io_detail::ParseDoubles(line);
		if (values.size() < 2)
			continue;
		data.times.push_back(values[0]);
		data.elevations.push_back(values[1]);
	}
	if (data.times.size() < 2 || data.times.size() != data.elevations.size())
		throw std::runtime_error(std::string(L_WAVEL_InvalidSeriesFile) + ": " + path);
	return data;
}

/**
 * @brief 写入波浪成分文件 (.wvc)。
 *        Write wave component file (.wvc).
 *
 * 文件格式：
 * - 头部行：以 # 开头的注释信息和统计量（Hs, Tp, fp, m0, WaterDepth, NumComponents）
 * - !Begin 标记后为数据行：每行包含 (frequency_Hz, amplitude_m, phase_rad, direction_rad, wavenumber_1pm)
 *
 * File format:
 * - Header: comment lines prefixed with # and statistics (Hs, Tp, fp, m0, WaterDepth, NumComponents)
 * - After !Begin marker: data lines with (frequency_Hz, amplitude_m, phase_rad, direction_rad, wavenumber_1pm)
 *
 * @param path   输出文件路径。Output file path.
 * @param input  波浪输入参数（取 waterDepth）。Wave input parameters (for waterDepth).
 * @param result 频谱结果（取波浪成分和统计量）。Spectrum result (for wave components and statistics).
 */
inline void WriteWaveComponentFile(const std::string &path,
	const WaveLInput &input,
	const WaveSpectrumResult &result)
{
	std::vector<std::string> lines;
	lines.reserve(result.waveTrains.size() + 16);
	lines.push_back("# Qahse.WaveL wave components");
	lines.push_back(std::to_string(result.significantHeight) + "\tHs");
	lines.push_back(std::to_string(result.peakPeriod) + "\tTp");
	lines.push_back(std::to_string(result.peakFrequency) + "\tFp");
	lines.push_back(std::to_string(result.zeroMoment) + "\tm0");
	lines.push_back(std::to_string(input.waterDepth) + "\tWaterDepth");
	lines.push_back(std::to_string(result.numComponents) + "\tNumComponents");
	lines.push_back("!Begin");
	lines.push_back("# frequency_Hz amplitude_m phase_rad direction_rad wavenumber_1pm");
	for (const auto &train : result.waveTrains)
	{
		const double frequency = train.omega / (2.0 * wavel_io_detail::kPi);
		lines.push_back(
			std::to_string(frequency) + "\t" +
			std::to_string(train.amplitude) + "\t" +
			std::to_string(train.phase) + "\t" +
			std::to_string(train.direction) + "\t" +
			std::to_string(train.wavenumber));
	}
	ZFile::WriteAllLines(path, lines);
}

/**
 * @brief 从波浪成分文件 (.wvc) 读取数据。
 *        Read data from a wave component file (.wvc).
 *
 * 解析文件头部统计量（Hs, Tp, Fp, m0, WaterDepth）和 !Begin 后的波浪成分数据行，
 * 每行解析为 WaveTrain（含自动计算 omega、cosDir、sinDir、A_omega、A_omega2）。
 *
 * Parses file header statistics (Hs, Tp, Fp, m0, WaterDepth) and wave component data lines
 * after !Begin. Each line is parsed into a WaveTrain (with auto-calculated omega, cosDir,
 * sinDir, A_omega, A_omega2).
 *
 * @param path 成分文件路径。Component file path.
 * @return     解析结果 WaveComponentFileData。Parsed WaveComponentFileData.
 * @throw std::runtime_error 当无有效波浪成分时抛出。Thrown when no valid wave components found.
 */
inline WaveComponentFileData ReadWaveComponentFile(const std::string &path)
{
	WaveComponentFileData data;
	const auto lines = ZFile::ReadAllLines(path);
	bool inBlock = false;
	for (const auto &raw : lines)
	{
		std::string line = ZString::Trim(raw);
		if (line.empty())
			continue;
		const std::string upper = ZString::ToUpper(std::string(line));
		if (upper == "!BEGIN")
		{
			inBlock = true;
			continue;
		}
		if (!inBlock)
		{
			if (line[0] == '#')
				continue;
			const auto parts = ZString::Split(line, '\t', true);
			if (parts.size() < 2)
				continue;
			const std::string key = ZString::ToUpper(parts[1]);
			const double value = ZString::StringTo<double>(parts[0]);
			if (key == "HS")
				data.significantHeight = value;
			else if (key == "TP")
				data.peakPeriod = value;
			else if (key == "FP")
				data.peakFrequency = value;
			else if (key == "M0")
				data.zeroMoment = value;
			else if (key == "WATERDEPTH")
				data.waterDepth = value;
			continue;
		}
		if (line[0] == '#')
			continue;
		const auto values = wavel_io_detail::ParseDoubles(line);
		if (values.size() < 5)
			continue;
		WaveTrain train;
		const double frequency = values[0];
		train.amplitude = values[1];
		train.phase = values[2];
		train.direction = values[3];
		train.wavenumber = values[4];
		train.omega = 2.0 * wavel_io_detail::kPi * frequency;
		train.cosDir = std::cos(train.direction);
		train.sinDir = std::sin(train.direction);
		train.A_omega = train.amplitude * train.omega;
		train.A_omega2 = train.A_omega * train.omega;
		data.waveTrains.push_back(train);
	}
	if (data.waveTrains.empty())
		throw std::runtime_error(std::string(L_WAVEL_InvalidComponentFile) + ": " + path);
	return data;
}

/**
 * @brief 写入自由表面时间序列文件 (.wts)。
 *        Write free-surface time series file (.wts).
 *
 * 文件格式：
 * - 头部：TimeStep, SimDuration, NumSamples
 * - !Begin 后为 (time_s, eta_m) 数据对，每行一对
 *
 * File format:
 * - Header: TimeStep, SimDuration, NumSamples
 * - After !Begin: (time_s, eta_m) data pairs, one pair per line
 *
 * @param path       输出文件路径。Output file path.
 * @param timeStep   时间步长 [s]。Time step [s].
 * @param times      时间向量 [s]。Time vector [s].
 * @param elevations 高程向量 [m]。Elevation vector [m].
 * @throw std::runtime_error 当时间与高程尺寸不匹配时抛出。Thrown when time and elevation sizes mismatch.
 */
inline void WriteWaveTimeSeriesFile(const std::string &path,
	double timeStep,
	const std::vector<double> &times,
	const std::vector<double> &elevations)
{
	if (times.size() != elevations.size())
		throw std::runtime_error(L_WAVEL_InvalidSeriesFile);
	std::vector<std::string> lines;
	lines.reserve(times.size() + 8);
	lines.push_back("# Qahse.WaveL free-surface time series");
	lines.push_back(std::to_string(timeStep) + "\tTimeStep");
	lines.push_back(std::to_string(times.empty() ? 0.0 : times.back()) + "\tSimDuration");
	lines.push_back(std::to_string(times.size()) + "\tNumSamples");
	lines.push_back("!Begin");
	lines.push_back("# time_s eta_m");
	for (std::size_t i = 0; i < times.size(); ++i)
		lines.push_back(std::to_string(times[i]) + "\t" + std::to_string(elevations[i]));
	ZFile::WriteAllLines(path, lines);
}

/**
 * @brief 写入二进制波浪缓存文件 (.wfc)。
 *        Write binary wave cache file (.wfc).
 *
 * 文件结构：
 * - Magic: "QWFC0001" (8 字节文件标识)
 * - Header: version(32), flags(32), nx(32), ny(32), nz(32), nt(32), dx(64), dy(64), dz(64), dt(64), zBottom(64)
 * - Statistics: waterDepth, Hs, Tp, fp, m0, fMin, fMax, spectralArea, reserved, numComponents
 * - Fields: eta, u, v, w, ax, ay, az, dynP（各 nx×ny×nz×nt 个 double）
 *
 * File structure:
 * - Magic: "QWFC0001" (8-byte file identifier)
 * - Header: various int32/double fields
 * - Statistics: waterDepth through numComponents
 * - Fields: eta, u, v, w, ax, ay, az, dynP (each nx×ny×nz×nt doubles)
 *
 * @param path   输出文件路径。Output file path.
 * @param input  输入参数（取 waterDepth）。Input parameters (for waterDepth).
 * @param result 频谱结果（取统计量）。Spectrum result (for statistics).
 * @param cache  运动学缓存（取网格参数和场量）。Kinematics cache (for grid params and fields).
 */
inline void WriteWaveCacheFile(const std::string &path,
	const WaveLInput &input,
	const WaveSpectrumResult &result,
	const wavel_detail::WaveFieldCache &cache)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open())
		throw std::runtime_error(std::string(L_WAVEL_CannotOpenOutput) + ": " + path);

	const char magic[8] = {'Q', 'W', 'F', 'C', '0', '0', '0', '1'};
	stream.write(magic, sizeof(magic));

	auto writePod = [&](const auto &value) {
		stream.write(reinterpret_cast<const char *>(&value), sizeof(value));
	};
	auto writeVector = [&](const std::vector<double> &values) {
		if (!values.empty())
			stream.write(reinterpret_cast<const char *>(values.data()), static_cast<std::streamsize>(sizeof(double) * values.size()));
	};

	const std::uint32_t version = 1;
	const std::uint32_t flags = 0u;
	const std::int32_t nx = cache.nx;
	const std::int32_t ny = cache.ny;
	const std::int32_t nz = cache.nz;
	const std::int32_t nt = cache.nt;
	const std::int32_t numComponents = result.numComponents;

	writePod(version);
	writePod(flags);
	writePod(nx);
	writePod(ny);
	writePod(nz);
	writePod(nt);
	writePod(cache.dx);
	writePod(cache.dy);
	writePod(cache.dz);
	writePod(cache.dt);
	writePod(cache.zBottom);
	writePod(input.waterDepth);
	writePod(result.significantHeight);
	writePod(result.peakPeriod);
	writePod(result.peakFrequency);
	writePod(result.zeroMoment);
	writePod(result.fMin);
	writePod(result.fMax);
	writePod(result.spectralArea);
	writePod(std::int32_t{1});
	writePod(numComponents);
	writeVector(cache.eta);
	writeVector(cache.u);
	writeVector(cache.v);
	writeVector(cache.w);
	writeVector(cache.ax);
	writeVector(cache.ay);
	writeVector(cache.az);
	writeVector(cache.dynP);

	if (!stream.good())
		throw std::runtime_error("Failed to write WaveL cache file: " + path);
}

/**
 * @brief 从二进制波浪缓存文件 (.wfc) 读取数据。
 *        Read data from a binary wave cache file (.wfc).
 *
 * 按 WriteWaveCacheFile 的格式逆序读取，验证 Magic 标识和网格维度合法性后分配存储。
 *
 * Reads in reverse order of WriteWaveCacheFile; validates Magic identifier and grid dimension
 * legality before allocating storage.
 *
 * @param path 缓存文件路径。Cache file path.
 * @return     解析结果 WaveCacheFileData。Parsed WaveCacheFileData.
 * @throw std::runtime_error 当 Magic 不匹配或缺维非法时抛出。Thrown when Magic mismatch or invalid dimensions.
 */
inline WaveCacheFileData ReadWaveCacheFile(const std::string &path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream.is_open())
		throw std::runtime_error(std::string(L_WAVEL_CannotOpenOutput) + ": " + path);

	char magic[8] = {};
	stream.read(magic, sizeof(magic));
	if (std::string(magic, sizeof(magic)) != std::string("QWFC0001", 8))
		throw std::runtime_error("Invalid WaveL cache file header: " + path);

	auto readPod = [&](auto &value) {
		stream.read(reinterpret_cast<char *>(&value), sizeof(value));
	};
	auto readVector = [&](std::vector<double> &values) {
		if (!values.empty())
			stream.read(reinterpret_cast<char *>(values.data()), static_cast<std::streamsize>(sizeof(double) * values.size()));
	};

	WaveCacheFileData data;
	std::uint32_t version = 0;
	std::uint32_t flags = 0;
	std::int32_t reservedWaveOrder = 0;
	readPod(version);
	readPod(flags);
	readPod(data.cache.nx);
	readPod(data.cache.ny);
	readPod(data.cache.nz);
	readPod(data.cache.nt);
	readPod(data.cache.dx);
	readPod(data.cache.dy);
	readPod(data.cache.dz);
	readPod(data.cache.dt);
	readPod(data.cache.zBottom);
	readPod(data.waterDepth);
	readPod(data.result.significantHeight);
	readPod(data.result.peakPeriod);
	readPod(data.result.peakFrequency);
	readPod(data.result.zeroMoment);
	readPod(data.result.fMin);
	readPod(data.result.fMax);
	readPod(data.result.spectralArea);
	readPod(reservedWaveOrder);
	readPod(data.result.numComponents);
	if (data.cache.nx <= 0 || data.cache.ny <= 0 || data.cache.nz <= 0 || data.cache.nt <= 0)
		throw std::runtime_error("Invalid WaveL cache dimensions: " + path);
	data.cache.Resize(data.cache.nx, data.cache.ny, data.cache.nz, data.cache.nt);
	readVector(data.cache.eta);
	readVector(data.cache.u);
	readVector(data.cache.v);
	readVector(data.cache.w);
	readVector(data.cache.ax);
	readVector(data.cache.ay);
	readVector(data.cache.az);
	readVector(data.cache.dynP);

	if (!stream.good())
		throw std::runtime_error("Failed to read WaveL cache file: " + path);
	return data;
}

/**
 * @brief 写入波浪摘要文件 (.sum)。
 *        Write wave summary file (.sum).
 *
 * 输出人类可读的文本摘要，包含模式标签、有效波高、谱峰周期/频率、水深、
 * 成分数、频率范围、谱面积、m0、文件路径等信息，以及警告列表。
 *
 * Outputs a human-readable text summary including mode label, Hs, Tp, fp, water depth,
 * component count, frequency range, spectral area, m0, file paths, and warning list.
 *
 * @param path      输出文件路径。Output file path.
 * @param input     输入参数（取 waterDepth）。Input parameters (for waterDepth).
 * @param result    频谱结果。Spectrum result.
 * @param modeLabel 模式标签（"GENERATE" 或 "IMPORT"）。Mode label ("GENERATE" or "IMPORT").
 */
inline void WriteWaveSummaryFile(const std::string &path,
	const WaveLInput &input,
	const WaveSpectrumResult &result,
	const std::string &modeLabel)
{
	std::ostringstream stream;
	stream.setf(std::ios::fixed);
	stream.precision(6);
	stream << "Qahse WaveL Summary\n";
	stream << "Mode: " << modeLabel << "\n";
	stream << "Significant wave height Hs: " << result.significantHeight << " m\n";
	stream << "Peak period Tp: " << result.peakPeriod << " s\n";
	stream << "Peak frequency fp: " << result.peakFrequency << " Hz\n";
	stream << "Water depth: " << input.waterDepth << " m\n";
	stream << "Component count: " << result.numComponents << "\n";
	stream << "Frequency range: [" << result.fMin << ", " << result.fMax << "] Hz\n";
	stream << "Spectral area: " << result.spectralArea << " m^2\n";
	stream << "m0: " << result.zeroMoment << " m^2\n";
	stream << "Primary cache file: " << result.cacheFilePath << "\n";
	stream << "Primary metadata file: " << result.metadataFilePath << "\n";
	stream << "WaveL scope: first-order incident wave environment service\n";
	if (!result.componentsFilePath.empty())
		stream << "Aux components file: " << result.componentsFilePath << "\n";
	if (!result.timeSeriesFilePath.empty())
		stream << "Aux time series file: " << result.timeSeriesFilePath << "\n";
	if (!result.kinematicsDirectory.empty())
		stream << "Diagnostic VTU directory: " << result.kinematicsDirectory << "\n";
	if (!result.warnings.empty())
	{
		stream << "Warnings:\n";
		for (const auto &warning : result.warnings)
			stream << "  - " << warning << "\n";
	}
	ZFile::WriteAllText(path, stream.str());
}

/**
 * @brief 写入波浪元数据文件 (.wfm)。
 *        Write wave metadata file (.wfm).
 *
 * 输出比摘要文件更详细的元数据，包含网格参数（GridNX/NY/NZ 和 GridDX/DY/DZ）、
 * 时间步长和模拟时长，以及所有输出文件路径。
 *
 * Outputs more detailed metadata than summary, including grid parameters (GridNX/NY/NZ
 * and GridDX/DY/DZ), time step and simulation duration, and all output file paths.
 *
 * @param path      输出文件路径。Output file path.
 * @param input     输入参数。Input parameters.
 * @param result    频谱结果。Spectrum result.
 * @param modeLabel 模式标签（"GENERATE" 或 "IMPORT"）。Mode label ("GENERATE" or "IMPORT").
 */
inline void WriteWaveMetadataFile(const std::string &path,
	const WaveLInput &input,
	const WaveSpectrumResult &result,
	const std::string &modeLabel)
{
	std::ostringstream stream;
	stream.setf(std::ios::fixed);
	stream.precision(6);
	stream << "Qahse WaveL Wave Field Metadata\n";
	stream << "Mode: " << modeLabel << "\n";
	stream << "Primary interface: .wfc/.wfm\n";
	stream << "Significant wave height Hs: " << result.significantHeight << " m\n";
	stream << "Peak period Tp: " << result.peakPeriod << " s\n";
	stream << "Peak frequency fp: " << result.peakFrequency << " Hz\n";
	stream << "Water depth: " << input.waterDepth << " m\n";
	stream << "WaveL scope: first-order incident wave environment service\n";
	stream << "Component count: " << result.numComponents << "\n";
	stream << "Frequency range: [" << result.fMin << ", " << result.fMax << "] Hz\n";
	stream << "Spectral area: " << result.spectralArea << " m^2\n";
	stream << "m0: " << result.zeroMoment << " m^2\n";
	stream << "GridNX/GridNY/GridNZ: " << input.gridNX << ", " << input.gridNY << ", " << input.gridNZ << "\n";
	stream << "GridDX/GridDY/GridDZ: " << input.gridDX << ", " << input.gridDY << ", " << input.gridDZ << " m\n";
	stream << "TimeStep/Duration: " << input.timeStep << " s, " << input.simDuration << " s\n";
	stream << "Cache file: " << result.cacheFilePath << "\n";
	if (!result.componentsFilePath.empty())
		stream << "Aux components file: " << result.componentsFilePath << "\n";
	if (!result.timeSeriesFilePath.empty())
		stream << "Aux time series file: " << result.timeSeriesFilePath << "\n";
	if (!result.summaryFilePath.empty())
		stream << "Aux summary file: " << result.summaryFilePath << "\n";
	if (!result.kinematicsDirectory.empty())
		stream << "Diagnostic VTU directory: " << result.kinematicsDirectory << "\n";
	if (!result.warnings.empty())
	{
		stream << "Warnings:\n";
		for (const auto &warning : result.warnings)
			stream << "  - " << warning << "\n";
	}
	ZFile::WriteAllText(path, stream.str());
}
