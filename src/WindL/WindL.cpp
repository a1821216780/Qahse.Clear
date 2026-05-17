#include "WindL/WindL.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "SiMwind/IO/LocaleString_SimWind.hpp"

namespace
{
constexpr double kTiny = 1.0e-12;

std::string Trim(const std::string &text)
{
	const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
	if (first == text.end())
		return {};
	const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
	return std::string(first, last);
}

std::string StripBom(std::string text)
{
	if (text.size() >= 3 &&
	    static_cast<unsigned char>(text[0]) == 0xEF &&
	    static_cast<unsigned char>(text[1]) == 0xBB &&
	    static_cast<unsigned char>(text[2]) == 0xBF)
	{
		text.erase(0, 3);
	}
	return text;
}

std::string ToLower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	return text;
}

bool IsWordChar(char ch)
{
	const auto uch = static_cast<unsigned char>(ch);
	return std::isalnum(uch) != 0 || ch == '_';
}

bool ContainsKey(const std::string &line, const std::string &key)
{
	const std::string lower = ToLower(line);
	const std::string lowerKey = ToLower(key);
	std::size_t pos = lower.find(lowerKey);
	while (pos != std::string::npos)
	{
		const bool leftOk = pos == 0 || !IsWordChar(lower[pos - 1]);
		const std::size_t end = pos + lowerKey.size();
		const bool rightOk = end >= lower.size() || !IsWordChar(lower[end]);
		std::size_t after = end;
		while (after < lower.size() && std::isspace(static_cast<unsigned char>(lower[after])) != 0)
			++after;
		const bool notAssignmentComment = after >= lower.size() || lower[after] != '=';
		if (leftOk && rightOk && notAssignmentComment)
			return true;
		pos = lower.find(lowerKey, pos + 1);
	}
	return false;
}

bool StartsWithValue(const std::string &line)
{
	if (line.empty())
		return false;
	const char ch = line.front();
	const auto uch = static_cast<unsigned char>(ch);
	return ch == '"' || ch == '+' || ch == '-' || ch == '.' || std::isdigit(uch) != 0 || std::isalpha(uch) != 0;
}

std::string FirstValue(const std::string &line)
{
	const std::string trimmed = Trim(line);
	if (trimmed.empty())
		return {};

	if (trimmed.front() == '"')
	{
		const auto end = trimmed.find('"', 1);
		if (end == std::string::npos)
			throw std::runtime_error("Unterminated quoted WindL value: " + line);
		return trimmed.substr(1, end - 1);
	}

	std::istringstream in(trimmed);
	std::string value;
	in >> value;
	return value;
}

int ParseIntValue(const std::string &line, const std::string &key)
{
	try
	{
		return std::stoi(FirstValue(line));
	}
	catch (const std::exception &)
	{
		throw std::runtime_error("Invalid integer value for WindL key " + key + ": " + line);
	}
}

double ParseDoubleValue(const std::string &line, const std::string &key)
{
	try
	{
		return std::stod(FirstValue(line));
	}
	catch (const std::exception &)
	{
		throw std::runtime_error("Invalid numeric value for WindL key " + key + ": " + line);
	}
}

bool ParseBoolValue(const std::string &line, const std::string &key)
{
	std::string value = ToLower(FirstValue(line));
	if (value == "true" || value == "1" || value == "yes")
		return true;
	if (value == "false" || value == "0" || value == "no")
		return false;
	throw std::runtime_error("Invalid boolean value for WindL key " + key + ": " + line);
}

std::filesystem::path ResolvePath(const std::filesystem::path &baseFile, const std::string &value)
{
	if (value.empty())
		return {};
	std::filesystem::path path(value);
	if (path.is_relative())
		path = baseFile.parent_path() / path;
	return std::filesystem::absolute(path).lexically_normal();
}

WindLWindType ParseWindType(int value)
{
	switch (value)
	{
	case 1: return WindLWindType::STEADY;
	case 2: return WindLWindType::USER_DEFINED;
	case 3: return WindLWindType::TURBSIM_WND;
	case 4: return WindLWindType::BLADED_WND;
	case 5: return WindLWindType::TURBSIM_BTS;
	case 6:
	case 8:
		throw std::runtime_error("WindL no longer supports WindType=" + std::to_string(value) + "; use SimWind for wind-file generation.");
	default:
		throw std::runtime_error("Unsupported WindL WindType=" + std::to_string(value));
	}
}

bool IsImportedType(WindLWindType type)
{
	return type == WindLWindType::TURBSIM_WND || type == WindLWindType::BLADED_WND || type == WindLWindType::TURBSIM_BTS;
}

