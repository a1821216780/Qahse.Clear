#include "WaveL/WaveKinematics.hpp"

#include <cmath>

namespace wavel_math
{
double DegToRad(double value)
{
	return value * kPi / 180.0;
}

double Clamp(double value, double lower, double upper)
{
	if (value < lower)
		return lower;
	if (value > upper)
		return upper;
	return value;
}

double SolveDispersion(double omega, double gravity, double depth)
{
	if (omega <= kTiny || gravity <= kTiny)
		return 0.0;
	if (depth <= kTiny)
		return omega * omega / gravity;

	double k = omega * omega / gravity;
	for (int i = 0; i < 32; ++i)
	{
		const double kd = k * depth;
		const double tanhKd = std::tanh(kd);
		const double sech = 1.0 / std::cosh(Clamp(kd, -50.0, 50.0));
		const double f = gravity * k * tanhKd - omega * omega;
		const double df = gravity * (tanhKd + k * depth * sech * sech);
		if (std::abs(df) <= kTiny)
			break;
		const double next = k - f / df;
		if (next <= 0.0 || !std::isfinite(next))
			break;
		if (std::abs(next - k) <= 1.0e-12 * std::max(1.0, k))
		{
			k = next;
			break;
		}
		k = next;
	}
	return k;
}

std::array<double, 3> DirectionVector(double directionRad)
{
	return {std::cos(directionRad), std::sin(directionRad), 0.0};
}

std::array<double, 3> RotateZ(const std::array<double, 3> &value, double angleDeg)
{
	const double angle = DegToRad(angleDeg);
	const double c = std::cos(angle);
	const double s = std::sin(angle);
	return {
	    value[0] * c - value[1] * s,
	    value[0] * s + value[1] * c,
	    value[2]};
}
} // namespace wavel_math
