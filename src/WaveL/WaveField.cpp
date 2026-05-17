#include "WaveL/WaveField.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "WaveL/WaveKinematics.hpp"

namespace
{
using wavel_math::kTiny;
using wavel_math::kPi;

constexpr double kMcfRatio[] = {
	0.314159, 0.320571, 0.327249, 0.334212, 0.341477, 0.349066, 0.356999, 0.365301,
	0.373999, 0.383121, 0.392699, 0.402768, 0.413367, 0.42454, 0.436332, 0.448799,
	0.461999, 0.475999, 0.490874, 0.506708, 0.523599, 0.541654, 0.560999, 0.581776,
	0.604152, 0.628319, 0.654498, 0.682955, 0.713998, 0.747998, 0.785398, 0.826735,
	0.872665, 0.923998, 0.981748, 1.047198, 1.121997, 1.208305, 1.308997, 1.427997,
	1.570796, 1.745329, 1.963495, 2.243995, 2.617994, 3.141593, 3.205707, 3.272492,
	3.34212, 3.414775, 3.490659, 3.569992, 3.653015, 3.739991, 3.831211, 3.926991,
	4.027683, 4.133675, 4.245395, 4.363323, 4.48799, 4.619989, 4.759989, 4.908739,
	5.067085, 5.235988, 5.416539, 5.609987, 5.817764, 6.041524, 6.283185, 6.544985,
	6.829549, 7.139983, 7.479983, 7.853982, 8.267349, 8.726646, 9.239978, 9.817477,
	10.47198, 11.21997, 12.08305, 13.08997, 14.27997, 15.70796, 17.45329, 19.63495,
	22.43995, 26.17994, 31.41593, 39.26991, 52.35988, 78.53982, 157.0796};

constexpr double kMcfPhaseDeg[] = {
	-443.14, -431.77, -420.39, -409.03, -397.66, -386.31, -374.96, -363.61,
	-352.27, -340.94, -329.62, -318.3, -306.99, -295.7, -284.21, -273.13,
	-261.87, -250.62, -239.38, -228.16, -216.97, -205.78, -194.62, -183.48,
	-173.03, -161.95, -150.91, -139.91, -128.95, -118.05, -107.20, -96.43,
	-85.74, -75.17, -64.67, -54.34, -44.10, -34.26, -24.62, -15.33, -6.53,
	1.61, 8.86, 14.83, 18.97, 20.54, 20.52, 20.47, 20.39, 20.26, 20.10,
	19.91, 19.68, 19.41, 19.11, 18.77, 18.39, 17.98, 17.54, 17.07, 16.56,
	16.03, 15.47, 14.48, 14.27, 13.64, 13.00, 12.34, 11.67, 11.00, 10.32,
	9.64, 8.96, 8.29, 7.63, 6.98, 6.35, 5.74, 5.15, 4.59, 4.05, 3.54,
	3.06, 2.61, 2.20, 1.82, 1.47, 1.16, 0.89, 0.65, 0.45, 0.29, 0.16,
	0.07, 0.02};

constexpr std::size_t kMcfTableSize = sizeof(kMcfRatio) / sizeof(kMcfRatio[0]);
static_assert(kMcfTableSize == sizeof(kMcfPhaseDeg) / sizeof(kMcfPhaseDeg[0]));

void Add(std::array<double, 3> &lhs, const std::array<double, 3> &rhs)
{
	for (std::size_t i = 0; i < lhs.size(); ++i)
		lhs[i] += rhs[i];
}

std::array<double, 3> ToArray(const Vec3 &value)
{
	return {value.x, value.y, value.z};
}

} // namespace

WaveField::WaveField(const WaveLInput &input, std::vector<WaveComponent> components)
	: input_(input),
	  components_(std::move(components))
{
}

void WaveField::SetInput(const WaveLInput &input)
{
	input_ = input;
}

void WaveField::SetComponents(std::vector<WaveComponent> components)
{
	components_ = std::move(components);
}

const WaveLInput &WaveField::Input() const
{
	return input_;
}

const std::vector<WaveComponent> &WaveField::Components() const
{
	return components_;
}

bool WaveField::Empty() const
{
	return components_.empty();
}

double WaveField::GetElevation(Vec3 pos, double time) const
{
	double elevation = 0.0;
	for (const auto &component : components_)
	{
		const double projected = pos.x * std::cos(component.direction) + pos.y * std::sin(component.direction);
		const double phase = component.wavenumber * projected -
		                     component.omega * (time + input_.timeOffset) +
		                     component.phase;
		elevation += component.amplitude * std::sin(phase);
	}
	return elevation;
}

