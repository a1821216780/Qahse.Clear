#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

#include "../../src/Params.h"
#include "../../src/IO/ZFile.hpp"
#include "../../src/WaveL/WaveField.hpp"
#include "../../src/WaveL/WaveSpectrum.hpp"
#include "../../src/WaveL/IO/WaveL_IO_Subs.hpp"

namespace
{
	constexpr double kPi = 3.14159265358979323846;

	std::filesystem::path TestRoot()
	{
		const auto root = std::filesystem::current_path();
		const auto dir = root / "build" / "test_wave";
		std::filesystem::create_directories(dir);
		return dir;
	}

	double IntegrateSpectrum(const std::function<double(double)> &fn, double fMin, double fMax, int n = 40000)
	{
		const double df = (fMax - fMin) / static_cast<double>(n);
		double area = 0.0;
		for (int i = 0; i <= n; ++i)
		{
			const double f = fMin + df * static_cast<double>(i);
			const double w = (i == 0 || i == n) ? 0.5 : 1.0;
			area += w * fn(f) * df;
		}
		return area;
	}
}

TEST(WaveL, JONSWAPGamma1IsPM)
{
	const double f = 0.12;
	const double Hs = 3.0;
	const double Tp = 10.0;
	const double s0 = WaveSpectrum::EvaluateJonswapSpectrum(f, Hs, Tp, false, true, 1.0, 0.07, 0.09);
	const double s1 = WaveSpectrum::EvaluateJonswapSpectrum(f, Hs, Tp, false, true, 1.0, 0.07, 0.09);
	EXPECT_NEAR(s0, s1, 1.0e-12);
}

TEST(WaveL, JONSWAPIntegralMatchesHs)
{
	const double Hs = 3.0;
	const double Tp = 10.0;
	const double area = IntegrateSpectrum([&](double f) {
		return WaveSpectrum::EvaluateJonswapSpectrum(f, Hs, Tp, true, true, 0.0, 0.0, 0.0);
	}, 0.5 / Tp, 10.0 / Tp);
	EXPECT_NEAR(area, Hs * Hs / 16.0, 3.0e-3);
}

TEST(WaveL, DirectionalSpectrumNormalizesToOne)
{
	const double dirMean = 15.0;
	const double dirMax = 30.0;
	const double area = IntegrateSpectrum([&](double theta) {
		return WaveSpectrum::EvaluateDirectionalWeight(theta, dirMean, dirMax, 2.0);
	}, (dirMean - dirMax) * kPi / 180.0, (dirMean + dirMax) * kPi / 180.0);
	EXPECT_NEAR(area, 1.0, 1.0e-3);
}

TEST(WaveL, WaveNumberSatisfiesDispersion)
{
	const double omega = 2.0 * kPi / 8.0;
	const double depth = 45.0;
	const double k = WaveSpectrum::SolveWaveNumber(omega, depth);
	const double residual = GRAVITY * k * std::tanh(k * depth) - omega * omega;
	EXPECT_NEAR(residual, 0.0, 1.0e-9);
}

TEST(WaveL, RegularWaveKinematicsMatchesSurfacePressure)
{
	WaveLInput input;
	input.mode = WaveMode::GENERATE;
	input.freqSpectrum = WaveFreqSpectrum::REGULAR;
	input.Hs = 4.0;
	input.Tp = 8.0;
	input.regularPhase = kPi / 2.0;
	input.waterDepth = 50.0;
	input.outputComponents = false;
	input.outputTimeSeries = false;
	input.summaryPath.clear();

	auto wave = WaveSpectrum::Generate(input);
	const auto kin = wave.GetKinematics(0.0, 0.0, 0.0, 0.0);
	EXPECT_NEAR(kin.eta, 2.0, 1.0e-9);
	EXPECT_NEAR(kin.dynP, DENSITYWATER * GRAVITY * 2.0, 1.0e-5);
}

TEST(WaveL, WheelerStretchingChangesAboveMSLVelocity)
{
	WaveLInput base;
	base.mode = WaveMode::GENERATE;
	base.freqSpectrum = WaveFreqSpectrum::REGULAR;
	base.Hs = 4.0;
	base.Tp = 8.0;
	base.regularPhase = kPi / 2.0;
	base.waterDepth = 50.0;
	base.outputComponents = false;
	base.outputTimeSeries = false;
	base.summaryPath.clear();

	WaveLInput vertical = base;
	vertical.stretching = WaveStretching::VERTICAL;
	WaveLInput wheeler = base;
	wheeler.stretching = WaveStretching::WHEELER;

	auto verticalWave = WaveSpectrum::Generate(vertical);
	auto wheelerWave = WaveSpectrum::Generate(wheeler);
	const auto kv = verticalWave.GetKinematics(0.0, 0.0, 1.0, 0.0);
	const auto kw = wheelerWave.GetKinematics(0.0, 0.0, 1.0, 0.0);
	EXPECT_NE(kw.vel[0], kv.vel[0]);
}

