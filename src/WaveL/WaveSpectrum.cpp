#include "WaveL/WaveSpectrum.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <filesystem>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <fftw/fftw3.h>

#include "../Params.h"
#include "WaveL/IO/LocaleString_WaveL.hpp"
#include "WaveL/IO/WaveL_IO_Subs.hpp"

namespace
{
	constexpr int kDenseSpectrumPoints = 8192;
	constexpr double kMinPositive = 1.0e-12;
	constexpr double kDeepWaterThreshold = 20.0;

	double Square(double value)
	{
		return value * value;
	}

	double Clamp(double value, double lo, double hi)
	{
		return std::max(lo, std::min(hi, value));
	}

	std::string DefaultBaseName(const std::string &path, const char *fallback)
	{
		if (path.empty())
			return fallback;
		const auto stem = std::filesystem::path(path).stem().string();
		return stem.empty() ? fallback : stem;
	}

	std::pair<std::vector<double>, std::vector<double>> SampleSpectrumGrid(
		double fMin,
		double fMax,
		const std::function<double(double)> &spectrum)
	{
		std::vector<double> freqs(kDenseSpectrumPoints + 1);
		std::vector<double> cdf(kDenseSpectrumPoints + 1, 0.0);
		const double deltaF = (fMax - fMin) / static_cast<double>(kDenseSpectrumPoints);
		double cumulative = 0.0;
		for (int i = 0; i <= kDenseSpectrumPoints; ++i)
		{
			const double f = fMin + deltaF * static_cast<double>(i);
			freqs[static_cast<std::size_t>(i)] = f;
			if (i > 0)
				cumulative += std::max(0.0, spectrum(f)) * deltaF;
			cdf[static_cast<std::size_t>(i)] = cumulative;
		}
		return {std::move(freqs), std::move(cdf)};
	}

	double InterpolateLinear(double x, const std::vector<double> &xs, const std::vector<double> &ys)
	{
		if (xs.empty() || ys.empty() || xs.size() != ys.size())
			return 0.0;
		if (x <= xs.front())
			return ys.front();
		if (x >= xs.back())
			return ys.back();
		const auto it = std::lower_bound(xs.begin(), xs.end(), x);
		const std::size_t idx = static_cast<std::size_t>(std::distance(xs.begin(), it));
		const std::size_t i0 = idx - 1;
		const double x0 = xs[i0];
		const double x1 = xs[idx];
		const double w = (x - x0) / (x1 - x0);
		return ys[i0] + (ys[idx] - ys[i0]) * w;
	}

	double QuantileFrequency(double target, const std::vector<double> &freqs, const std::vector<double> &cdf)
	{
		if (cdf.empty())
			return 0.0;
		if (target <= 0.0)
			return freqs.front();
		if (target >= cdf.back())
			return freqs.back();
		const auto it = std::lower_bound(cdf.begin(), cdf.end(), target);
		const std::size_t idx = static_cast<std::size_t>(std::distance(cdf.begin(), it));
		if (idx == 0)
			return freqs.front();
		const double y0 = cdf[idx - 1];
		const double y1 = cdf[idx];
		const double w = (target - y0) / (y1 - y0);
		return freqs[idx - 1] + (freqs[idx] - freqs[idx - 1]) * w;
	}

	double CumulativeAt(double f, const std::vector<double> &freqs, const std::vector<double> &cdf)
	{
		return InterpolateLinear(f, freqs, cdf);
	}

	double EffectiveHsForOchi(const WaveLInput &input)
	{
		return std::sqrt(Square(input.Hs1) + Square(input.Hs2));
	}

	void ApplyAutoOchi(WaveLInput &input)
	{
		if (!input.autoOchi)
			return;
		const double totalHs = input.Hs > 0.0 ? input.Hs : EffectiveHsForOchi(input);
		input.Hs1 = 0.72 * totalHs;
		input.Hs2 = std::sqrt(std::max(0.0, totalHs * totalHs - input.Hs1 * input.Hs1));
		input.f1 = 0.6 / input.Tp;
		input.f2 = 1.6 / input.Tp;
		input.lambda1 = 2.0;
		input.lambda2 = 2.0;
	}

	std::vector<double> BuildUniformTimes(double dt, double duration)
	{
		const int n = std::max(1, static_cast<int>(std::llround(duration / dt)) + 1);
		std::vector<double> times(static_cast<std::size_t>(n));
		for (int i = 0; i < n; ++i)
			times[static_cast<std::size_t>(i)] = static_cast<double>(i) * dt;
		return times;
	}

	double SamplePeriodicSeries(double t, const WaveTimeSeriesData &series)
	{
		if (series.times.empty())
			return 0.0;
		const double start = series.times.front();
		const double end = series.times.back();
		const double span = std::max(end - start, kMinPositive);
		double local = start + std::fmod((t - start), span);
		if (local < start)
			local += span;
		if (local <= series.times.front())
			return series.elevations.front();
		if (local >= series.times.back())
			return series.elevations.back();

		const auto it = std::lower_bound(series.times.begin(), series.times.end(), local);
		const std::size_t idx = static_cast<std::size_t>(std::distance(series.times.begin(), it));
		if (idx == 0)
			return series.elevations.front();
		const double t0 = series.times[idx - 1];
		const double t1 = series.times[idx];
		const double y0 = series.elevations[idx - 1];
		const double y1 = series.elevations[idx];
		const double w = (local - t0) / (t1 - t0);
		return y0 + (y1 - y0) * w;
	}

