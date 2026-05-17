#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "WaveL/WaveL.hpp"

namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;

std::filesystem::path TempPath(const std::string &name)
{
	auto path = std::filesystem::temp_directory_path() / "Qahse_WaveL_Tests";
	std::filesystem::create_directories(path);
	return path / name;
}
} // namespace

TEST(WaveL_Runtime, RegularWaveSamplesElevationAndVelocity)
{
	WaveLInput input;
	input.waveType = WaveType::REGULAR;
	input.waterDepth = 87.0;
	input.gravity = 9.81;
	input.waveStretching = WaveStretching::WHEELER;
	input.hs = 6.0;
	input.tp = 10.0;
	input.waveDirMean = 0.0;

	const auto wave = WaveL::Load(input);
	const auto state = wave.StateAt(0.0, 0.0, -10.0, 0.0);

	EXPECT_EQ(wave.Result().components.size(), 1u);
	EXPECT_NEAR(state.elevation, 0.0, 1.0e-12);
	EXPECT_NEAR(state.waveVelocity[1], 0.0, 1.0e-12);
	EXPECT_TRUE(std::isfinite(state.waveVelocity[0]));
	EXPECT_TRUE(std::isfinite(state.waveAcceleration[2]));
}

TEST(WaveL_Runtime, QBladeNamedApisMatchLegacyConvenienceApis)
{
	WaveLInput input;
	input.waveType = WaveType::REGULAR;
	input.waterDepth = 87.0;
	input.gravity = 9.81;
	input.waveStretching = WaveStretching::WHEELER;
	input.hs = 6.0;
	input.tp = 10.0;
	input.constCurrent = 0.25;
	input.shearCurrent = 0.1;
	input.shearCurrentDepth = 25.0;

	const auto wave = WaveL::Load(input);
	const Vec3 pos(4.0, 0.0, -10.0);
	const double time = 2.0;
	const double elevation = wave.GetElevation(pos, time);
	EXPECT_NEAR(elevation, wave.ElevationAt(pos.x, pos.y, time), 1.0e-12);

	std::array<double, 3> legacyVelocity{};
	std::array<double, 3> legacyAcceleration{};
	double legacyDynP = 0.0;
	wave.Field().WaveKinematicsAt(pos.x,
	                               pos.y,
	                               pos.z,
	                               time,
	                               elevation,
	                               legacyVelocity,
	                               legacyAcceleration,
	                               legacyDynP);

	Vec3 velocity;
	Vec3 acceleration;
	double dynP = 0.0;
	wave.GetVelocityAndAcceleration(pos,
	                                time,
	                                elevation,
	                                input.waterDepth,
	                                input.waveStretching,
	                                &velocity,
	                                &acceleration,
	                                &dynP,
	                                0,
	                                0.0);

	EXPECT_NEAR(velocity.x, legacyVelocity[0], 1.0e-12);
	EXPECT_NEAR(velocity.y, legacyVelocity[1], 1.0e-12);
	EXPECT_NEAR(velocity.z, legacyVelocity[2], 1.0e-12);
	EXPECT_NEAR(acceleration.x, legacyAcceleration[0], 1.0e-12);
	EXPECT_NEAR(acceleration.y, legacyAcceleration[1], 1.0e-12);
	EXPECT_NEAR(acceleration.z, legacyAcceleration[2], 1.0e-12);
	EXPECT_NEAR(dynP, legacyDynP, 1.0e-12);

	const auto state = wave.StateAt(pos.x, pos.y, pos.z, time);
	const Vec3 current = wave.GetOceanCurrentAt(pos, elevation);
	EXPECT_NEAR(current.x, state.currentVelocity[0], 1.0e-12);
	EXPECT_NEAR(current.y, state.currentVelocity[1], 1.0e-12);
	EXPECT_NEAR(current.z, state.currentVelocity[2], 1.0e-12);
}

