#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Math/Vec3.h"
#include "WaveL/WaveField.hpp"
#include "WaveL/WaveL_Type.hpp"

using WaveLProgressCallback = std::function<void(const std::string &)>;

WaveLInput ReadWaveLInput(const std::string &path);

class WaveL
{
public:
	static WaveLInput ReadInputFile(const std::string &path);
	static void ValidateInputOnly(const WaveLInput &input);
	static WaveL Load(const WaveLInput &input, WaveLProgressCallback progress = {});
	static WaveL LoadFromFile(const std::string &path, WaveLProgressCallback progress = {});

	double GetElevation(Vec3 pos, double time) const;
	std::vector<double> GetElevationPerDirection(Vec3 pos,
	                                             double time,
	                                             const std::vector<double> &waveDir,
	                                             double deltaDir) const;
	void GetVelocityAndAcceleration(Vec3 pos,
	                                double time,
	                                double elevation,
	                                double depth,
	                                WaveStretching stretchingType,
	                                Vec3 *vel,
	                                Vec3 *acc,
	                                double *dynP,
	                                int isFuchs = 0,
	                                double dia = 0.0) const;
	Vec3 GetOceanCurrentAt(Vec3 position, double elevation) const;
	double GetPhaseMCFPhaseShift(double x) const;

	double ElevationAt(double x, double y, double time) const;
	SeaState StateAt(double x, double y, double z, double time) const;

	const WaveLInput &Input() const;
	const WaveField &Field() const;
	const WaveLResult &Result() const;

private:
	WaveLInput input_;
	WaveField field_;
	WaveLResult result_;
};