std::string SelectedWindFilePath(const WindLInput &input)
{
	switch (input.windType)
	{
	case WindLWindType::TURBSIM_WND: return input.turWindFilePath;
	case WindLWindType::BLADED_WND: return input.bldWindFilePath;
	case WindLWindType::TURBSIM_BTS: return input.iecWindFilePath;
	default: return {};
	}
}

WndFormat SelectedWindFormat(WindLWindType type)
{
	switch (type)
	{
	case WindLWindType::TURBSIM_WND: return WndFormat::TURBSIM_WND;
	case WindLWindType::BLADED_WND: return WndFormat::BLADED_WND;
	case WindLWindType::TURBSIM_BTS: return WndFormat::TURBSIM_BTS;
	default: return WndFormat::TURBSIM_BTS;
	}
}

WindImportMetadata BuildImportMetadata(const WindLInput &input)
{
	WindImportMetadata metadata;
	metadata.filePath = SelectedWindFilePath(input);
	metadata.format = SelectedWindFormat(input.windType);
	metadata.hubHeight = input.refHeight;
	metadata.refHeight = input.refHeight;
	metadata.meanWindSpeed = input.hWindSpeed;
	return metadata;
}

std::string FormatSeconds(double value)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << value;
	return out.str();
}

void ValidateImportInput(const WindImportMetadata &metadata)
{
	if (metadata.filePath.empty())
		throw std::runtime_error("WindL import requires a selected wind-file path");

	const std::filesystem::path path(metadata.filePath);
	if (!std::filesystem::is_regular_file(path))
		throw std::runtime_error("The import wind file does not exist: " + path.string());

	auto ext = ToLower(path.extension().string());
	if (ext != ".bts" && ext != ".wnd")
		throw std::runtime_error("WindL import supports only .bts and .wnd wind files");
	if (ext == ".bts" && metadata.format != WndFormat::TURBSIM_BTS)
		throw std::runtime_error("WindL .bts import requires WndFormat=TURBSIM_BTS");
	if (ext == ".wnd" && metadata.format == WndFormat::TURBSIM_BTS)
		throw std::runtime_error("WindL .wnd import requires a TurbSim WND or Bladed WND decoder");
}

std::filesystem::path ImportSummaryBasePath(const WindImportMetadata &metadata)
{
	if (!metadata.savePath.empty() && !metadata.saveName.empty())
		return std::filesystem::path(metadata.savePath) / metadata.saveName;

	const std::filesystem::path source(metadata.filePath);
	if (!source.empty())
		return source.parent_path() / (source.stem().string() + "_import");

	return std::filesystem::current_path() / "import_wind";
}

void WriteImportSummary(const WindImportMetadata &metadata, const ::WindField &field, const std::filesystem::path &path)
{
	std::ofstream out(path);
	if (!out)
		throw std::runtime_error(std::string(L_WIND_CannotOpenSUM) + ": " + path.string());

	out << L_SUM_Title << "\n";
	out << L_SUM_Separator << "\n\n";
	out << std::setprecision(10);
	out << "ImportSource: " << field.sourcePath.string() << "\n";
	out << "ImportFormat: ";
	switch (field.wndFormat)
	{
	case WndFormat::TURBSIM_BTS: out << "TURBSIM_BTS"; break;
	case WndFormat::TURBSIM_WND: out << "TURBSIM_WND"; break;
	case WndFormat::BLADED_WND: out << "BLADED_WND"; break;
	}
	out << "\n";
	out << "UsedCompanionSummary: " << (field.usedCompanionSummary ? "true" : "false") << "\n\n";

	out << L_SUM_Grid << "\n";
	out << "  NumPointY: " << field.ny << "\n";
	out << "  NumPointZ: " << field.nz << "\n";
	out << "  LenWidthY: " << field.fieldDimY << " m\n";
	out << "  LenHeightZ: " << field.fieldDimZ << " m\n";
	out << "  Zbottom: " << field.zBottom << " m\n";
	out << "  TimeStep: " << field.dt << " s\n";
	out << "  NumSteps: " << field.nSteps << "\n";
	out << "  HubHt: " << field.hubHeight << " m\n";
	out << "  MeanWindSpeed: " << field.meanWindSpeed << " m/s\n\n";

	out << L_SUM_InputKeywordStatus << "\n";
	out << "  CreadW/CycleWind: sampling-time boundary behavior\n";
	out << "  InterpMethod: imported-field sampling interpolation method\n";
	out << "  SelectedWindFile: " << metadata.filePath << "\n\n";

	out << L_SUM_Statistics << "\n";
	static const char *names[3] = {"u", "v", "w"};
	for (int comp = 0; comp < 3; ++comp)
	{
		out << "  " << names[comp] << ": mean=" << field.mean[static_cast<std::size_t>(comp)]
		    << " sigma=" << field.sigma[static_cast<std::size_t>(comp)]
		    << " TI=" << 100.0 * field.turbulenceIntensity[static_cast<std::size_t>(comp)] << "%\n";
	}

	if (!field.warnings.empty())
	{
		out << "\n" << L_SUM_Warnings << "\n";
		for (const auto &warning : field.warnings)
			out << "  - " << warning << "\n";
	}
}

