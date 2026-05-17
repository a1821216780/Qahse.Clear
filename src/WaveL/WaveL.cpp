#include "WaveL/WaveL.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <utility>

#include "WaveL/IO/LocaleString_WaveL.hpp"
#include "WaveL/IO/WaveL_IO_Subs.hpp"
#include "WaveL/WaveSpectrum.hpp"

namespace
{
std::filesystem::path OutputBasePath(const WaveLInput &input)
{
	if (!input.savePath.empty() && !input.saveName.empty())
		return std::filesystem::path(input.savePath) / input.saveName;
	if (!input.inputPath.empty())
		return input.inputPath.parent_path() / "WaveL_result";
	return std::filesystem::current_path() / "WaveL_result";
}

void RequireFile(const std::string &path, const std::string &name)
{
	if (path.empty())
		throw std::runtime_error("WaveL requires " + name);
	if (!std::filesystem::is_regular_file(path))
		throw std::runtime_error("WaveL cannot find " + name + ": " + path);
}

void WriteSummary(const WaveLInput &input, const WaveLResult &result, const std::filesystem::path &path)
{
	if (!path.parent_path().empty())
		std::filesystem::create_directories(path.parent_path());
	std::ofstream out(path);
	if (!out)
		throw std::runtime_error("Cannot write WaveL summary: " + path.string());

	out << L_WAVEL_SUM_TITLE << "\n";
	out << "------------------------------------------------------------\n\n";
	out << std::setprecision(12);
	out << "Input: " << input.inputPath.string() << "\n";
	out << "WaveType: " << static_cast<int>(input.waveType) << "\n";
	out << "WaterDepth: " << input.waterDepth << "\n";
	out << "Gravity: " << input.gravity << "\n";
	out << "WaveStretching: " << static_cast<int>(input.waveStretching) << "\n";
	out << "TimeOffset: " << input.timeOffset << "\n";
	out << "Hs: " << input.hs << "\n";
	out << "Tp: " << input.tp << "\n";
	out << "Components: " << result.components.size() << "\n";
	if (!result.componentPath.empty())
		out << "ComponentFile: " << result.componentPath << "\n";
	out << "\nFirstComponents:\n";
	for (std::size_t i = 0; i < result.components.size() && i < 8; ++i)
	{
		const auto &c = result.components[i];
		out << "  " << i
		    << " f=" << c.frequency
		    << " A=" << c.amplitude
		    << " phaseRad=" << c.phase
		    << " dirRad=" << c.direction
		    << " k=" << c.wavenumber << "\n";
	}
}
} // namespace

WaveLInput ReadWaveLInput(const std::string &path)
{
	return WaveL::ReadInputFile(path);
}

WaveLInput WaveL::ReadInputFile(const std::string &path)
{
	return wavel_io_detail::ReadWaveLInputFile(path);
}

void WaveL::ValidateInputOnly(const WaveLInput &input)
{
	if (input.waveType == WaveType::NONE)
		return;
	if (input.waterDepth <= 0.0)
		throw std::runtime_error("WaveL requires positive WaterDepth");
	if (input.gravity <= 0.0)
		throw std::runtime_error("WaveL requires positive Gravity");
	if (input.waveType == WaveType::REGULAR ||
	    input.waveType == WaveType::JONSWAP ||
	    input.waveType == WaveType::ISSC ||
	    input.waveType == WaveType::TORSETHAUGEN)
	{
		if (input.hs <= 0.0 || input.tp <= 0.0)
			throw std::runtime_error("WaveL WaveType=1/2/3/4 requires positive Hs and Tp");
	}
	if (input.waveType == WaveType::OCHI_HUBBLE)
	{
		if (input.autoOchi && input.hs <= 0.0)
			throw std::runtime_error("WaveL WaveType=5 requires positive Hs when AutoOchi is true");
		if (!input.autoOchi &&
		    (input.ochiHs1 <= 0.0 || input.ochiHs2 <= 0.0 ||
		     input.ochiF1 <= 0.0 || input.ochiF2 <= 0.0 ||
		     input.ochiLambda1 <= 0.0 || input.ochiLambda2 <= 0.0))
			throw std::runtime_error("WaveL WaveType=5 requires positive Ochi parameters when AutoOchi is false");
	}
	if (input.waveType == WaveType::USER_SPECTRUM)
		RequireFile(input.spectrumFilePath, "SpectrumFilePath");
	if (input.waveType == WaveType::COMPONENT_FILE)
		RequireFile(input.componentFilePath, "ComponentFilePath");
	if (input.waveType == WaveType::ELEVATION_TIME_SERIES)
		RequireFile(input.timeSeriesFilePath, "TimeSeriesFilePath");
}

WaveL WaveL::Load(const WaveLInput &input, WaveLProgressCallback progress)
{
	ValidateInputOnly(input);

	WaveL model;
	model.input_ = input;
	if (progress)
		progress(" Building WaveL wave components.");

	model.result_.components = wavel_spectrum::BuildComponents(input);
	model.field_ = WaveField(input, model.result_.components);

	const auto basePath = OutputBasePath(input);
	if (input.saveComponents)
	{
		model.result_.componentPath = basePath.string() + "_components.dat";
		if (progress)
			progress(" Writing WaveL component file \"" + model.result_.componentPath + "\".");
		wavel_spectrum::WriteComponentFile(model.result_.componentPath, model.result_.components);
	}
	if (input.sumPrint)
	{
		model.result_.summaryPath = basePath.string() + ".sum";
		if (progress)
			progress(" Writing WaveL summary \"" + model.result_.summaryPath + "\".");
		WriteSummary(input, model.result_, model.result_.summaryPath);
	}

	if (progress)
		progress(" Loaded WaveL source with " + std::to_string(model.result_.components.size()) + " components.");
	return model;
}

WaveL WaveL::LoadFromFile(const std::string &path, WaveLProgressCallback progress)
{
	return Load(ReadInputFile(path), std::move(progress));
}

double WaveL::GetElevation(Vec3 pos, double time) const
{
	return field_.GetElevation(pos, time);
}

std::vector<double> WaveL::GetElevationPerDirection(Vec3 pos,
                                                    double time,
                                                    const std::vector<double> &waveDir,
                                                    double deltaDir) const
{
	return field_.GetElevationPerDirection(pos, time, waveDir, deltaDir);
}

void WaveL::GetVelocityAndAcceleration(Vec3 pos,
                                       double time,
                                       double elevation,
                                       double depth,
                                       WaveStretching stretchingType,
                                       Vec3 *vel,
                                       Vec3 *acc,
                                       double *dynP,
                                       int isFuchs,
                                       double dia) const
{
	field_.GetVelocityAndAcceleration(pos,
	                                  time,
	                                  elevation,
	                                  depth,
	                                  stretchingType,
	                                  vel,
	                                  acc,
	                                  dynP,
	                                  isFuchs,
	                                  dia);
}

Vec3 WaveL::GetOceanCurrentAt(Vec3 position, double elevation) const
{
	return field_.GetOceanCurrentAt(position, elevation);
}

double WaveL::GetPhaseMCFPhaseShift(double x) const
{
	return field_.GetPhaseMCFPhaseShift(x);
}

double WaveL::ElevationAt(double x, double y, double time) const
{
	return GetElevation(Vec3(x, y, 0.0), time);
}

SeaState WaveL::StateAt(double x, double y, double z, double time) const
{
	return field_.StateAt(x, y, z, time);
}

const WaveLInput &WaveL::Input() const
{
	return input_;
}

const WaveField &WaveL::Field() const
{
	return field_;
}

const WaveLResult &WaveL::Result() const
{
	return result_;
}
