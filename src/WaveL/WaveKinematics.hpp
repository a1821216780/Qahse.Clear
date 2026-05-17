#pragma once

#include <array>

namespace wavel_math
{
	constexpr double kPi = 3.141592653589793238462643383279502884;
	constexpr double kTiny = 1.0e-12;

	double DegToRad(double value);
	double Clamp(double value, double lower, double upper);
	double SolveDispersion(double omega, double gravity, double depth);
	std::array<double, 3> DirectionVector(double directionRad);
	std::array<double, 3> RotateZ(const std::array<double, 3> &value, double angleDeg);
}