bool ParseTwoDoubles(const std::string &line, double &a, double &b)
{
	std::istringstream in(line);
	in >> a >> b;
	return static_cast<bool>(in);
}

double NormalizeUserTime(double time, double duration, bool cycle)
{
	if (time < 0.0)
		return 0.0;
	if (duration <= 0.0)
		return 0.0;
	if (time <= duration)
		return time;
	if (!cycle)
		return duration;
	double wrapped = std::fmod(time, duration);
	if (wrapped < 0.0)
		wrapped += duration;
	return wrapped;
}

double InterpolateUserSpeed(const std::vector<WindLTimeSpeed> &series, double time, bool cycle)
{
	if (series.empty())
		throw std::runtime_error("WindType=2 requires WindSpeedList data");
	if (series.size() == 1)
		return series.front().speed;

	const double t = NormalizeUserTime(time, series.back().time, cycle);
	if (t <= series.front().time)
		return series.front().speed;
	if (t >= series.back().time)
		return series.back().speed;

	const auto upper = std::upper_bound(series.begin(), series.end(), t, [](double value, const WindLTimeSpeed &point) {
		return value < point.time;
	});
	const auto left = std::prev(upper);
	const double span = std::max(upper->time - left->time, kTiny);
	const double alpha = (t - left->time) / span;
	return left->speed * (1.0 - alpha) + upper->speed * alpha;
}

double ApplyPowerLaw(double speed, double z, double refHeight, double plExp)
{
	if (std::abs(plExp) <= kTiny || refHeight <= kTiny)
		return speed;
	const double safeZ = std::max(z, kTiny);
	return speed * std::pow(safeZ / refHeight, plExp);
}
} // namespace

WindLInput ReadWindLInput(const std::string &path)
{
	return WindL::ReadInputFile(path);
}

WindLInput WindL::ReadInputFile(const std::string &path)
{
	std::ifstream in(path);
	if (!in)
		throw std::runtime_error("Cannot open WindL input file: " + path);

	WindLInput input;
	input.inputPath = std::filesystem::absolute(path).lexically_normal();

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(in, line))
		lines.push_back(StripBom(line));

	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		const std::string current = Trim(lines[i]);
		if (current.empty())
			continue;
		if (current.rfind("END", 0) == 0)
			break;
		if (!StartsWithValue(current))
			continue;

		if (ContainsKey(current, "WindType"))
		{
			input.windType = ParseWindType(ParseIntValue(current, "WindType"));
		}
		else if (ContainsKey(current, "CreadW") || ContainsKey(current, "CycleWind"))
		{
			input.cycleWind = ParseBoolValue(current, "CreadW");
		}
		else if (ContainsKey(current, "HWindSpeed"))
		{
			input.hWindSpeed = ParseDoubleValue(current, "HWindSpeed");
		}
		else if (ContainsKey(current, "RefHt"))
		{
			input.refHeight = ParseDoubleValue(current, "RefHt");
		}
		else if (ContainsKey(current, "PLexp") || ContainsKey(current, "PLExp"))
		{
			input.plExp = ParseDoubleValue(current, "PLexp");
		}
		else if (ContainsKey(current, "gridY_min"))
		{
			input.gridYMin = ParseDoubleValue(current, "gridY_min");
		}
		else if (ContainsKey(current, "gridY_max"))
		{
			input.gridYMax = ParseDoubleValue(current, "gridY_max");
		}
		else if (ContainsKey(current, "gridY_step"))
		{
			input.gridYStep = ParseDoubleValue(current, "gridY_step");
		}
		else if (ContainsKey(current, "gridZ_min"))
		{
			input.gridZMin = ParseDoubleValue(current, "gridZ_min");
		}
		else if (ContainsKey(current, "gridZ_max"))
		{
			input.gridZMax = ParseDoubleValue(current, "gridZ_max");
		}
		else if (ContainsKey(current, "gridZ_step"))
		{
			input.gridZStep = ParseDoubleValue(current, "gridZ_step");
		}
		else if (ContainsKey(current, "WindSpeedNum"))
		{
			const int count = ParseIntValue(current, "WindSpeedNum");
			if (count < 0)
				throw std::runtime_error("WindSpeedNum cannot be negative");
			input.windSpeedList.clear();
			for (int row = 0; row < count;)
			{
				++i;
				if (i >= lines.size())
					throw std::runtime_error("WindSpeedNum exceeds available WindSpeedList rows");
				const std::string dataLine = Trim(lines[i]);
				if (dataLine.empty())
					continue;
				double t = 0.0;
				double speed = 0.0;
				if (!ParseTwoDoubles(dataLine, t, speed))
					throw std::runtime_error("Invalid WindSpeedList row: " + dataLine);
				input.windSpeedList.push_back({t, speed});
				++row;
			}
		}
		else if (ContainsKey(current, "TurWindFilePath"))
		{
			input.turWindFilePath = ResolvePath(input.inputPath, FirstValue(current)).string();
		}
		else if (ContainsKey(current, "BldWindFilePath"))
		{
			input.bldWindFilePath = ResolvePath(input.inputPath, FirstValue(current)).string();
		}
		else if (ContainsKey(current, "IECWindFilePath"))
		{
			input.iecWindFilePath = ResolvePath(input.inputPath, FirstValue(current)).string();
		}
	}

	return input;
}