	std::array<double, 3> CenterCacheVelocity(const wavel_detail::WaveFieldCache &cache)
	{
		if (cache.nt == 0 || cache.nx == 0 || cache.ny == 0 || cache.nz == 0)
			return {0.0, 0.0, 0.0};
		const int ix = cache.nx / 2;
		const int iy = cache.ny / 2;
		const int iz = cache.nz / 2;
		const int it = cache.nt / 2;
		const std::size_t idx = cache.Index(ix, iy, iz, it);
		return {cache.u[idx], cache.v[idx], cache.w[idx]};
	}

}

WaveSpectrum::WaveSpectrum(const WaveLInput &input, std::string sourcePath)
	: input_(input), sourcePath_(std::move(sourcePath))
{
}

void WaveSpectrum::ValidateInputOnly(const WaveLInput &input)
{
	WaveSpectrum spectrum(input);
	spectrum.ValidateInput();
}

WaveSpectrum WaveSpectrum::Generate(const WaveLInput &input, WaveProgressCallback progress)
{
	WaveSpectrum spectrum(input);
	spectrum.Generate(progress);
	return spectrum;
}

WaveSpectrum WaveSpectrum::Import(const WaveLInput &input, WaveProgressCallback progress)
{
	WaveSpectrum spectrum(input);
	std::string cachePath = input.importedCachePath;
	if (cachePath.empty() && input.importedComponentsPath.empty())
		cachePath = input.cachePath;
	if (!cachePath.empty())
	{
		spectrum.ImportCache(cachePath, progress);
		return spectrum;
	}
	std::string componentsPath = input.importedComponentsPath;
	if (componentsPath.empty())
		componentsPath = input.componentsPath;
	if (componentsPath.empty())
		throw std::runtime_error(L_WAVEL_ImportComponentsNeed);
	spectrum.ImportComponents(componentsPath, progress);
	return spectrum;
}

WaveSpectrum WaveSpectrum::GenerateFromFile(const std::string &qoePath, WaveProgressCallback progress)
{
	WaveLInput input = ReadWaveLInput(qoePath);
	if (input.mode != WaveMode::GENERATE)
		throw std::runtime_error(L_WAVEL_ModeUnsupported);
	WaveSpectrum spectrum(input, qoePath);
	spectrum.Generate(progress);
	return spectrum;
}

WaveSpectrum WaveSpectrum::ImportFromFile(const std::string &qoePath, WaveProgressCallback progress)
{
	WaveLInput input = ReadWaveLInput(qoePath);
	if (input.mode != WaveMode::IMPORT)
		throw std::runtime_error(L_WAVEL_ModeUnsupported);
	return Import(input, progress);
}

void WaveSpectrum::ValidateInput() const
{
	if (input_.mode == WaveMode::BATCH)
		throw std::runtime_error(L_WAVEL_BatchUnsupported);
	if (input_.Hs < 0.0)
		throw std::runtime_error(L_WAVEL_HsNonNegative);
	if (input_.mode == WaveMode::IMPORT)
	{
		if (input_.importedCachePath.empty() && input_.cachePath.empty() &&
			input_.importedComponentsPath.empty() && input_.componentsPath.empty())
			throw std::runtime_error("IMPORT mode requires ImportedCachePath or ImportedComponentsPath.");
		return;
	}
	if (input_.freqSpectrum != WaveFreqSpectrum::USER_TIMESERIES && input_.Tp <= 0.0)
		throw std::runtime_error(L_WAVEL_TpPositive);
	if (input_.timeStep <= 0.0)
		throw std::runtime_error(L_WAVEL_TimeStepPositive);
	if (input_.simDuration <= 0.0)
		throw std::runtime_error(L_WAVEL_DurationPositive);
	if (input_.waterDepth <= 0.0)
		throw std::runtime_error(L_WAVEL_DepthPositive);
	if (input_.numFreqBins <= 0)
		throw std::runtime_error(L_WAVEL_FreqBinsPositive);
	if (input_.numDirBins <= 0)
		throw std::runtime_error(L_WAVEL_DirBinsPositive);
	if (!input_.autoFreqRange && !(input_.fCutOut > input_.fCutIn && input_.fCutIn >= 0.0))
		throw std::runtime_error(L_WAVEL_FreqRangeInvalid);
	if (input_.freqSpectrum == WaveFreqSpectrum::USER_SPECTRUM && input_.importedSpectrumPath.empty())
		throw std::runtime_error(L_WAVEL_UserSpectrumNeeded);
	if (input_.freqSpectrum == WaveFreqSpectrum::USER_TIMESERIES && input_.importedTimeSeriesPath.empty())
		throw std::runtime_error(L_WAVEL_UserSeriesNeeded);
}

void WaveSpectrum::ResolveDerivedPaths()
{
	const std::filesystem::path baseDir = sourcePath_.empty()
		? std::filesystem::current_path()
		: std::filesystem::path(sourcePath_).parent_path();
	const std::filesystem::path resultDir = baseDir / "result";
	const std::string baseName = DefaultBaseName(sourcePath_, "WaveL");
	if (input_.cachePath.empty())
		input_.cachePath = (resultDir / (baseName + ".wfc")).string();
	if (input_.metadataPath.empty())
		input_.metadataPath = (resultDir / (baseName + ".wfm")).string();
	if (input_.componentsPath.empty() && input_.outputComponents)
		input_.componentsPath = (resultDir / (baseName + ".wvc")).string();
	if (input_.timeSeriesPath.empty() && input_.outputTimeSeries)
		input_.timeSeriesPath = (resultDir / (baseName + ".wts")).string();
	if (input_.kinematicsPath.empty() && input_.outputKinematicsGrid)
		input_.kinematicsPath = (resultDir / (baseName + "_kinematics")).string();
	wavel_io_detail::ResolvePaths(input_, sourcePath_.empty() ? (baseDir / (baseName + ".qoe")).string() : sourcePath_);
}