TEST(WaveL_Runtime, RegularWaveMatchesAiryTheory)
{
	WaveLInput input;
	input.waveType = WaveType::REGULAR;
	input.waterDepth = 87.0;
	input.gravity = 9.81;
	input.waveStretching = WaveStretching::NO_STRETCHING;
	input.hs = 6.0;
	input.tp = 10.0;
	input.waveDirMean = 30.0;

	const auto wave = WaveL::Load(input);
	ASSERT_EQ(wave.Result().components.size(), 1u);
	const auto &component = wave.Result().components.front();

	const Vec3 pos(3.0, 2.0, -10.0);
	const double time = 2.5;
	const double dirX = std::cos(component.direction);
	const double dirY = std::sin(component.direction);
	const double projected = pos.x * dirX + pos.y * dirY;
	const double theta = component.wavenumber * projected - component.omega * time + component.phase;
	const double sinhKd = std::sinh(component.wavenumber * input.waterDepth);
	const double depthXY = std::cosh(component.wavenumber * (pos.z + input.waterDepth)) / sinhKd;
	const double depthZ = std::sinh(component.wavenumber * (pos.z + input.waterDepth)) / sinhKd;
	const double aOmega = component.amplitude * component.omega;
	const double aOmega2 = aOmega * component.omega;

	Vec3 velocity;
	Vec3 acceleration;
	double dynP = 0.0;
	const double elevation = wave.GetElevation(pos, time);
	wave.GetVelocityAndAcceleration(pos,
	                                time,
	                                elevation,
	                                input.waterDepth,
	                                input.waveStretching,
	                                &velocity,
	                                &acceleration,
	                                &dynP);

	EXPECT_NEAR(elevation, component.amplitude * std::sin(theta), 1.0e-12);
	EXPECT_NEAR(velocity.x, aOmega * dirX * depthXY * std::sin(theta), 1.0e-12);
	EXPECT_NEAR(velocity.y, aOmega * dirY * depthXY * std::sin(theta), 1.0e-12);
	EXPECT_NEAR(velocity.z, -aOmega * depthZ * std::cos(theta), 1.0e-12);
	EXPECT_NEAR(acceleration.x, -aOmega2 * dirX * depthXY * std::cos(theta), 1.0e-12);
	EXPECT_NEAR(acceleration.y, -aOmega2 * dirY * depthXY * std::cos(theta), 1.0e-12);
	EXPECT_NEAR(acceleration.z, -aOmega2 * depthZ * std::sin(theta), 1.0e-12);
	EXPECT_NEAR(dynP,
	            std::tanh(component.wavenumber * input.waterDepth) *
	                depthXY * component.amplitude * std::sin(theta),
	            1.0e-12);
}

TEST(WaveL_Runtime, ElevationPerDirectionBinsLikeQBlade)
{
	WaveLInput input;
	input.waveType = WaveType::REGULAR;
	input.waterDepth = 87.0;
	input.gravity = 9.81;
	input.hs = 6.0;
	input.tp = 10.0;
	input.waveDirMean = 0.0;

	const auto wave = WaveL::Load(input);
	const Vec3 pos(0.0, 0.0, 0.0);
	const double time = 2.0;
	const auto elevation = wave.GetElevationPerDirection(pos, time, {0.0, 90.0}, 10.0);

	ASSERT_EQ(elevation.size(), 2u);
	EXPECT_NEAR(elevation[0], wave.GetElevation(pos, time), 1.0e-12);
	EXPECT_NEAR(elevation[1], 0.0, 1.0e-12);
}

TEST(WaveL_Runtime, McCamyFuchsCorrectionOnlyChangesAcceleration)
{
	WaveLInput input;
	input.waveType = WaveType::REGULAR;
	input.waterDepth = 87.0;
	input.gravity = 9.81;
	input.waveStretching = WaveStretching::WHEELER;
	input.hs = 6.0;
	input.tp = 10.0;

	const auto wave = WaveL::Load(input);
	const Vec3 pos(3.0, 0.0, -10.0);
	const double time = 2.0;
	const double elevation = wave.GetElevation(pos, time);

	Vec3 velocity;
	Vec3 acceleration;
	double dynP = 0.0;
	wave.GetVelocityAndAcceleration(pos,
	                                time,
	                                elevation,
	                                input.waterDepth,
	                                input.waveStretching,
	                                &velocity,
	                                &acceleration,
	                                &dynP,
	                                0,
	                                0.0);

	Vec3 fuchsVelocity;
	Vec3 fuchsAcceleration;
	double fuchsDynP = 0.0;
	wave.GetVelocityAndAcceleration(pos,
	                                time,
	                                elevation,
	                                input.waterDepth,
	                                input.waveStretching,
	                                &fuchsVelocity,
	                                &fuchsAcceleration,
	                                &fuchsDynP,
	                                1,
	                                6.0);

	EXPECT_NEAR(fuchsVelocity.x, velocity.x, 1.0e-12);
	EXPECT_NEAR(fuchsVelocity.y, velocity.y, 1.0e-12);
	EXPECT_NEAR(fuchsVelocity.z, velocity.z, 1.0e-12);
	EXPECT_NEAR(fuchsDynP, dynP, 1.0e-12);
	EXPECT_GT(std::abs(fuchsAcceleration.x - acceleration.x) +
	              std::abs(fuchsAcceleration.z - acceleration.z),
	          1.0e-7);

	EXPECT_THROW(wave.GetVelocityAndAcceleration(pos,
	                                             time,
	                                             elevation,
	                                             input.waterDepth,
	                                             input.waveStretching,
	                                             &fuchsVelocity,
	                                             &fuchsAcceleration,
	                                             &fuchsDynP,
	                                             1,
	                                             0.0),
	             std::runtime_error);
}