std::vector<double> WaveField::GetElevationPerDirection(Vec3 pos,
                                                        double time,
                                                        const std::vector<double> &waveDir,
                                                        double deltaDir) const
{
	std::vector<double> elevation(waveDir.size(), 0.0);
	for (const auto &component : components_)
	{
		const double projected = pos.x * std::cos(component.direction) + pos.y * std::sin(component.direction);
		double direction = component.direction * 180.0 / kPi;
		while (direction < 0.0)
			direction += 360.0;

		for (std::size_t i = 0; i < waveDir.size(); ++i)
		{
			if (direction >= waveDir[i] - deltaDir / 2.0 && direction < waveDir[i] + deltaDir / 2.0)
			{
				const double phase = component.wavenumber * projected -
				                     component.omega * (time + input_.timeOffset) +
				                     component.phase;
				elevation[i] += component.amplitude * std::sin(phase);
			}
		}
	}
	return elevation;
}

double WaveField::ElevationAt(double x, double y, double time) const
{
	return GetElevation(Vec3(x, y, 0.0), time);
}

void WaveField::GetVelocityAndAcceleration(Vec3 pos,
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
	if (!vel && !acc)
		return;
	if (isFuchs != 0 && dia <= kTiny)
		throw std::runtime_error("WaveL McCamy-Fuchs correction requires positive cylinder diameter.");
	if (depth <= kTiny)
		return;
	if (pos.z + depth < 0.0)
		return;

	double evalZ = pos.z;
	double freeSurface = elevation;
	if (stretchingType == WaveStretching::NO_STRETCHING)
		freeSurface = 0.0;
	if (evalZ > freeSurface)
		return;

	if (stretchingType == WaveStretching::VERTICAL)
	{
		if (evalZ > 0.0)
			evalZ = 0.0;
	}
	else if (stretchingType == WaveStretching::WHEELER)
	{
		evalZ = (evalZ - elevation) * depth / (elevation + depth);
	}

	if (evalZ + depth < 0.0)
		return;

	for (const auto &component : components_)
	{
		const double k = component.wavenumber;
		const double projected = pos.x * std::cos(component.direction) + pos.y * std::sin(component.direction);
		const double phase = k * projected - component.omega * (time + input_.timeOffset) + component.phase;
		const double sinPhase = std::sin(phase);
		const double cosPhase = std::cos(phase);
		const double aOmega = component.amplitude * component.omega;
		const double aOmega2 = aOmega * component.omega;
		const double dirX = std::cos(component.direction);
		const double dirY = std::sin(component.direction);

		double depthVarXY = 0.0;
		double depthVarZ = 0.0;
		if (depth > 100.0)
		{
			depthVarXY = std::exp(k * evalZ);
			depthVarZ = depthVarXY;
			if (stretchingType == WaveStretching::EXTRAPOLATION && evalZ > 0.0)
			{
				depthVarXY = 1.0 + k * evalZ;
				depthVarZ = depthVarXY;
			}
		}
		else
		{
			const double sinhKd = std::sinh(k * depth);
			if (std::abs(sinhKd) <= kTiny)
				continue;
			depthVarXY = std::cosh(k * (evalZ + depth)) / sinhKd;
			depthVarZ = std::sinh(k * (evalZ + depth)) / sinhKd;
			if (stretchingType == WaveStretching::EXTRAPOLATION && evalZ > 0.0)
			{
				depthVarXY = std::cosh(k * depth) / sinhKd + evalZ * k;
				depthVarZ = 1.0 + evalZ * k * std::cosh(k * depth) / sinhKd;
			}
		}

		if (vel)
		{
			vel->x += aOmega * dirX * depthVarXY * sinPhase;
			vel->y += aOmega * dirY * depthVarXY * sinPhase;
			vel->z += -aOmega * depthVarZ * cosPhase;
		}

		if (acc)
		{
			double accCosPhase = cosPhase;
			double accSinPhase = sinPhase;
			double accFactor = 1.0;
			if (isFuchs != 0)
			{
				accFactor = std::min(
					1.05 * std::tanh(depth * k) /
						std::pow(std::pow(std::abs(dia * k / 2.0 - 0.2), 2.2) + 1.0, 0.85),
					1.0);
				const double phaseShift = GetPhaseMCFPhaseShift(2.0 * kPi / (dia * k));
				accCosPhase = std::cos(phase + phaseShift);
				accSinPhase = std::sin(phase + phaseShift);
			}
			acc->x += -aOmega2 * dirX * depthVarXY * accCosPhase * accFactor;
			acc->y += -aOmega2 * dirY * depthVarXY * accCosPhase * accFactor;
			acc->z += -aOmega2 * depthVarZ * accSinPhase * accFactor;
		}

		if (dynP)
			*dynP += std::tanh(k * depth) * depthVarXY * component.amplitude * sinPhase;
	}
}