void WindL::ValidateInputOnly(const WindLInput &input)
{
	if ((input.windType == WindLWindType::STEADY || input.windType == WindLWindType::USER_DEFINED) && input.hWindSpeed <= 0.0)
		throw std::runtime_error("WindType=1/2 requires positive HWindSpeed");

	if (input.windType == WindLWindType::USER_DEFINED)
	{
		if (input.windSpeedList.empty())
			throw std::runtime_error("WindType=2 requires WindSpeedList rows");
		for (std::size_t i = 1; i < input.windSpeedList.size(); ++i)
		{
			if (input.windSpeedList[i].time <= input.windSpeedList[i - 1].time)
				throw std::runtime_error("WindSpeedList time values must be strictly increasing");
		}
	}

	if (IsImportedType(input.windType))
		ValidateImportInput(BuildImportMetadata(input));
}

void WindL::ValidateImportInputOnly(const WindImportMetadata &metadata)
{
	ValidateImportInput(metadata);
}

::WindField WindL::Import(const WindImportMetadata &metadata, WindLProgressCallback progress)
{
	ValidateImportInputOnly(metadata);
	if (progress)
		progress(std::string(" Importing wind file \"") + metadata.filePath + "\".");

	::WindField field = ::WindField::ReadAny(metadata.filePath, metadata.format, metadata);
	if (progress)
	{
		progress(std::string(" Imported format with grid ") + std::to_string(field.ny) + " x " + std::to_string(field.nz) +
		         ", steps=" + std::to_string(field.nSteps) + ", dt=" + FormatSeconds(field.dt) + ".");
	}

	if (metadata.sumPrint)
	{
		auto summaryPath = ImportSummaryBasePath(metadata).replace_extension(".sum");
		field.summaryPath = summaryPath.string();
		if (progress)
			progress(std::string(" Writing import summary \"") + field.summaryPath + "\".");
		WriteImportSummary(metadata, field, summaryPath);
	}

	return field;
}

::WindField WindL::ImportFromFile(const std::string &path, WindLProgressCallback progress)
{
	const WindL model = LoadFromFile(path, std::move(progress));
	if (!model.HasImportedField())
		throw std::runtime_error("WindL input does not select an imported wind-file WindType");
	return *model.ImportedField();
}

WindL WindL::Load(const WindLInput &input, WindLProgressCallback progress)
{
	ValidateInputOnly(input);

	WindL model;
	model.input_ = input;
	if (IsImportedType(input.windType))
	{
		model.importedField_ = Import(BuildImportMetadata(input), std::move(progress));
		model.hasImportedField_ = true;
	}
	else if (progress)
	{
		progress(input.windType == WindLWindType::STEADY ? " Loaded steady WindL source." : " Loaded user-defined WindL source.");
	}
	return model;
}

WindL WindL::LoadFromFile(const std::string &path, WindLProgressCallback progress)
{
	return Load(ReadInputFile(path), std::move(progress));
}

std::array<double, 3> WindL::VelocityAt(double x, double y, double z, double time, const WindVelocityOptions &options) const
{
	if (hasImportedField_)
	{
		WindVelocityOptions effective = options;
		if (!input_.cycleWind)
			effective.cycleWind = false;
		return importedField_.SampleAt(x, y, z, time, effective);
	}

	double speed = input_.hWindSpeed;
	if (input_.windType == WindLWindType::USER_DEFINED)
		speed = InterpolateUserSpeed(input_.windSpeedList, time, input_.cycleWind);
	speed = ApplyPowerLaw(speed, z, input_.refHeight, input_.plExp);
	return {speed, 0.0, 0.0};
}

const WindLInput &WindL::Input() const
{
	return input_;
}

const WindField *WindL::ImportedField() const
{
	return hasImportedField_ ? &importedField_ : nullptr;
}

bool WindL::HasImportedField() const
{
	return hasImportedField_;
}