TEST(WaveL_Runtime, McCamyFuchsPhaseShiftMatchesQBladeTable)
{
	WaveLInput input;
	input.waveType = WaveType::REGULAR;
	input.waterDepth = 87.0;
	input.gravity = 9.81;
	input.hs = 6.0;
	input.tp = 10.0;

	const auto wave = WaveL::Load(input);
	EXPECT_NEAR(wave.GetPhaseMCFPhaseShift(0.314159), -443.14 / 180.0 * kPi, 1.0e-12);
	EXPECT_NEAR(wave.GetPhaseMCFPhaseShift(157.0796), 0.02 / 180.0 * kPi, 1.0e-12);
	const double mid = 0.5 * (0.314159 + 0.320571);
	const double expected = 0.5 * (-443.14 + -431.77) / 180.0 * kPi;
	EXPECT_NEAR(wave.GetPhaseMCFPhaseShift(mid), expected, 1.0e-10);
}

TEST(WaveL_Runtime, ComponentFileRoundTripsThroughProvider)
{
	const auto path = TempPath("components.dat");
	{
		std::ofstream out(path);
		out << "0.1 2.0 90.0 0.0\n";
	}

	WaveLInput input;
	input.waveType = WaveType::COMPONENT_FILE;
	input.waterDepth = 100.0;
	input.gravity = 9.81;
	input.componentFilePath = path.string();

	const auto wave = WaveL::Load(input);
	EXPECT_EQ(wave.Result().components.size(), 1u);
	EXPECT_NEAR(wave.ElevationAt(0.0, 0.0, 0.0), 2.0, 1.0e-9);
}

TEST(WaveL_Runtime, JonswapGenerationCreatesFiniteComponents)
{
	WaveLInput input;
	input.waveType = WaveType::JONSWAP;
	input.waterDepth = 87.0;
	input.gravity = 9.81;
	input.hs = 6.0;
	input.tp = 10.0;
	input.waveFreqNum = 16;
	input.waveDirNum = 1;
	input.seed = 12345;

	const auto wave = WaveL::Load(input);
	ASSERT_EQ(wave.Result().components.size(), 16u);
	const auto state = wave.StateAt(0.0, 0.0, -5.0, 12.0);
	EXPECT_TRUE(std::isfinite(state.elevation));
	EXPECT_TRUE(std::isfinite(state.waterVelocity[0]));
}

TEST(WaveL_Runtime, TorsethaugenAndOchiHubbleCreateFiniteComponents)
{
	for (const auto type : {WaveType::TORSETHAUGEN, WaveType::OCHI_HUBBLE})
	{
		WaveLInput input;
		input.waveType = type;
		input.waterDepth = 87.0;
		input.gravity = 9.81;
		input.hs = 6.0;
		input.tp = 10.0;
		input.waveFreqNum = 16;
		input.waveDirNum = 2;
		input.waveDirMax = 20.0;
		input.seed = 12345;

		const auto wave = WaveL::Load(input);
		ASSERT_GE(wave.Result().components.size(), 16u);
		const auto state = wave.StateAt(0.0, 0.0, -5.0, 12.0);
		EXPECT_TRUE(std::isfinite(state.elevation));
		EXPECT_TRUE(std::isfinite(state.waterVelocity[0]));
	}
}