double WaveSpectrum::EvaluateJonswapSpectrum(double f, double Hs, double Tp, bool autoGamma, bool autoSigma, double gamma, double sigma1, double sigma2)
{
	if (f <= 0.0 || Hs <= 0.0 || Tp <= 0.0)
		return 0.0;
	const double fp = 1.0 / Tp;
	if (autoGamma)
	{
		const double ratio = Tp / std::sqrt(Hs);
		if (ratio <= 3.6)
			gamma = 5.0;
		else if (ratio <= 5.0)
			gamma = std::exp(5.75 - 1.15 * ratio);
		else
			gamma = 1.0;
	}
	const double sigma = autoSigma ? (f <= fp ? 0.07 : 0.09) : (f <= fp ? sigma1 : sigma2);
	const double fr = f / fp;
	return 0.3125 * Hs * Hs * Tp * std::pow(fr, -5.0) * std::exp(-1.25 * std::pow(fr, -4.0)) *
		(1.0 - 0.287 * std::log(std::max(gamma, 1.0))) *
		std::pow(std::max(gamma, 1.0), std::exp(-0.5 * Square((fr - 1.0) / sigma)));
}

double WaveSpectrum::EvaluateTorsethaugenSpectrum(double f, double Hs, double Tp, bool autoGamma, bool autoSigma, double gamma, double sigma1, double sigma2, bool doublePeak)
{
	if (f <= 0.0 || Hs <= 0.0 || Tp <= 0.0)
		return 0.0;

	const double a_f = 6.6;
	const double a_e = 2.0;
	const double a_u = 25.0;
	const double a_10 = 0.7;
	const double a_1 = 0.5;
	const double k_g = 35.0;
	const double b_1 = 2.0;
	const double a_20 = 0.6;
	const double a_2 = 0.3;
	const double a_3 = 6.0;

	const double Tpf = a_f * std::pow(Hs, 1.0 / 3.0);
	const double Tl = a_e * std::sqrt(Hs);
	const double Tu = a_u;
	double e_l = (Tpf - Tp) / std::max(Tpf - Tl, kMinPositive);
	double e_u = (Tp - Tpf) / std::max(Tu - Tpf, kMinPositive);
	e_l = Clamp(e_l, 0.0, 1.0);
	e_u = Clamp(e_u, 0.0, 1.0);

	double R = 0.0;
	double H1 = 0.0;
	double Tp1 = Tp;
	double Gamma1 = 1.0;
	double H2 = 0.0;
	double Tp2 = Tp;

	if (Tp <= Tpf)
	{
		R = (1.0 - a_10) * std::exp(-Square(e_l / a_1)) + a_10;
		H1 = R * Hs;
		Tp1 = Tp;
		const double steepness = 2.0 * M_PI / GRAVITY * H1 / (Tp1 * Tp1);
		Gamma1 = k_g * std::pow(steepness, 6.0 / 7.0);
		H2 = std::sqrt(std::max(0.0, 1.0 - R * R)) * Hs;
		Tp2 = Tpf + b_1;
	}
	else
	{
		R = (1.0 - a_20) * std::exp(-Square(e_u / a_2)) + a_20;
		H1 = R * Hs;
		Tp1 = Tp;
		const double steepness = 2.0 * M_PI / GRAVITY * Hs / (Tpf * Tpf);
		Gamma1 = k_g * std::pow(steepness, 6.0 / 7.0) * (1.0 + a_3 * e_u);
		H2 = std::sqrt(std::max(0.0, 1.0 - R * R)) * Hs;
		Tp2 = a_f * std::pow(std::max(H2, kMinPositive), 1.0 / 3.0);
	}

	if (!autoGamma)
		Gamma1 = gamma;

	const double sigma = autoSigma ? ((f <= 1.0 / Tp) ? 0.07 : 0.09) : ((f <= 1.0 / Tp) ? sigma1 : sigma2);
	const double G0 = 3.26;
	const double Ay = (1.0 + 1.1 * std::pow(std::log(std::max(Gamma1, 1.0)), 1.19)) / std::max(Gamma1, 1.0);
	const double f1n = f * Tp1;
	const double f2n = f * Tp2;
	const double S1 = G0 * Ay * std::pow(f1n, -4.0) * std::exp(-std::pow(f1n, -4.0)) *
		std::pow(std::max(Gamma1, 1.0), std::exp(-Square(f1n - 1.0) / (2.0 * sigma * sigma)));
	const double S2 = G0 * std::pow(f2n, -4.0) * std::exp(-std::pow(f2n, -4.0));
	return doublePeak ? (S1 + S2) : S1;
}

