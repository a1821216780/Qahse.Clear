#include "WaveL/WaveField.hpp"

#include <algorithm>
#include <utility>

WaveField::WaveField(WaveSpectrum spectrum)
	: spectrum_(std::move(spectrum))
{
}

WaveKinematics WaveField::GetKinematics(double x, double y, double z, double time) const
{
	return spectrum_.GetKinematics(x, y, z, time);
}

double WaveField::GetFreeSurfaceZ(double x, double y, double time) const
{
	return spectrum_.GetFreeSurfaceZ(x, y, time);
}

bool WaveField::IsSubmerged(double x, double y, double z, double time, double tolerance) const
{
	return spectrum_.IsSubmerged(x, y, z, time, tolerance);
}

bool WaveField::IsWetted(double x, double y, double z, double time, double tolerance) const
{
	return IsSubmerged(x, y, z, time, tolerance);
}

WaveEnvironmentSample WaveField::SampleEnvironment(double x, double y, double z, double time, double tolerance) const
{
	WaveEnvironmentSample sample;
	sample.kinematics = GetKinematics(x, y, z, time);
	sample.freeSurfaceZ = GetFreeSurfaceZ(x, y, time);
	sample.immersionDepth = std::max(0.0, sample.freeSurfaceZ + tolerance - z);
	sample.submerged = z <= sample.freeSurfaceZ + tolerance;
	return sample;
}

std::vector<WaveEnvironmentSample> WaveField::SampleEnvironment(const std::vector<WaveSamplePoint> &points, double time, double tolerance) const
{
	std::vector<WaveEnvironmentSample> samples;
	samples.reserve(points.size());
	for (const auto &point : points)
		samples.push_back(SampleEnvironment(point.x, point.y, point.z, time, tolerance));
	return samples;
}

void WaveField::ValidateInputOnly(const WaveLInput &input)
{
	WaveSpectrum::ValidateInputOnly(input);
}

WaveField WaveField::Generate(const WaveLInput &input, WaveProgressCallback progress)
{
	return WaveField(WaveSpectrum::Generate(input, progress));
}

WaveField WaveField::Import(const WaveLInput &input, WaveProgressCallback progress)
{
	return WaveField(WaveSpectrum::Import(input, progress));
}

WaveField WaveField::GenerateFromFile(const std::string &qoePath, WaveProgressCallback progress)
{
	return WaveField(WaveSpectrum::GenerateFromFile(qoePath, progress));
}

WaveField WaveField::ImportFromFile(const std::string &qoePath, WaveProgressCallback progress)
{
	return WaveField(WaveSpectrum::ImportFromFile(qoePath, progress));
}
