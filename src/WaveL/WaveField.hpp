#pragma once

#include <string>
#include <vector>

#include "WaveL/WaveSpectrum.hpp"

class WaveField
{
public:
	WaveField() = default;
	explicit WaveField(WaveSpectrum spectrum);

	const WaveLInput &GetInput() const { return spectrum_.GetInput(); }
	const WaveSpectrumResult &GetResult() const { return spectrum_.GetResult(); }
	const std::vector<WaveTrain> &GetWaveTrains() const { return spectrum_.GetWaveTrains(); }
	double GetHs() const { return spectrum_.GetHs(); }
	double GetTp() const { return spectrum_.GetTp(); }
	double GetFp() const { return spectrum_.GetFp(); }
	double GetDepth() const { return spectrum_.GetDepth(); }

	WaveKinematics GetKinematics(double x, double y, double z, double time) const;
	double GetFreeSurfaceZ(double x, double y, double time) const;
	bool IsSubmerged(double x, double y, double z, double time, double tolerance = 0.0) const;
	bool IsWetted(double x, double y, double z, double time, double tolerance = 0.0) const;
	WaveEnvironmentSample SampleEnvironment(double x, double y, double z, double time, double tolerance = 0.0) const;
	std::vector<WaveEnvironmentSample> SampleEnvironment(const std::vector<WaveSamplePoint> &points, double time, double tolerance = 0.0) const;

	static void ValidateInputOnly(const WaveLInput &input);
	static WaveField Generate(const WaveLInput &input, WaveProgressCallback progress = {});
	static WaveField Import(const WaveLInput &input, WaveProgressCallback progress = {});
	static WaveField GenerateFromFile(const std::string &qoePath, WaveProgressCallback progress = {});
	static WaveField ImportFromFile(const std::string &qoePath, WaveProgressCallback progress = {});

private:
	WaveSpectrum spectrum_;
};