double WaveSpectrum::EvaluateOchiHubbleSpectrum(double f, double Hs1, double Hs2, double f1, double f2, double lambda1, double lambda2)
{
	if (f <= 0.0)
		return 0.0;
	const double w1 = f1 * 2.0 * M_PI;
	const double w2 = f2 * 2.0 * M_PI;
	const double w = f * 2.0 * M_PI;
	const double S1 = 0.25 *
		std::pow(((4.0 * lambda1 + 1.0) / 4.0) * std::pow(w1, 4.0), lambda1) /
		std::tgamma(lambda1) * Square(Hs1) / std::pow(w, 4.0 * lambda1 + 1.0) *
		std::exp(-(4.0 * lambda1 + 1.0) / 4.0 * std::pow(w1 / w, 4.0));
	const double S2 = 0.25 *
		std::pow(((4.0 * lambda2 + 1.0) / 4.0) * std::pow(w2, 4.0), lambda2) /
		std::tgamma(lambda2) * Square(Hs2) / std::pow(w, 4.0 * lambda2 + 1.0) *
		std::exp(-(4.0 * lambda2 + 1.0) / 4.0 * std::pow(w2 / w, 4.0));
	return S1 + S2;
}

double WaveSpectrum::EvaluateDirectionalWeight(double directionRad, double dirMeanDeg, double dirMaxDeg, double spreadExp)
{
	const double dirMean = dirMeanDeg * M_PI / 180.0;
	const double dirMax = dirMaxDeg * M_PI / 180.0;
	if (dirMax <= 0.0)
		return std::fabs(directionRad - dirMean) < 1.0e-10 ? 1.0 : 0.0;
	if (directionRad < dirMean - dirMax || directionRad > dirMean + dirMax)
		return 0.0;
	const double C = std::sqrt(M_PI) * std::tgamma(spreadExp + 1.0) / (2.0 * dirMax * std::tgamma(spreadExp + 0.5));
	return C * std::pow(std::fabs(std::cos(M_PI * (directionRad - dirMean) / (2.0 * dirMax))), 2.0 * spreadExp);
}

double WaveSpectrum::SolveWaveNumber(double omega, double waterDepth)
{
	if (omega <= 0.0 || waterDepth <= 0.0)
		return 0.0;
	const double deep = omega * omega / GRAVITY;
	if (waterDepth * deep > kDeepWaterThreshold)
		return deep;
	double k = deep * std::pow(1.0 - std::exp(-std::pow(omega * std::sqrt(waterDepth / GRAVITY), 2.5)), -0.4);
	k = std::max(k, deep * 0.5);
	for (int iter = 0; iter < 32; ++iter)
	{
		const double kd = k * waterDepth;
		const double tanhKd = std::tanh(kd);
		const double sech2 = 1.0 / Square(std::cosh(kd));
		const double f = GRAVITY * k * tanhKd - omega * omega;
		const double df = GRAVITY * (tanhKd + kd * sech2);
		const double delta = f / std::max(df, kMinPositive);
		k -= delta;
		if (std::fabs(delta) < 1.0e-12 * std::max(1.0, k))
			break;
	}
	return k;
}

void WaveSpectrum::Generate(WaveProgressCallback progress)
{
	ValidateInput();
	ResolveDerivedPaths();
	result_ = WaveSpectrumResult{};
	result_.warnings.clear();
	importedExternalKinematics_ = false;

	if (progress)
		progress("WaveL: generating wave spectrum");

	switch (input_.freqSpectrum)
	{
	case WaveFreqSpectrum::NONE:
		break;
	case WaveFreqSpectrum::REGULAR:
		GenerateRegular();
		break;
	case WaveFreqSpectrum::USER_SPECTRUM:
		GenerateFromUserSpectrum(progress);
		break;
	case WaveFreqSpectrum::USER_TIMESERIES:
		GenerateFromUserTimeSeries(progress);
		break;
	case WaveFreqSpectrum::JONSWAP:
	case WaveFreqSpectrum::TORSETHAUGEN:
	case WaveFreqSpectrum::OCHI_HUBBLE:
		GenerateSpectrumDriven(progress);
		break;
	}

	ApplyDirectionalDistribution();
	UpdateResultStatistics();
	BuildPrimaryCache();
	WriteOutputs(progress);
}

void WaveSpectrum::ImportComponents(const std::string &path, WaveProgressCallback progress)
{
	ValidateInput();
	ResolveDerivedPaths();
	importedExternalKinematics_ = false;
	if (progress)
		progress("WaveL: importing component file");

	const auto imported = ReadWaveComponentFile(path);
	result_ = WaveSpectrumResult{};
	result_.waveTrains = imported.waveTrains;
	result_.numComponents = static_cast<int>(imported.waveTrains.size());
	result_.significantHeight = imported.significantHeight;
	result_.peakPeriod = imported.peakPeriod;
	result_.peakFrequency = imported.peakFrequency;
	result_.zeroMoment = imported.zeroMoment;
	if (imported.waterDepth > 0.0)
		input_.waterDepth = imported.waterDepth;
	UpdateResultStatistics();
	result_.componentsFilePath = path;
	WriteOutputs(progress);
}

