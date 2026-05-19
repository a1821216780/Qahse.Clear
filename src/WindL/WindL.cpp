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
#include "WindL/IO/WindL_IO_Subs.hpp"

namespace
{
constexpr double kTiny = 1.0e-12;

std::string ToLower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	return text;
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
	return windl_io_detail::ReadWindLInputFile(path);
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

Vec3 WindL::getWindspeed(Vec3 vec, double time, bool mirror, bool isAutoFielShift, double shiftTime) const
{
	if (hasImportedField_)
		return importedField_.getWindspeed(vec, time, mirror, isAutoFielShift, shiftTime);

	WindVelocityOptions options;
	options.mirrorTime = mirror;
	options.cycleWind = !mirror;
	options.autoFieldShift = isAutoFielShift;
	options.shiftTime = isAutoFielShift ? 0.0 : shiftTime;
	const auto velocity = VelocityAt(vec.x, vec.y, vec.z, time, options);
	return Vec3(velocity[0], velocity[1], velocity[2]);
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