void WaveField::WaveKinematicsAt(double x,
                                 double y,
                                 double z,
                                 double time,
                                 double elevation,
                                 std::array<double, 3> &velocity,
                                 std::array<double, 3> &acceleration,
                                 double &dynamicPressure) const
{
	Vec3 vel;
	Vec3 acc;
	dynamicPressure = 0.0;
	GetVelocityAndAcceleration(Vec3(x, y, z),
	                           time,
	                           elevation,
	                           input_.waterDepth,
	                           input_.waveStretching,
	                           &vel,
	                           &acc,
	                           &dynamicPressure,
	                           0,
	                           0.0);
	velocity = ToArray(vel);
	acceleration = ToArray(acc);
}

Vec3 WaveField::GetOceanCurrentAt(Vec3 position, double elevation) const
{
	if (input_.waterDepth <= kTiny)
		return Vec3(0.0, 0.0, 0.0);
	double z = position.z;
	if (z + input_.waterDepth < 0.0)
		return Vec3(0.0, 0.0, 0.0);
	if (input_.waveStretching == WaveStretching::NO_STRETCHING && z > 0.0)
		return Vec3(0.0, 0.0, 0.0);
	if (input_.waveStretching != WaveStretching::NO_STRETCHING && z > elevation)
		return Vec3(0.0, 0.0, 0.0);

	double evalZ = z;
	double shearExtrapolation = 0.0;
	if (input_.waveStretching == WaveStretching::VERTICAL)
	{
		if (evalZ > 0.0)
			evalZ = 0.0;
	}
	else if (input_.waveStretching == WaveStretching::WHEELER)
	{
		evalZ = -input_.waterDepth * (evalZ - elevation) / (elevation - input_.waterDepth);
	}
	else if (input_.waveStretching == WaveStretching::EXTRAPOLATION)
	{
		if (input_.shearCurrentDepth > kTiny)
			shearExtrapolation = evalZ * input_.shearCurrent / input_.shearCurrentDepth;
		evalZ = 0.0;
	}
	if (evalZ + input_.waterDepth < 0.0)
		return Vec3(0.0, 0.0, 0.0);

	Vec3 result;
	const auto constant = wavel_math::RotateZ({input_.constCurrent, 0.0, 0.0}, input_.constCurrentDir);
	result += Vec3(constant[0], constant[1], constant[2]);

	if (input_.shearCurrentDepth > kTiny && -evalZ < input_.shearCurrentDepth)
	{
		const double speed = (input_.shearCurrentDepth + evalZ) / input_.shearCurrentDepth * input_.shearCurrent + shearExtrapolation;
		const auto current = wavel_math::RotateZ({speed, 0.0, 0.0}, input_.shearCurrentDir);
		result += Vec3(current[0], current[1], current[2]);
	}

	const double profileBase = std::max((input_.waterDepth + evalZ) / input_.waterDepth, 0.0);
	const double profileSpeed = std::pow(profileBase, input_.profileCurrentExponent) * input_.profileCurrent;
	const auto profile = wavel_math::RotateZ({profileSpeed, 0.0, 0.0}, input_.profileCurrentDir);
	result += Vec3(profile[0], profile[1], profile[2]);
	return result;
}

std::array<double, 3> WaveField::CurrentAt(double z, double elevation) const
{
	return ToArray(GetOceanCurrentAt(Vec3(0.0, 0.0, z), elevation));
}

double WaveField::GetPhaseMCFPhaseShift(double x) const
{
	if (x < kMcfRatio[0])
		return kMcfPhaseDeg[0] / 180.0 * kPi;
	if (x > kMcfRatio[kMcfTableSize - 1])
		return kMcfPhaseDeg[kMcfTableSize - 1] / 180.0 * kPi;

	for (std::size_t i = 0; i + 1 < kMcfTableSize; ++i)
	{
		if (x >= kMcfRatio[i] && x <= kMcfRatio[i + 1])
		{
			const double phaseDeg = kMcfPhaseDeg[i] +
			                        (kMcfPhaseDeg[i + 1] - kMcfPhaseDeg[i]) /
			                            (kMcfRatio[i + 1] - kMcfRatio[i]) *
			                            (x - kMcfRatio[i]);
			return phaseDeg / 180.0 * kPi;
		}
	}

	return 0.0;
}

SeaState WaveField::StateAt(double x, double y, double z, double time) const
{
	SeaState state;
	const Vec3 pos(x, y, z);
	state.elevation = GetElevation(pos, time);

	Vec3 waveVelocity;
	Vec3 waveAcceleration;
	state.dynamicPressure = 0.0;
	GetVelocityAndAcceleration(pos,
	                           time,
	                           state.elevation,
	                           input_.waterDepth,
	                           input_.waveStretching,
	                           &waveVelocity,
	                           &waveAcceleration,
	                           &state.dynamicPressure,
	                           0,
	                           0.0);

	const Vec3 current = GetOceanCurrentAt(pos, state.elevation);
	state.waveVelocity = ToArray(waveVelocity);
	state.waveAcceleration = ToArray(waveAcceleration);
	state.currentVelocity = ToArray(current);
	state.waterVelocity = state.waveVelocity;
	Add(state.waterVelocity, state.currentVelocity);
	return state;
}