void WaveSpectrum::ImportCache(const std::string &path, WaveProgressCallback progress)
{
	ValidateInput();
	ResolveDerivedPaths();
	if (progress)
		progress("WaveL: importing cache file");

	const auto imported = ReadWaveCacheFile(path);
	cache_ = imported.cache;
	cacheBuilt_ = true;
	importedExternalKinematics_ = true;
	result_ = imported.result;
	result_.cacheFilePath = path;
	input_.waterDepth = imported.waterDepth;
	input_.gridNX = cache_.nx;
	input_.gridNY = cache_.ny;
	input_.gridNZ = cache_.nz;
	input_.gridDX = cache_.dx;
	input_.gridDY = cache_.dy;
	input_.gridDZ = cache_.dz;
	input_.timeStep = cache_.dt;
	input_.simDuration = cache_.dt * static_cast<double>(std::max(0, cache_.nt - 1));
	result_.warnings.clear();

	if (result_.significantHeight <= 0.0)
	{
		const int ix = cache_.nx / 2;
		const int iy = cache_.ny / 2;
		const int iz = std::max(0, cache_.nz - 1);
		double meanEta = 0.0;
		double meanSq = 0.0;
		for (int it = 0; it < cache_.nt; ++it)
		{
			const double eta = cache_.eta[cache_.Index(ix, iy, iz, it)];
			meanEta += eta;
			meanSq += eta * eta;
		}
		meanEta /= static_cast<double>(std::max(1, cache_.nt));
		meanSq /= static_cast<double>(std::max(1, cache_.nt));
		result_.significantHeight = 4.0 * std::sqrt(std::max(0.0, meanSq - meanEta * meanEta));
	}

	WriteOutputs(progress);
}

double WaveSpectrum::GetElevation(double x, double y, double time) const
{
	return GetKinematics(x, y, 0.0, time).eta;
}

double WaveSpectrum::GetFreeSurfaceZ(double x, double y, double time) const
{
	return GetElevation(x, y, time);
}

bool WaveSpectrum::IsSubmerged(double x, double y, double z, double time, double tolerance) const
{
	return z <= GetFreeSurfaceZ(x, y, time) + tolerance;
}

WaveKinematics WaveSpectrum::GetKinematics(double x, double y, double z, double time) const
{
	if (cacheBuilt_ || importedExternalKinematics_)
		return wavel_detail::WaveKinematicsEngine::EvaluateFromCache(cache_, x, y, z, time);
	return wavel_detail::WaveKinematicsEngine::Evaluate(result_.waveTrains, input_, x, y, z, time);
}

std::vector<WaveKinematics> WaveSpectrum::SampleKinematics(const std::vector<WaveSamplePoint> &points, double time) const
{
	std::vector<WaveKinematics> values;
	values.reserve(points.size());
	for (const auto &point : points)
		values.push_back(GetKinematics(point.x, point.y, point.z, time));
	return values;
}

void WaveSpectrum::GenerateRegular()
{
	if (input_.Hs <= 0.0)
		return;
	WaveTrain train;
	train.amplitude = input_.Hs * 0.5;
	train.phase = input_.regularPhase;
	train.omega = 2.0 * M_PI / input_.Tp;
	train.wavenumber = SolveWaveNumber(train.omega, input_.waterDepth);
	train.direction = input_.dirMean * M_PI / 180.0;
	train.cosDir = std::cos(train.direction);
	train.sinDir = std::sin(train.direction);
	train.A_omega = train.amplitude * train.omega;
	train.A_omega2 = train.A_omega * train.omega;
	result_.waveTrains.push_back(train);
}

