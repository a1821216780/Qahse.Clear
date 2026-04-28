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

#include "../WindL_Type.hpp"
#include "../../IO/Serializer.hpp"
#include "../../IO/Yaml.hpp"
#include "../../IO/ZFile.hpp"
#include "../../IO/ZPath.hpp"
#include "../../IO/ZString.hpp"

namespace windl_io_detail
{
	inline constexpr const char *kYamlRoot = "Qahse.WindL";

	inline std::string WindLYamlKey(const std::string &key)
	{
		return std::string(kYamlRoot) + "." + key;
	}

	inline std::string Upper(const std::string &value)
	{
		return ZString::ToUpper(std::string(value));
	}

	inline bool IsYamlPath(const std::string &path)
	{
		std::string ext = std::filesystem::path(path).extension().string();
		ext = Upper(ext);
		return ext == ".YML" || ext == ".YAML";
	}

	inline void RequireFile(const std::string &path)
	{
		if (!ZFile::Exists(path))
			throw std::runtime_error("File not found: " + path);
	}

	inline std::string KeyForFormat(Serializer::Format format, const std::string &key)
	{
		return format == Serializer::Format::Yaml ? WindLYamlKey(key) : key;
	}

	inline std::string CohModelToText(CohModel value)
	{
		switch (value)
		{
		case CohModel::IEC: return "IEC";
		case CohModel::GENERAL: return "GENERAL";
		case CohModel::DEFAULT_COH: return "default";
		default: return "default";
		}
	}

	inline CohModel ParseCohModel(std::string token, CohModel defaultValue = CohModel::DEFAULT_COH)
	{
		token = ZString::Trim(token);
		if (token.empty())
			return defaultValue;

		const std::string upper = Upper(token);
		if (upper == "DEFAULT" || upper == "DEFAULT_COH")
			return CohModel::DEFAULT_COH;
		if (upper == "IEC" || token == "0")
			return CohModel::IEC;
		if (upper == "GENERAL" || token == "1")
			return CohModel::GENERAL;

		try { return ZString::StringToEnum<CohModel>(token, true); }
		catch (...) { return defaultValue; }
	}

	inline void ApplyDerivedFields(WindLInput &input)
	{
		if (input.windModel == WindModel::EWM1)
		{
			input.ewmReturn = 1;
			input.ewmType = EWMType::Turbulent;
		}
		else if (input.windModel == WindModel::EWM50)
		{
			input.ewmReturn = 50;
			input.ewmType = EWMType::Turbulent;
		}
	}

	inline void ResolveIfSet(const std::string &baseFilePath, std::string &path)
	{
		if (!path.empty())
			path = ZPath::ResolvePath(baseFilePath, path);
	}

	inline void ResolvePaths(WindLInput &input, const std::string &baseFilePath)
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

	template <typename T>
	struct FieldRef
	{
		const char *key;
		T &value;
	};

	template <typename T>
	FieldRef<T> Field(const char *key, T &value)
	{
		return {key, value};
	}

	class WindLInputSerializer : public Serializer
	{
	public:
		WindLInput data;

		WindLInputSerializer()
		{
			SetValueFirst(true);
		}

		explicit WindLInputSerializer(const WindLInput &input)
			: data(input)
		{
			SetValueFirst(true);
		}