TEST(WaveL, WaveFieldProvidesSurfaceAndSubmergence)
{
	WaveLInput input;
	input.mode = WaveMode::GENERATE;
	input.freqSpectrum = WaveFreqSpectrum::REGULAR;
	input.Hs = 4.0;
	input.Tp = 8.0;
	input.regularPhase = kPi / 2.0;
	input.waterDepth = 50.0;
	input.outputComponents = false;
	input.outputTimeSeries = false;
	input.summaryPath.clear();

	auto field = WaveField::Generate(input);
	const double eta = field.GetFreeSurfaceZ(0.0, 0.0, 0.0);
	EXPECT_NEAR(eta, 2.0, 1.0e-9);
	EXPECT_TRUE(field.IsSubmerged(0.0, 0.0, 1.0, 0.0));
	EXPECT_FALSE(field.IsSubmerged(0.0, 0.0, 3.0, 0.0));

	const auto sample = field.SampleEnvironment(0.0, 0.0, 1.0, 0.0);
	EXPECT_TRUE(sample.submerged);
	EXPECT_NEAR(sample.freeSurfaceZ, 2.0, 1.0e-9);
	EXPECT_NEAR(sample.immersionDepth, 1.0, 1.0e-9);
	EXPECT_NEAR(sample.kinematics.eta, 2.0, 1.0e-9);
}

TEST(WaveL, ComponentsRoundTrip)
{
	const auto root = TestRoot();
	const auto qoePath = root / "roundtrip.qoe";
	const auto wvcPath = root / "roundtrip.wvc";
	const auto wvmPath = root / "roundtrip.wvm";

	WaveLInput input;
	input.mode = WaveMode::GENERATE;
	input.freqSpectrum = WaveFreqSpectrum::REGULAR;
	input.Hs = 4.0;
	input.Tp = 8.0;
	input.waterDepth = 60.0;
	input.outputComponents = true;
	input.outputTimeSeries = false;
	input.componentsPath = wvcPath.string();
	input.summaryPath = wvmPath.string();
	WriteWaveLInput(qoePath.string(), input);

	auto generated = WaveSpectrum::GenerateFromFile(qoePath.string());
	ASSERT_TRUE(ZFile::Exists(wvcPath.string()));

	WaveLInput importInput;
	importInput.mode = WaveMode::IMPORT;
	importInput.importedComponentsPath = wvcPath.string();
	importInput.summaryPath = (root / "imported.wvm").string();
	auto imported = WaveSpectrum::Import(importInput);
	ASSERT_EQ(imported.GetWaveTrains().size(), generated.GetWaveTrains().size());
	ASSERT_FALSE(imported.GetWaveTrains().empty());
	EXPECT_NEAR(imported.GetWaveTrains().front().amplitude, generated.GetWaveTrains().front().amplitude, 1.0e-12);
	EXPECT_NEAR(imported.GetWaveTrains().front().omega, generated.GetWaveTrains().front().omega, 1.0e-12);
}

TEST(WaveL, CacheRoundTripPreservesKinematics)
{
	const auto root = TestRoot();
	const auto cachePath = root / "cache_roundtrip.wfc";
	const auto metadataPath = root / "cache_roundtrip.wfm";
	const auto summaryPath = root / "cache_roundtrip.wvm";

	WaveLInput input;
	input.mode = WaveMode::GENERATE;
	input.freqSpectrum = WaveFreqSpectrum::JONSWAP;
	input.Hs = 3.2;
	input.Tp = 9.0;
	input.waterDepth = 55.0;
	input.numFreqBins = 20;
	input.numDirBins = 1;
	input.randomSeed = 20260508;
	input.outputComponents = false;
	input.outputTimeSeries = false;
	input.outputKinematicsGrid = false;
	input.cachePath = cachePath.string();
	input.metadataPath = metadataPath.string();
	input.summaryPath = summaryPath.string();

	auto generated = WaveField::Generate(input);
	ASSERT_TRUE(ZFile::Exists(cachePath.string()));
	ASSERT_TRUE(ZFile::Exists(metadataPath.string()));
	EXPECT_NE(std::string::npos, ZFile::ReadAllText(metadataPath.string()).find(".wfc/.wfm"));

	WaveLInput importInput;
	importInput.mode = WaveMode::IMPORT;
	importInput.importedCachePath = cachePath.string();
	importInput.metadataPath = (root / "cache_roundtrip_import.wfm").string();
	importInput.summaryPath = (root / "cache_roundtrip_import.wvm").string();

	auto imported = WaveField::Import(importInput);
	for (double t : {0.0, 1.7, 9.5, 20.0})
	{
		const auto g = generated.GetKinematics(0.0, 0.0, -10.0, t);
		const auto r = imported.GetKinematics(0.0, 0.0, -10.0, t);
		EXPECT_NEAR(r.eta, g.eta, 1.0e-9);
		EXPECT_NEAR(r.vel[0], g.vel[0], 1.0e-9);
		EXPECT_NEAR(r.vel[1], g.vel[1], 1.0e-9);
		EXPECT_NEAR(r.vel[2], g.vel[2], 1.0e-9);
		EXPECT_NEAR(r.acc[0], g.acc[0], 1.0e-9);
		EXPECT_NEAR(r.acc[1], g.acc[1], 1.0e-9);
		EXPECT_NEAR(r.acc[2], g.acc[2], 1.0e-9);
		EXPECT_NEAR(r.dynP, g.dynP, 1.0e-9);
	}
}