void WaveSpectrum::GenerateSpectrumDriven(WaveProgressCallback progress)
{
	if (input_.Hs <= 0.0 && input_.freqSpectrum != WaveFreqSpectrum::OCHI_HUBBLE)
		return;

	ApplyAutoOchi(input_);

	double fMin = input_.fCutIn;
	double fMax = input_.fCutOut;
	if (input_.autoFreqRange || !(fMax > fMin))
	{
		fMin = 0.5 / std::max(input_.Tp, kMinPositive);
		fMax = 10.0 / std::max(input_.Tp, kMinPositive);
		if (input_.freqSpectrum == WaveFreqSpectrum::OCHI_HUBBLE)
		{
			fMin = 0.5 * input_.f1;
			fMax = 10.0 * input_.f2;
		}
	}

	std::function<double(double)> spectrumFn;
	double targetM0 = 0.0;
	switch (input_.freqSpectrum)
	{
	case WaveFreqSpectrum::JONSWAP:
		spectrumFn = [&](double f) {
			return EvaluateJonswapSpectrum(f, input_.Hs, input_.Tp, input_.autoGamma, input_.autoSigma, input_.gamma, input_.sigma1, input_.sigma2);
		};
		targetM0 = Square(input_.Hs) / 16.0;
		break;
	case WaveFreqSpectrum::TORSETHAUGEN:
		spectrumFn = [&](double f) {
			return EvaluateTorsethaugenSpectrum(f, input_.Hs, input_.Tp, input_.autoGamma, input_.autoSigma, input_.gamma, input_.sigma1, input_.sigma2, input_.doublePeak);
		};
		targetM0 = Square(input_.Hs) / 16.0;
		break;
	case WaveFreqSpectrum::OCHI_HUBBLE:
		spectrumFn = [&](double f) {
			return EvaluateOchiHubbleSpectrum(f, input_.Hs1, input_.Hs2, input_.f1, input_.f2, input_.lambda1, input_.lambda2);
		};
		targetM0 = Square(EffectiveHsForOchi(input_)) / 16.0;
		break;
	default:
		return;
	}

	const auto [freqs, cdfRaw] = SampleSpectrumGrid(fMin, fMax, spectrumFn);
	const double rawArea = cdfRaw.back();
	if (rawArea <= 0.0)
		return;
	const double scale = targetM0 > 0.0 ? targetM0 / rawArea : 1.0;
	const std::vector<double> cdf = [&]() {
		std::vector<double> scaled = cdfRaw;
		for (double &value : scaled)
			value *= scale;
		return scaled;
	}();
	const double totalArea = cdf.back();

	std::mt19937 rng(input_.randomSeed);
	std::uniform_real_distribution<double> phaseDist(0.0, 2.0 * M_PI);
	result_.waveTrains.clear();
	result_.waveTrains.reserve(static_cast<std::size_t>(input_.numFreqBins));

	if (input_.discretization == WaveDiscretization::EQUAL_FREQUENCY)
	{
		const double df = (fMax - fMin) / static_cast<double>(input_.numFreqBins);
		for (int i = 0; i < input_.numFreqBins; ++i)
		{
			const double left = fMin + df * static_cast<double>(i);
			const double right = left + df;
			const double energy = std::max(0.0, CumulativeAt(right, freqs, cdf) - CumulativeAt(left, freqs, cdf));
			if (energy <= kMinPositive)
				continue;
			WaveTrain train;
			train.amplitude = std::sqrt(2.0 * energy);
			const double frequency = 0.5 * (left + right);
			train.omega = 2.0 * M_PI * frequency;
			train.phase = phaseDist(rng);
			train.wavenumber = SolveWaveNumber(train.omega, input_.waterDepth);
			train.A_omega = train.amplitude * train.omega;
			train.A_omega2 = train.A_omega * train.omega;
			result_.waveTrains.push_back(train);
		}
	}
	else
	{
		const double energyPerBin = totalArea / static_cast<double>(input_.numFreqBins);
		double previousQuantile = 0.0;
		double previousFreq = fMin;
		for (int i = 0; i < input_.numFreqBins; ++i)
		{
			const double currentQuantile = energyPerBin * static_cast<double>(i + 1);
			const double freq = QuantileFrequency(currentQuantile, freqs, cdf);
			const double bandWidth = std::max(freq - previousFreq, 0.0);
			int splitCount = 1;
			if (input_.dfMax > 0.0 && bandWidth > input_.dfMax)
				splitCount = std::max(1, static_cast<int>(std::ceil(bandWidth / input_.dfMax)));
			const double subEnergy = (currentQuantile - previousQuantile) / static_cast<double>(splitCount);
			for (int sub = 0; sub < splitCount; ++sub)
			{
				const double q = previousQuantile + subEnergy * (static_cast<double>(sub) + 0.5);
				const double subFreq = QuantileFrequency(q, freqs, cdf);
				WaveTrain train;
				train.amplitude = std::sqrt(2.0 * subEnergy);
				train.omega = 2.0 * M_PI * subFreq;
				train.phase = phaseDist(rng);
				train.wavenumber = SolveWaveNumber(train.omega, input_.waterDepth);
				train.A_omega = train.amplitude * train.omega;
				train.A_omega2 = train.A_omega * train.omega;
				result_.waveTrains.push_back(train);
			}
			previousQuantile = currentQuantile;
			previousFreq = freq;
		}
	}

	if (progress)
		progress("WaveL: solved wave numbers");
}

void WaveSpectrum::GenerateFromUserSpectrum(WaveProgressCallback progress)
{
	const auto user = ReadWaveUserSpectrumData(input_.importedSpectrumPath);
	double fMin = user.frequencies.front();
	double fMax = user.frequencies.back();
	std::function<double(double)> spectrumFn = [&](double f) {
		return InterpolateLinear(f, user.frequencies, user.spectralDensity);
	};

	const auto [freqs, cdfRaw] = SampleSpectrumGrid(fMin, fMax, spectrumFn);
	double rawArea = cdfRaw.back();
	if (rawArea <= 0.0)
		return;
	double scale = 1.0;
	if (input_.Hs > 0.0)
		scale = (Square(input_.Hs) / 16.0) / rawArea;
	std::vector<double> cdf = cdfRaw;
	for (double &value : cdf)
		value *= scale;

	std::mt19937 rng(input_.randomSeed);
	std::uniform_real_distribution<double> phaseDist(0.0, 2.0 * M_PI);
	const double totalArea = cdf.back();
	const double energyPerBin = totalArea / static_cast<double>(input_.numFreqBins);

	result_.waveTrains.clear();
	for (int i = 0; i < input_.numFreqBins; ++i)
	{
		const double q = energyPerBin * (static_cast<double>(i) + 0.5);
		const double frequency = QuantileFrequency(q, freqs, cdf);
		WaveTrain train;
		train.amplitude = std::sqrt(2.0 * energyPerBin);
		train.omega = 2.0 * M_PI * frequency;
		train.phase = phaseDist(rng);
		train.wavenumber = SolveWaveNumber(train.omega, input_.waterDepth);
		train.A_omega = train.amplitude * train.omega;
		train.A_omega2 = train.A_omega * train.omega;
		result_.waveTrains.push_back(train);
	}

	if (progress)
		progress("WaveL: discretized user spectrum");
}