	protected:
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
				Field("EWMType", data.ewmType),
				Field("EWMReturn", data.ewmReturn),
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
				Field("BatchLauncher", data.batchLauncher));
		}

	private:
		template <typename T>
		void One(FieldRef<T> field)
		{
			ReadOrWrite(KeyForFormat(GetFormat(), field.key), field.value);
		}

		template <typename... FieldsT>
		void Fields(FieldsT... fields)
		{
			(One(fields), ...);
		}

		void CohField(const char *key, CohModel &value)
		{
			std::string text = CohModelToText(value);
			ReadOrWrite(KeyForFormat(GetFormat(), key), text);
			if (IsReadMode())
				value = ParseCohModel(text, value);
		}
	};

	inline void AddYamlNode(YML &yaml, const std::string &key, const std::string &value)
	{
		yaml.AddNode(WindLYamlKey(key), value);
	}

	template <typename T>
	void AddYamlValue(YML &yaml, const std::string &key, const T &value)
	{
		AddYamlNode(yaml, key, YML::ToYmlValueString(value));
	}

	inline int ReadYamlInt(const YML &yaml, const std::string &key, int defaultValue = 0)
	{
		const std::string value = yaml.read(WindLYamlKey(key));
		return ZString::Trim(value).empty() ? defaultValue : YML::YmlToInt(value);
	}

	inline std::vector<std::vector<double>> ReadYamlMatrix(const YML &yaml, const std::string &key)
	{
		const std::string value = yaml.read(WindLYamlKey(key));
		if (ZString::Trim(value).empty())
			return {};
		return YML::YmlTo2DDoubleArray(value).data;
	}

	inline std::string FormatDouble(double value)
	{
		std::ostringstream stream;
		stream.imbue(std::locale::classic());
		stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
		return stream.str();
	}

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

	template <typename T>
	T ReadKeywordValue(const std::vector<std::string> &lines, const std::string &targetKey, T defaultValue)
	{
		const std::string target = Upper(targetKey);
		for (const auto &line : lines)
		{
			std::string key;
			std::string value;
			if (!TryParseKeywordLine(line, key, value))
				continue;
			if (Upper(key) != target)
				continue;

			if (Upper(ZString::Trim(value)) == "DEFAULT")
				return defaultValue;

			try { return ZString::StringTo<T>(value); }
			catch (...) { return defaultValue; }
		}
		return defaultValue;
	}

	inline std::vector<std::vector<double>> ReadRowsAfterBegin(const std::vector<std::string> &lines,
	                                                           std::size_t expectedCols,
	                                                           std::size_t maxRows = 0)
	{
		std::size_t start = 0;
		for (std::size_t i = 0; i < lines.size(); ++i)
		{
			const std::string text = Upper(ZString::Trim(lines[i]));
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

	template <typename... Columns>
	std::size_t MinColumnSize(const Columns &...columns)
	{
		return std::min({columns.size()...});
	}

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

	template <typename... Columns>
	void AppendColumns(const std::vector<double> &row, Columns &...columns)
	{
		std::size_t index = 0;
		((columns.push_back(row[index++])), ...);
	}

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

	inline void AddRows(std::vector<std::string> &lines, const std::vector<std::vector<double>> &rows)
	{
		for (const auto &row : rows)
			lines.push_back(JoinNumbers(row));
	}

	inline std::vector<std::vector<double>> PointRows(const UserWindSpeedData &data)
	{
		std::vector<std::vector<double>> rows;
		rows.reserve(data.points.size());
		for (const auto &point : data.points)
			rows.push_back({point.y, point.z});
		return rows;
	}

	inline int EffectiveComponentCount(const UserWindSpeedData &data)
	{
		if (data.nComp > 0)
			return data.nComp;
		if (!data.components.empty())
			return static_cast<int>(data.components.front().size());
		return 0;
	}

	inline int EffectivePointCount(const UserWindSpeedData &data)
	{
		if (data.nPoints > 0)
			return data.nPoints;
		if (!data.points.empty())
			return static_cast<int>(data.points.size());
		return static_cast<int>(data.components.size());
	}

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

	inline char ComponentName(int index)
	{
		if (index == 0) return 'u';
		if (index == 1) return 'v';
		if (index == 2) return 'w';
		return static_cast<char>('a' + index);
	}

	inline double ReadYamlDoubleOr(const YML &yaml, const std::string &key, double defaultValue)
	{
		const std::string value = yaml.read(WindLYamlKey(key));
		return ZString::Trim(value).empty() ? defaultValue : YML::YmlToDouble(value);
	}
}

inline WindLInput ReadWindLInput(const std::string &path)
{
	windl_io_detail::RequireFile(path);

	windl_io_detail::WindLInputSerializer serializer;
	if (windl_io_detail::IsYamlPath(path))
		serializer.ReadYamlFile(path);
	else
		serializer.ReadTextFile(path);

	windl_io_detail::ResolvePaths(serializer.data, path);
	windl_io_detail::ApplyDerivedFields(serializer.data);
	return serializer.data;
}

inline void WriteWindLInput(const WindLInput &input,
                            const std::string &path,
                            const std::string &templatePath = "")
{
	windl_io_detail::WindLInputSerializer serializer(input);
	if (windl_io_detail::IsYamlPath(path))
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

inline void ConvertWindLInput(const std::string &inputPath,
                              const std::string &outputPath,
                              const std::string &templatePath = "")
{
	WriteWindLInput(ReadWindLInput(inputPath), outputPath, templatePath);
}

inline UserShearData ReadUserShear(const std::string &path)
{
	using namespace windl_io_detail;
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

inline void WriteUserShear(const UserShearData &data, const std::string &path)
{
	using namespace windl_io_detail;
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
	lines.push_back("-- Qahse.WindL user shear table");
	lines.push_back(std::to_string(data.numHeights > 0 ? data.numHeights : static_cast<int>(rows.size())) +
	                " NumUSRz - Number of heights");
	lines.push_back(FormatDouble(data.stdScale1) + " StdScale1 - u standard deviation scale");
	lines.push_back(FormatDouble(data.stdScale2) + " StdScale2 - v standard deviation scale");
	lines.push_back(FormatDouble(data.stdScale3) + " StdScale3 - w standard deviation scale");
	lines.push_back("Height WindSpeed WindDirection StandardDeviation LengthScale");
	lines.push_back("!Begin");
	AddRows(lines, rows);
	ZFile::WriteAllLines(path, lines);
}

inline void ConvertUserShear(const std::string &inputPath, const std::string &outputPath)
{
	WriteUserShear(ReadUserShear(inputPath), outputPath);
}

inline UserSpectraData ReadUserSpectra(const std::string &path)
{
	using namespace windl_io_detail;
	if (IsYamlPath(path))
	{
		RequireFile(path);
		YML yaml(path, false);
		UserSpectraData data;
		data.numFrequencies = ReadYamlInt(yaml, "NumUSRf", 0);
		LoadColumns(ReadYamlMatrix(yaml, "Data"), data.frequencies, data.uPsd, data.vPsd, data.wPsd);
		if (data.numFrequencies == 0)
			data.numFrequencies = static_cast<int>(data.frequencies.size());
		return data;
	}

	RequireFile(path);
	const auto lines = ZFile::ReadAllLines(path);
	UserSpectraData data;
	data.numFrequencies = ReadKeywordValue<int>(lines, "NumUSRf", 0);
	LoadColumns(ReadRowsAfterBegin(lines, 4, static_cast<std::size_t>(std::max(data.numFrequencies, 0))),
	            data.frequencies,
	            data.uPsd,
	            data.vPsd,
	            data.wPsd);
	if (data.numFrequencies == 0)
		data.numFrequencies = static_cast<int>(data.frequencies.size());
	return data;
}

inline void WriteUserSpectra(const UserSpectraData &data, const std::string &path)
{
	using namespace windl_io_detail;
	const auto rows = ColumnsToRows(data.frequencies, data.uPsd, data.vPsd, data.wPsd);

	if (IsYamlPath(path))
	{
		YML yaml;
		AddYamlValue(yaml, "NumUSRf", data.numFrequencies > 0 ? data.numFrequencies : static_cast<int>(rows.size()));
		AddYamlNode(yaml, "Data", YML::ToYmlValueString(rows, 2));
		yaml.save(path);
		return;
	}

	std::vector<std::string> lines;
	lines.reserve(rows.size() + 5);
	lines.push_back("-- Qahse.WindL user spectra table");
	lines.push_back(std::to_string(data.numFrequencies > 0 ? data.numFrequencies : static_cast<int>(rows.size())) +
	                " NumUSRf - Number of frequencies");
	lines.push_back("Frequency uPSD vPSD wPSD");
	lines.push_back("!Begin");
	AddRows(lines, rows);
	ZFile::WriteAllLines(path, lines);
}

inline void ConvertUserSpectra(const std::string &inputPath, const std::string &outputPath)
{
	WriteUserSpectra(ReadUserSpectra(inputPath), outputPath);
}

inline UserWindSpeedData ReadUserWindSpeed(const std::string &path)
{
	using namespace windl_io_detail;
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

inline void WriteUserWindSpeed(const UserWindSpeedData &data, const std::string &path)
{
	using namespace windl_io_detail;
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
	lines.push_back("-- Qahse.WindL user wind speed table");
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

inline void ConvertUserWindSpeed(const std::string &inputPath, const std::string &outputPath)
{
	WriteUserWindSpeed(ReadUserWindSpeed(inputPath), outputPath);
}
