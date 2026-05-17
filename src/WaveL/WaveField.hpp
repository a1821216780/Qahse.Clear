#pragma once

#include <vector>

#include "Math/Vec3.h"
#include "WaveL/WaveL_Type.hpp"

class WaveField
{
public:
	WaveField() = default;
	explicit WaveField(const WaveLInput &input, std::vector<WaveComponent> components = {});

	void SetInput(const WaveLInput &input);
	void SetComponents(std::vector<WaveComponent> components);
	const WaveLInput &Input() const;
	const std::vector<WaveComponent> &Components() const;
	bool Empty() const;

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
	void WaveKinematicsAt(double x,
	                      double y,
	                      double z,
	                      double time,
	                      double elevation,
	                      std::array<double, 3> &velocity,
	                      std::array<double, 3> &acceleration,
	                      double &dynamicPressure) const;
	std::array<double, 3> CurrentAt(double z, double elevation) const;
	SeaState StateAt(double x, double y, double z, double time) const;

private:
	WaveLInput input_;
	std::vector<WaveComponent> components_;
};