void WaveSpectrum::GenerateFromUserTimeSeries(WaveProgressCallback progress)
{
	const auto series = ReadWaveTimeSeriesData(input_.importedTimeSeriesPath);
	const auto times = BuildUniformTimes(input_.timeStep, input_.simDuration);
	const int n = static_cast<int>(times.size());
	std::vector<double> samples(static_cast<std::size_t>(n), 0.0);
	for (int i = 0; i < n; ++i)
		samples[static_cast<std::size_t>(i)] = SamplePeriodicSeries(times[static_cast<std::size_t>(i)], series);

	const int spectrumSize = n / 2 + 1;
	fftw_complex *spectrum = fftw_alloc_complex(static_cast<std::size_t>(spectrumSize));
	if (!spectrum)
		throw std::runtime_error("WaveL FFTW allocation failed");
	fftw_plan plan = fftw_plan_dft_r2c_1d(n, samples.data(), spectrum, FFTW_ESTIMATE);
	if (!plan)
	{
		fftw_free(spectrum);
		throw std::runtime_error("WaveL FFTW plan creation failed");
	}
	fftw_execute(plan);
	fftw_destroy_plan(plan);

	result_.waveTrains.clear();
	result_.waveTrains.reserve(static_cast<std::size_t>(spectrumSize));
	for (int k = 1; k < spectrumSize; ++k)
	{
		const double re = spectrum[static_cast<std::size_t>(k)][0];
		const double im = spectrum[static_cast<std::size_t>(k)][1];
		const double magnitude = std::sqrt(re * re + im * im);
		double amplitude = 2.0 * magnitude / static_cast<double>(n);
		if (n % 2 == 0 && k == spectrumSize - 1)
			amplitude = magnitude / static_cast<double>(n);
		if (amplitude <= 1.0e-8)
			continue;

		WaveTrain train;
		const double frequency = static_cast<double>(k) / (static_cast<double>(n) * input_.timeStep);
		train.amplitude = amplitude;
		train.phase = std::atan2(-im, re) + M_PI / 2.0;
		train.omega = 2.0 * M_PI * frequency;
		train.direction = input_.dirMean * M_PI / 180.0;
		train.cosDir = std::cos(train.direction);
		train.sinDir = std::sin(train.direction);
		train.wavenumber = SolveWaveNumber(train.omega, input_.waterDepth);
		train.A_omega = train.amplitude * train.omega;
		train.A_omega2 = train.A_omega * train.omega;
		result_.waveTrains.push_back(train);
	}

	std::sort(result_.waveTrains.begin(), result_.waveTrains.end(), [](const WaveTrain &a, const WaveTrain &b) {
		return a.omega < b.omega;
	});
	fftw_free(spectrum);
	if (progress)
		progress("WaveL: decomposed user time series with FFTW");
}

void WaveSpectrum::ApplyDirectionalDistribution()
{
	if (result_.waveTrains.empty())
		return;

	const double meanRad = input_.dirMean * M_PI / 180.0;
	if (input_.dirSpectrum == WaveDirSpectrum::UNIDIRECTIONAL || input_.numDirBins <= 1 || std::fabs(input_.dirMax) <= kMinPositive)
	{
		for (auto &train : result_.waveTrains)
		{
			train.direction = meanRad;
			train.cosDir = std::cos(meanRad);
			train.sinDir = std::sin(meanRad);
		}
		return;
	}

	const double dirMean = input_.dirMean * M_PI / 180.0;
	const double dirMax = input_.dirMax * M_PI / 180.0;
	const double delta = 2.0 * dirMax / static_cast<double>(kDenseSpectrumPoints);
	std::vector<double> dirs(kDenseSpectrumPoints + 1);
	std::vector<double> cdf(kDenseSpectrumPoints + 1, 0.0);
	double cumulative = 0.0;
	for (int i = 0; i <= kDenseSpectrumPoints; ++i)
	{
		const double dir = dirMean - dirMax + delta * static_cast<double>(i);
		dirs[static_cast<std::size_t>(i)] = dir;
		if (i > 0)
			cumulative += EvaluateDirectionalWeight(dir, input_.dirMean, input_.dirMax, input_.dirSpreadExp) * delta;
		cdf[static_cast<std::size_t>(i)] = cumulative;
	}
	if (cdf.back() <= 0.0)
	{
		for (auto &train : result_.waveTrains)
		{
			train.direction = meanRad;
			train.cosDir = std::cos(meanRad);
			train.sinDir = std::sin(meanRad);
		}
		return;
	}
	for (double &value : cdf)
		value /= cdf.back();

	std::vector<double> directionSteps;
	directionSteps.reserve(static_cast<std::size_t>(input_.numDirBins));
	for (int i = 0; i < input_.numDirBins; ++i)
	{
		const double q = (static_cast<double>(i) + 0.5) / static_cast<double>(input_.numDirBins);
		directionSteps.push_back(QuantileFrequency(q, dirs, cdf));
	}

	std::mt19937 rng(input_.randomSeed);
	std::shuffle(directionSteps.begin(), directionSteps.end(), rng);
	for (std::size_t i = 0; i < result_.waveTrains.size(); ++i)
	{
		const double direction = directionSteps[i % directionSteps.size()];
		result_.waveTrains[i].direction = direction;
		result_.waveTrains[i].cosDir = std::cos(direction);
		result_.waveTrains[i].sinDir = std::sin(direction);
	}
}