TEST(WaveL, SampleEnvironmentMatchesDirectEngine)
{
	WaveLInput input;
	input.mode = WaveMode::GENERATE;
	input.freqSpectrum = WaveFreqSpectrum::JONSWAP;
	input.Hs = 2.8;
	input.Tp = 8.7;
	input.waterDepth = 45.0;
	input.numFreqBins = 18;
	input.numDirBins = 1;
	input.randomSeed = 12345;
	input.outputComponents = false;
	input.outputTimeSeries = false;
	input.summaryPath.clear();
	input.metadataPath.clear();
	input.cachePath.clear();
	input.gridNX = 1;
	input.gridNY = 1;
	input.gridNZ = 16;
	input.gridDX = 0.0;
	input.gridDY = 0.0;
	input.gridDZ = 3.0;

	auto field = WaveField::Generate(input);
	const double x = 0.0;
	const double y = 0.0;
	const double z = -12.0;
	const double t = 5.0;
	const auto sample = field.SampleEnvironment(x, y, z, t);
	const auto direct = wavel_detail::WaveKinematicsEngine::Evaluate(field.GetWaveTrains(), field.GetInput(), x, y, z, t);
	EXPECT_NEAR(sample.kinematics.eta, direct.eta, 1.0e-8);
	EXPECT_NEAR(sample.kinematics.vel[0], direct.vel[0], 1.0e-8);
	EXPECT_NEAR(sample.kinematics.vel[1], direct.vel[1], 1.0e-8);
	EXPECT_NEAR(sample.kinematics.vel[2], direct.vel[2], 1.0e-8);
	EXPECT_NEAR(sample.kinematics.acc[0], direct.acc[0], 1.0e-8);
	EXPECT_NEAR(sample.kinematics.acc[1], direct.acc[1], 1.0e-8);
	EXPECT_NEAR(sample.kinematics.acc[2], direct.acc[2], 1.0e-8);
	EXPECT_NEAR(sample.kinematics.dynP, direct.dynP, 1.0e-8);
	EXPECT_NEAR(sample.freeSurfaceZ, direct.eta, 1.0e-8);
}

TEST(WaveL, UserTimeseriesDominantFrequencyRecovered)
{
	const auto root = TestRoot();
	const auto path = root / "user_series.wts";
	const double dt = 0.1;
	const double period = 8.0;
	std::vector<double> times;
	std::vector<double> eta;
	for (int i = 0; i <= 400; ++i)
	{
		const double t = static_cast<double>(i) * dt;
		times.push_back(t);
		eta.push_back(1.5 * std::sin(2.0 * kPi * t / period));
	}
	WriteWaveTimeSeriesFile(path.string(), dt, times, eta);

	WaveLInput input;
	input.mode = WaveMode::GENERATE;
	input.freqSpectrum = WaveFreqSpectrum::USER_TIMESERIES;
	input.importedTimeSeriesPath = path.string();
	input.timeStep = dt;
	input.simDuration = times.back();
	input.waterDepth = 50.0;
	input.outputComponents = false;
	input.outputTimeSeries = false;
	input.summaryPath.clear();

	auto wave = WaveSpectrum::Generate(input);
	ASSERT_FALSE(wave.GetWaveTrains().empty());
	EXPECT_NEAR(wave.GetFp(), 1.0 / period, 2.0e-2);
}

TEST(WaveL, UserTimeseriesSingleSineReconstructsPhase)
{
	const auto root = TestRoot();
	const auto path = root / "user_series_phase.wts";
	const double dt = 0.25;
	const double period = 6.0;
	std::vector<double> times;
	std::vector<double> eta;
	for (int i = 0; i <= 48; ++i)
	{
		const double t = static_cast<double>(i) * dt;
		times.push_back(t);
		eta.push_back(1.2 * std::sin(2.0 * kPi * t / period));
	}
	WriteWaveTimeSeriesFile(path.string(), dt, times, eta);

	WaveLInput input;
	input.mode = WaveMode::GENERATE;
	input.freqSpectrum = WaveFreqSpectrum::USER_TIMESERIES;
	input.importedTimeSeriesPath = path.string();
	input.timeStep = dt;
	input.simDuration = times.back();
	input.waterDepth = 50.0;
	input.outputComponents = false;
	input.outputTimeSeries = false;
	input.summaryPath.clear();

	auto wave = WaveSpectrum::Generate(input);
	for (double t : {0.25, 1.5, 2.75, 4.0, 5.5})
		EXPECT_NEAR(wave.GetElevation(0.0, 0.0, t), 1.2 * std::sin(2.0 * kPi * t / period), 1.0e-6);
}