void WaveSpectrum::UpdateResultStatistics()
{
	result_.numComponents = static_cast<int>(result_.waveTrains.size());
	if (result_.waveTrains.empty())
	{
		result_.significantHeight = 0.0;
		result_.peakPeriod = input_.Tp > 0.0 ? input_.Tp : 0.0;
		result_.peakFrequency = input_.Tp > 0.0 ? 1.0 / input_.Tp : 0.0;
		result_.zeroMoment = 0.0;
		result_.spectralArea = 0.0;
		result_.fMin = 0.0;
		result_.fMax = 0.0;
		return;
	}

	double m0 = 0.0;
	double maxAmplitude = -1.0;
	double fp = 0.0;
	result_.fMin = std::numeric_limits<double>::max();
	result_.fMax = 0.0;
	for (const auto &train : result_.waveTrains)
	{
		const double frequency = train.omega / (2.0 * M_PI);
		result_.fMin = std::min(result_.fMin, frequency);
		result_.fMax = std::max(result_.fMax, frequency);
		m0 += 0.5 * Square(train.amplitude);
		if (train.amplitude > maxAmplitude)
		{
			maxAmplitude = train.amplitude;
			fp = frequency;
		}
	}
	result_.zeroMoment = m0;
	result_.spectralArea = m0;
	if (result_.significantHeight <= 0.0)
		result_.significantHeight = 4.0 * std::sqrt(std::max(0.0, m0));
	if (result_.peakFrequency <= 0.0)
		result_.peakFrequency = fp;
	if (result_.peakPeriod <= 0.0 && result_.peakFrequency > 0.0)
		result_.peakPeriod = 1.0 / result_.peakFrequency;
	result_.waveTrains.shrink_to_fit();
}

void WaveSpectrum::BuildPrimaryCache()
{
	cacheBuilt_ = false;
	if (input_.mode != WaveMode::GENERATE)
		return;
	wavel_detail::WaveKinematicsEngine::BuildCache(result_.waveTrains, input_, cache_);
	input_.gridNX = cache_.nx;
	input_.gridNY = cache_.ny;
	input_.gridNZ = cache_.nz;
	input_.gridDX = cache_.dx;
	input_.gridDY = cache_.dy;
	input_.gridDZ = cache_.dz;

	cacheBuilt_ = true;

	const auto cacheVel = CenterCacheVelocity(cache_);
	const double sampleX = 0.0;
	const double sampleY = 0.0;
	const double sampleZ = cache_.zBottom + static_cast<double>(input_.gridNZ / 2) * cache_.dz;
	const double sampleT = static_cast<double>(cache_.nt / 2) * input_.timeStep;
	const auto direct = GetKinematics(sampleX, sampleY, sampleZ, sampleT);
	const double mismatch =
		std::fabs(cacheVel[0] - direct.vel[0]) +
		std::fabs(cacheVel[1] - direct.vel[1]) +
		std::fabs(cacheVel[2] - direct.vel[2]);
	if (mismatch > 1.0e-8)
		result_.warnings.push_back(L_WAVEL_CacheMismatch);
}

void WaveSpectrum::WriteCacheFiles()
{
	if (input_.cachePath.empty() || input_.metadataPath.empty())
		return;
	if (!cacheBuilt_ && !importedExternalKinematics_)
		BuildPrimaryCache();
	if (!cacheBuilt_)
		return;
	if (input_.mode == WaveMode::GENERATE || (input_.mode == WaveMode::IMPORT && input_.importedCachePath.empty()))
	{
		WriteWaveCacheFile(input_.cachePath, input_, result_, cache_);
		result_.cacheFilePath = input_.cachePath;
	}
	else if (!result_.cacheFilePath.empty())
	{
		// Preserve the imported cache path instead of silently rewriting it.
	}
	else
	{
		result_.cacheFilePath = !input_.importedCachePath.empty() ? input_.importedCachePath : input_.cachePath;
	}
	WriteWaveMetadataFile(input_.metadataPath, input_, result_, input_.mode == WaveMode::IMPORT ? "IMPORT" : "GENERATE");
	result_.metadataFilePath = input_.metadataPath;
}

void WaveSpectrum::WriteOutputs(WaveProgressCallback progress)
{
	if (!input_.cachePath.empty())
		result_.cacheFilePath = input_.mode == WaveMode::IMPORT && !input_.importedCachePath.empty() ? input_.importedCachePath : input_.cachePath;
	if (!input_.metadataPath.empty())
		result_.metadataFilePath = input_.metadataPath;

	if (input_.mode == WaveMode::GENERATE && input_.outputComponents && !input_.componentsPath.empty())
	{
		WriteWaveComponentFile(input_.componentsPath, input_, result_);
		result_.componentsFilePath = input_.componentsPath;
	}

	if (input_.mode == WaveMode::GENERATE && input_.outputTimeSeries && !input_.timeSeriesPath.empty())
	{
		const auto times = BuildUniformTimes(input_.timeStep, input_.simDuration);
		std::vector<double> eta(times.size(), 0.0);
		for (std::size_t i = 0; i < times.size(); ++i)
			eta[i] = GetElevation(0.0, 0.0, times[i]);
		WriteWaveTimeSeriesFile(input_.timeSeriesPath, input_.timeStep, times, eta);
		result_.timeSeriesFilePath = input_.timeSeriesPath;
	}

	if (input_.mode == WaveMode::GENERATE && input_.outputKinematicsGrid && !input_.kinematicsPath.empty())
	{
		if (progress)
			progress("WaveL: writing kinematics snapshots");
		wavel_detail::WaveKinematicsEngine::WriteKinematicsSnapshots(result_.waveTrains, input_, input_.kinematicsPath);
		result_.kinematicsDirectory = input_.kinematicsPath;
	}

	if (!input_.summaryPath.empty())
	{
		const std::string modeLabel = input_.mode == WaveMode::IMPORT ? "IMPORT" : "GENERATE";
		WriteWaveSummaryFile(input_.summaryPath, input_, result_, modeLabel);
		result_.summaryFilePath = input_.summaryPath;
	}

	WriteCacheFiles();
}
