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
	/**
	 * @brief 频谱采样密集网格点数，用于构建高精度的累积分布函数 (CDF)。
	 *        Dense spectrum sampling grid point count, used to build high-resolution CDF.
	 *
	 * 8192 + 1 = 8193 个采样点覆盖 [fMin, fMax]，提供足够的分辨率以保证
	 * 等能量离散化的精度。
	 *
	 * 8192 + 1 = 8193 sample points covering [fMin, fMax], providing sufficient
	 * resolution for accurate equal-energy discretization.
	 */
	constexpr int kDenseSpectrumPoints = 8192;

	/** @brief 最小正阈值，用于防止除零和处理浮点舍入误差。Minimum positive threshold for division-by-zero safety and floating-point tolerance. */
	constexpr double kMinPositive = 1.0e-12;

	/** @brief 深水判别阈值 (kd > 20)。Deep-water discrimination threshold (kd > 20). */
	constexpr double kDeepWaterThreshold = 20.0;

	/**
	 * @brief 返回值的平方。
	 *        Return the square of a value.
	 * @code
	 * double s = Square(3.0); // 9.0
	 * @endcode
	 */
	double Square(double value)
	{
		return value * value;
	}

	/**
	 * @brief 将数值钳制到 [lo, hi] 区间。
	 *        Clamp a value to the interval [lo, hi].
	 */
	double Clamp(double value, double lo, double hi)
	{
		return std::max(lo, std::min(hi, value));
	}

	/**
	 * @brief 根据文件路径获取默认基名（用于自动派生输出文件名）。
	 *        Get the default base name from a file path (for auto-deriving output filenames).
	 * @param path     文件路径（空时使用 fallback）。File path (uses fallback if empty).
	 * @param fallback 回退名称。Fallback name.
	 * @return         文件路径的 stem 部分，或 fallback。The stem part of the path, or fallback.
	 */
	std::string DefaultBaseName(const std::string &path, const char *fallback)
	{
		if (path.empty())
			return fallback;
		const auto stem = std::filesystem::path(path).stem().string();
		return stem.empty() ? fallback : stem;
	}

	/**
	 * @brief 在密集频率网格上对频谱函数进行采样，构建频率向量和累积分布函数 (CDF) 向量。
	 *        Sample the spectrum function on a dense frequency grid, building frequency and CDF vectors.
	 *
	 * 在 [fMin, fMax] 上均匀采样 kDenseSpectrumPoints+1 个点，用梯形积分法
	 * 累加谱密度得到累积能量分布。返回 (freqs, cdf) 对。
	 *
	 * Samples kDenseSpectrumPoints+1 points uniformly on [fMin, fMax], accumulating
	 * spectral density via trapezoidal integration to build the cumulative energy distribution.
	 * Returns the (freqs, cdf) pair.
	 *
	 * @param fMin     最小频率 [Hz]。Minimum frequency [Hz].
	 * @param fMax     最大频率 [Hz]。Maximum frequency [Hz].
	 * @param spectrum 频谱函数 S(f)。Spectrum function S(f).
	 * @return         (频率向量, CDF 向量) 对。Pair of (frequency vector, CDF vector).
	 */
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

	/**
	 * @brief 在已知 (xs, ys) 数据点上进行线性插值。
	 *        Perform linear interpolation on known (xs, ys) data points.
	 * @param x  插值横坐标。Interpolation abscissa.
	 * @param xs 已知横坐标序列（须单调递增）。Known abscissa sequence (must be monotonically increasing).
	 * @param ys 已知纵坐标序列。Known ordinate sequence.
	 * @return   插值结果；x 在范围外时返回边界值。Interpolated result; returns boundary value when x is out of range.
	 */
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

	/**
	 * @brief 通过 CDF 反函数（分位数函数）查找给定累积能量对应的频率。
	 *        Find the frequency corresponding to a given cumulative energy via CDF inverse (quantile function).
	 * @param target 目标累积值（在 [0, cdf.back()] 内）。Target cumulative value (within [0, cdf.back()]).
	 * @param freqs  频率向量。Frequency vector.
	 * @param cdf    累积分布函数向量。CDF vector.
	 * @return       目标频率 [Hz]；target 在范围外返回边界频率。Target frequency [Hz]; returns boundary freq if out of range.
	 */
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

	/**
	 * @brief 获取给定频率在 CDF 中的累积能量值（通过线性插值）。
	 *        Get the cumulative energy at a given frequency from the CDF (via linear interpolation).
	 * @param f     频率 [Hz]。Frequency [Hz].
	 * @param freqs 频率向量。Frequency vector.
	 * @param cdf   CDF 向量。CDF vector.
	 * @return      累积能量值。Cumulative energy value.
	 */
	double CumulativeAt(double f, const std::vector<double> &freqs, const std::vector<double> &cdf)
	{
		return InterpolateLinear(f, freqs, cdf);
	}

	/**
	 * @brief 计算 Ochi-Hubble 谱的有效波高 Hs_eff = √(Hs1² + Hs2²)。
	 *        Compute effective significant wave height for Ochi-Hubble: Hs_eff = √(Hs1² + Hs2²).
	 */
	double EffectiveHsForOchi(const WaveLInput &input)
	{
		return std::sqrt(Square(input.Hs1) + Square(input.Hs2));
	}

	/**
	 * @brief 根据总 Hs 和 Tp 自动推导 Ochi-Hubble 双峰参数。
	 *        Auto-derive Ochi-Hubble double-peak parameters from total Hs and Tp.
	 *
	 * 当 autoOchi=true 时：
	 * - Hs1 = 0.72 × totalHs
	 * - Hs2 = √(Hs² - Hs1²)
	 * - f1 = 0.6/Tp, f2 = 1.6/Tp
	 * - lambda1 = lambda2 = 2.0
	 *
	 * When autoOchi=true:
	 * - Hs1 = 0.72 × totalHs
	 * - Hs2 = √(Hs² - Hs1²)
	 * - f1 = 0.6/Tp, f2 = 1.6/Tp
	 * - lambda1 = lambda2 = 2.0
	 */
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

	/**
	 * @brief 生成 [0, duration] 上以 dt 为步长的均匀时间序列。
	 *        Generate a uniform time sequence on [0, duration] with step dt.
	 * @param dt       时间步长 [s]。Time step [s].
	 * @param duration 总时长 [s]。Total duration [s].
	 * @return         均匀时间向量（n = duration/dt + 1 个元素）。Uniform time vector (n = duration/dt + 1 elements).
	 */
	std::vector<double> BuildUniformTimes(double dt, double duration)
	{
		const int n = std::max(1, static_cast<int>(std::llround(duration / dt)) + 1);
		std::vector<double> times(static_cast<std::size_t>(n));
		for (int i = 0; i < n; ++i)
			times[static_cast<std::size_t>(i)] = static_cast<double>(i) * dt;
		return times;
	}

	/**
	 * @brief 从周期性时间序列中通过线性插值采样高程值。
	 *        Sample elevation from a periodic time series via linear interpolation.
	 * @param t      目标时间 [s]。Target time [s].
	 * @param series 时间序列表（support周期性循环）。Time series table (supports periodic cycling).
	 * @return       插值高程值 [m]。Interpolated elevation value [m].
	 */
	double SamplePeriodicSeries(double t, const WaveTimeSeriesData &series)
	{
		if (series.times.empty())
			return 0.0;
		const double start = series.times.front();
		const double end = series.times.back();
		const double span = std::max(end - start, kMinPositive);
		// 周期性循环：将 t 折叠到 [start, start+span]
		// Periodic cycling: fold t into [start, start+span]
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

	/**
	 * @brief 获取运动学缓存中心点（x=nx/2, y=ny/2, z=nz/2, t=nt/2）的速度向量。
	 *        Get the velocity vector at the center point of the kinematics cache
	 *        (x=nx/2, y=ny/2, z=nz/2, t=nt/2).
	 *
	 * 用于验证缓存与直接叠加的一致性。
	 * Used to verify cache consistency with direct superposition.
	 */
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

// ---- 构造与静态工厂方法 ----
// Constructor and static factory methods

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

/**
 * @brief 校验所有输入参数的合法性。
 *        Validate all input parameters.
 *
 * 校验规则：
 * - GENERATE 模式下：Tp>0, timeStep>0, simDuration>0, waterDepth>0, numFreqBins>0, numDirBins>0
 * - IMPORT 模式下：必须提供导入路径（cachePath 或 componentsPath）
 * - 当 freqSpectrum 为 USER_SPECTRUM 时：必须提供 importedSpectrumPath
 * - 当 freqSpectrum 为 USER_TIMESERIES 时：必须提供 importedTimeSeriesPath
 * - BATCH 模式不支持，直接报错
 *
 * Validation rules:
 * - GENERATE mode: Tp>0, timeStep>0, simDuration>0, waterDepth>0, numFreqBins>0, numDirBins>0
 * - IMPORT mode: must provide import path (cachePath or componentsPath)
 * - USER_SPECTRUM: must provide importedSpectrumPath
 * - USER_TIMESERIES: must provide importedTimeSeriesPath
 * - BATCH mode not supported, errors immediately
 *
 * @throw std::runtime_error 当任何参数非法时抛出。Thrown when any parameter is invalid.
 */
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

/**
 * @brief 根据 sourcePath 自动派生未显式指定的输出文件路径。
 *        Auto-derive output file paths that haven't been explicitly specified from sourcePath.
 *
 * 派生规则：
 * - 所有输出文件放置在 sourcePath 同级的 result/ 子目录下
 * - 文件基名取自 sourcePath 的 stem（无后缀部分），无源文件时默认 "WaveL"
 * - 缓存文件 (.wfc)、元数据 (.wfm)、成分文件 (.wvc)、时间序列 (.wts)、
 *   运动学目录 (_kinematics/)、摘要 (.sum) 均按此规则
 *
 * Derivation rules:
 * - All output files placed under result/ subdirectory at sourcePath level
 * - File base name uses sourcePath stem, defaults to "WaveL" when no source
 * - Cache (.wfc), metadata (.wfm), components (.wvc), time series (.wts),
 *   kinematics directory (_kinematics/), summary (.sum) all follow this rule
 */
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

/**
 * @brief 评估 JONSWAP 谱密度。
 *        Evaluate JONSWAP spectral density.
 *
 * 实现标准 JONSWAP 公式（Hasselmann 等，1973），含以下特性：
 * - 自动 gamma 推导：根据 Tp/√Hs 比值（Goda 建议）
 * - 自动 sigma 设置：fp 左侧 0.07，右侧 0.09
 * - 归一化因子 (1 - 0.287×ln(γ)) 保证谱面积 = Hs²/16
 *
 * Implements the standard JONSWAP formula (Hasselmann et al., 1973) with:
 * - Auto-gamma derivation from Tp/√Hs ratio (Goda recommendation)
 * - Auto-sigma: 0.07 left of fp, 0.09 right of fp
 * - Normalization factor (1 - 0.287×ln(γ)) ensures spectral area = Hs²/16
 */
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

/**
 * @brief 评估 Torsethaugen 谱密度（挪威大陆架经验双峰谱）。
 *        Evaluate Torsethaugen spectral density (Norwegian shelf empirical double-peak spectrum).
 *
 * 算法流程：
 * 1. 根据 Hs 估算特征谱峰 Tpf = a_f × Hs^(1/3)，其中 a_f = 6.6
 * 2. 比较 Tp 与 Tpf：Tp ≤ Tpf → 风浪主导；Tp > Tpf → 涌浪主导
 * 3. 计算风浪与涌浪的能量分配比 R
 * 4. 各自构建 JONSWAP 型谱（S1 为风浪/主导峰，S2 为涌浪/次峰）
 * 5. 若 doublePeak=true，返回 S1+S2，否则仅返回 S1
 *
 * Algorithm flow:
 * 1. Estimate characteristic peak Tpf = a_f × Hs^(1/3), with a_f = 6.6
 * 2. Compare Tp vs Tpf: Tp ≤ Tpf → wind-sea dominated; Tp > Tpf → swell dominated
 * 3. Compute energy partition ratio R between wind-sea and swell
 * 4. Build JONSWAP-type spectra (S1 = wind-sea/primary, S2 = swell/secondary)
 * 5. If doublePeak=true, return S1+S2, else return S1 only
 */
double WaveSpectrum::EvaluateTorsethaugenSpectrum(double f, double Hs, double Tp, bool autoGamma, bool autoSigma, double gamma, double sigma1, double sigma2, bool doublePeak)
{
	if (f <= 0.0 || Hs <= 0.0 || Tp <= 0.0)
		return 0.0;

	const double a_f = 6.6;     // 特征谱峰因子 Characteristic peak factor
	const double a_e = 2.0;     // 下界周期因子 Lower-bound period factor
	const double a_u = 25.0;    // 上界周期因子 Upper-bound period factor
	const double a_10 = 0.7;    // 风浪最小能量比 Minimum wind-sea energy ratio
	const double a_1 = 0.5;     // 风浪过渡宽度 Wind-sea transition width
	const double k_g = 35.0;    // gamma 比例因子 Gamma scaling factor
	const double b_1 = 2.0;     // 涌浪周期偏移 Swell period offset
	const double a_20 = 0.6;    // 涌浪最小能量比 Minimum swell energy ratio
	const double a_2 = 0.3;     // 涌浪过渡宽度 Swell transition width
	const double a_3 = 6.0;     // 涌浪 gamma 修正 Swell gamma correction

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
		// 风浪主导：第一峰为风浪 Wind-sea dominant: first peak is wind-sea
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
		// 涌浪主导：第一峰为涌浪 Swell dominant: first peak is swell
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
	// S1: 主导风浪/涌浪峰的 JONSWAP 型谱 Primary wind-sea/swell JONSWAP-type spectrum
	const double S1 = G0 * Ay * std::pow(f1n, -4.0) * std::exp(-std::pow(f1n, -4.0)) *
		std::pow(std::max(Gamma1, 1.0), std::exp(-Square(f1n - 1.0) / (2.0 * sigma * sigma)));
	// S2: 次峰的 Pierson-Moskowitz 型谱 Secondary Pierson-Moskowitz-type spectrum
	const double S2 = G0 * std::pow(f2n, -4.0) * std::exp(-std::pow(f2n, -4.0));
	return doublePeak ? (S1 + S2) : S1;
}

/**
 * @brief 评估 Ochi-Hubble 双峰谱密度。
 *        Evaluate Ochi-Hubble double-peak spectral density.
 *
 * 每个分谱公式：S_i(f) = 0.25 × ((4λ_i+1)ω_i⁴/4)^λ_i / Γ(λ_i) × Hs_i² / ω^(4λ_i+1) × exp(-((4λ_i+1)/4)×(ω_i/ω)⁴)
 * 其中 ω = 2πf 为圆频率，Γ 为 Gamma 函数。
 *
 * Each component formula: S_i(f) = 0.25 × ((4λ_i+1)ω_i⁴/4)^λ_i / Γ(λ_i) × Hs_i² / ω^(4λ_i+1) × exp(-((4λ_i+1)/4)×(ω_i/ω)⁴)
 * where ω = 2πf is angular frequency, Γ is the Gamma function.
 */
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

/**
 * @brief 评估余弦扩散方向分布函数。
 *        Evaluate cosine spreading directional distribution function.
 *
 * 方向分布 D(θ) 在 [θ_mean - θ_max, θ_mean + θ_max] 内按 cos^2s 分布，
 * 使得 ∫D(θ)dθ = 1 在整个扩散扇区内归一化。
 *
 * The directional distribution D(θ) follows cos^2s within [θ_mean - θ_max, θ_mean + θ_max],
 * normalized so that ∫D(θ)dθ = 1 over the entire spreading sector.
 */
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

/**
 * @brief 通过牛顿迭代法求解线性色散关系的波数 k。
 *        Solve wavenumber k from the linear dispersion relation via Newton iteration.
 *
 * 迭代公式：k_{n+1} = k_n - f(k_n)/f'(k_n)
 * 其中 f(k) = gk×tanh(kd) - ω²
 *      f'(k) = g×(tanh(kd) + kd×sech²(kd))
 *
 * 初始猜测使用近似公式（Guo 2002 近似），收敛容差 1e-12，最大迭代 32 次。
 *
 * Iteration formula: k_{n+1} = k_n - f(k_n)/f'(k_n)
 * where f(k) = gk×tanh(kd) - ω²
 *       f'(k) = g×(tanh(kd) + kd×sech²(kd))
 *
 * Initial guess uses an approximation (Guo 2002), convergence tolerance 1e-12, max 32 iterations.
 */
double WaveSpectrum::SolveWaveNumber(double omega, double waterDepth)
{
	if (omega <= 0.0 || waterDepth <= 0.0)
		return 0.0;
	const double deep = omega * omega / GRAVITY;
	// 深水条件可直接用近似解 Deep-water → direct approximate solution
	if (waterDepth * deep > kDeepWaterThreshold)
		return deep;
	// 初始猜测：Guo (2002) 近似 Initial guess: Guo (2002) approximation
	double k = deep * std::pow(1.0 - std::exp(-std::pow(omega * std::sqrt(waterDepth / GRAVITY), 2.5)), -0.4);
	k = std::max(k, deep * 0.5);
	// 牛顿迭代 Newton iteration
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

/**
 * @brief 执行完整的波浪频谱生成流水线。
 *        Execute the complete wave spectrum generation pipeline.
 *
 * 七个步骤按顺序执行，从校验、路径派生、频谱生成到缓存构建和输出。
 *
 * Seven steps executed sequentially from validation, path derivation, spectrum generation
 * through cache building and output.
 */
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

/**
 * @brief 从波浪成分文件 (.wvc) 导入数据。
 *        Import data from a wave component file (.wvc).
 *
 * 读取文件中的波浪成分列表（频率、幅值、相位、方向、波数），
 * 重建 WaveTrain 向量，并更新水深和统计量。
 *
 * Reads wave component list (frequency, amplitude, phase, direction, wavenumber),
 * reconstructs WaveTrain vector, and updates water depth and statistics.
 */
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

/**
 * @brief 从缓存文件 (.wfc) 导入数据，含预计算运动学网格。
 *        Import data from a cache file (.wfc), including precomputed kinematics grid.
 *
 * 从二进制缓存文件读取四维运动学网格数据及频谱统计量，设置 importedExternalKinematics_=true
 * 以启用缓存插值查询路径。若有效波高未预设，从网格中心的波面高程统计中计算。
 *
 * Reads 4D kinematics grid data and spectrum statistics from binary cache file, sets
 * importedExternalKinematics_=true to enable cache interpolation query path. If Hs is not preset,
 * computes it from surface elevation statistics at the grid center.
 */
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
		// 从缓存中心点的波面高程时间序列计算有效波高
		// Compute Hs from surface elevation time series at cache center point
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

/**
 * @brief 获取自由表面高程（等价于 z=0 处的运动学 eta）。
 *        Get free surface elevation (equivalent to kinematics eta at z=0).
 */
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

/**
 * @brief 获取运动学量：优先使用缓存插值，后备使用直接叠加。
 *        Get kinematics: prefer cache interpolation, fallback to direct superposition.
 */
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

/**
 * @brief 生成规则波成分（单频正弦波）。
 *        Generate a regular wave component (single-frequency sinusoidal).
 *
 * 幅值 = Hs/2，圆频率 = 2π/Tp，相位 = regularPhase，方向 = dirMean。
 * 生成后存入 result_.waveTrains。
 *
 * Amplitude = Hs/2, angular frequency = 2π/Tp, phase = regularPhase, direction = dirMean.
 * Stored into result_.waveTrains after generation.
 */
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

/**
 * @brief 从频谱模型（JONSWAP/Torsethaugen/Ochi-Hubble）生成波浪成分。
 *        Generate wave components from spectral models (JONSWAP/Torsethaugen/Ochi-Hubble).
 *
 * 核心流程：
 * 1. 确定频率范围（自动或手动）
 * 2. 构建频谱函数 lambda，在 8192 点密集网格上采样并计算 CDF
 * 3. 缩放 CDF 至目标能量 m0 = Hs²/16
 * 4. 按离散化方法生成波浪成分：
 *    - 等能量：将总能量均分为 numFreqBins 份，每份生成一个成分
 *    - 等频率：将频率范围等分子区间，按子区间能量生成成分
 * 5. 对每个成分用随机相位和 SolveWaveNumber() 计算波数
 *
 * Core flow:
 * 1. Determine frequency range (auto or manual)
 * 2. Build spectrum function lambda, sample on 8192-point dense grid, compute CDF
 * 3. Scale CDF to target energy m0 = Hs²/16
 * 4. Generate wave components per discretization method:
 *    - Equal energy: divide total energy into numFreqBins equal parts, one component each
 *    - Equal frequency: divide frequency range into equal sub-intervals, components per sub-energy
 * 5. Random phase and SolveWaveNumber() for each component
 */
void WaveSpectrum::GenerateSpectrumDriven(WaveProgressCallback progress)
{
	if (input_.Hs <= 0.0 && input_.freqSpectrum != WaveFreqSpectrum::OCHI_HUBBLE)
		return;

	ApplyAutoOchi(input_);

	double fMin = input_.fCutIn;
	double fMax = input_.fCutOut;
	if (input_.autoFreqRange || !(fMax > fMin))
	{
		// 自动频率范围：覆盖谱峰的 0.5/Tp ~ 10/Tp
		// Auto frequency range: cover 0.5/Tp to 10/Tp around peak
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

	// 密集采样 → CDF Dense sampling → CDF
	const auto [freqs, cdfRaw] = SampleSpectrumGrid(fMin, fMax, spectrumFn);
	const double rawArea = cdfRaw.back();
	if (rawArea <= 0.0)
		return;
	// 缩放至目标零阶矩 Scale to target zero moment
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
		// 等频率间隔离散化 Equal frequency spacing discretization
		const double df = (fMax - fMin) / static_cast<double>(input_.numFreqBins);
		for (int i = 0; i < input_.numFreqBins; ++i)
		{
			const double left = fMin + df * static_cast<double>(i);
			const double right = left + df;
			// 子区间内的能量 Energy within sub-interval
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
		// 等能量离散化 Equal energy discretization
		const double energyPerBin = totalArea / static_cast<double>(input_.numFreqBins);
		double previousQuantile = 0.0;
		double previousFreq = fMin;
		for (int i = 0; i < input_.numFreqBins; ++i)
		{
			const double currentQuantile = energyPerBin * static_cast<double>(i + 1);
			const double freq = QuantileFrequency(currentQuantile, freqs, cdf);
			const double bandWidth = std::max(freq - previousFreq, 0.0);
			// 若带宽超过 dfMax，拆分为多个子成分 Split into sub-components if bandwidth exceeds dfMax
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

/**
 * @brief 从用户自定义频谱文件生成波浪成分。
 *        Generate wave components from a user-defined spectrum file.
 *
 * 读取用户指定的 (f, S(f)) 数据对，构建 CDF 后按等能量法离散化。
 * 若指定了 Hs，将谱面积缩放至 Hs²/16。
 *
 * Reads user-defined (f, S(f)) data pairs, builds CDF, then discretizes via equal-energy method.
 * If Hs is specified, scales spectral area to Hs²/16.
 */
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

/**
 * @brief 从用户自定义时间序列通过 FFT 分解为波浪成分。
 *        Decompose user-defined time series into wave components via FFT.
 *
 * 使用 FFTW3 的一维实数到复数 DFT (r2c)，将时域自由表面高程序列变换为频域，
 * 从中提取各频率分量的幅值和相位作为波浪成分。
 *
 * 特殊处理：
 * - 奈奎斯特频率 (k=N/2) 处的幅值不乘 2（对称谱的单侧表示）
 * - 幅值小于 1e-8 的成分被过滤
 * - 相位加上 π/2 以转换为正弦相位约定
 * - 最终按频率升序排列波浪成分
 *
 * Uses FFTW3 1D real-to-complex DFT (r2c) to transform time-domain free-surface elevation
 * into frequency domain, extracting amplitude and phase for each frequency component as wave trains.
 *
 * Special handling:
 * - Amplitude at Nyquist (k=N/2) is not doubled (single-sided representation of symmetric spectrum)
 * - Components with amplitude < 1e-8 are filtered
 * - Phase shifted by +π/2 to convert to sine phase convention
 * - Final wave trains sorted by ascending frequency
 */
void WaveSpectrum::GenerateFromUserTimeSeries(WaveProgressCallback progress)
{
	const auto series = ReadWaveTimeSeriesData(input_.importedTimeSeriesPath);
	const auto times = BuildUniformTimes(input_.timeStep, input_.simDuration);
	const int n = static_cast<int>(times.size());
	// 在均匀时间网格上采样用户序列 Sample user series on uniform time grid
	std::vector<double> samples(static_cast<std::size_t>(n), 0.0);
	for (int i = 0; i < n; ++i)
		samples[static_cast<std::size_t>(i)] = SamplePeriodicSeries(times[static_cast<std::size_t>(i)], series);

	// FFTW3 r2c 变换：实数时域 -> 复数半谱 FFTW3 r2c transform: real time-domain -> complex half-spectrum
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
	// k=0 为直流分量，跳过 Skip k=0 (DC component)
	for (int k = 1; k < spectrumSize; ++k)
	{
		const double re = spectrum[static_cast<std::size_t>(k)][0];
		const double im = spectrum[static_cast<std::size_t>(k)][1];
		const double magnitude = std::sqrt(re * re + im * im);
		double amplitude = 2.0 * magnitude / static_cast<double>(n);
		if (n % 2 == 0 && k == spectrumSize - 1)
			amplitude = magnitude / static_cast<double>(n);  // 奈奎斯特频率不乘 2 Nyquist freq not doubled
		if (amplitude <= 1.0e-8)
			continue;

		WaveTrain train;
		const double frequency = static_cast<double>(k) / (static_cast<double>(n) * input_.timeStep);
		train.amplitude = amplitude;
		train.phase = std::atan2(-im, re) + M_PI / 2.0;  // 转换为 sine 相位约定 Convert to sine phase convention
		train.omega = 2.0 * M_PI * frequency;
		train.direction = input_.dirMean * M_PI / 180.0;
		train.cosDir = std::cos(train.direction);
		train.sinDir = std::sin(train.direction);
		train.wavenumber = SolveWaveNumber(train.omega, input_.waterDepth);
		train.A_omega = train.amplitude * train.omega;
		train.A_omega2 = train.A_omega * train.omega;
		result_.waveTrains.push_back(train);
	}

	// 按频率升序排列（后续处理依赖此顺序） Sort by ascending frequency (subsequent processing depends on this)
	std::sort(result_.waveTrains.begin(), result_.waveTrains.end(), [](const WaveTrain &a, const WaveTrain &b) {
		return a.omega < b.omega;
	});
	fftw_free(spectrum);
	if (progress)
		progress("WaveL: decomposed user time series with FFTW");
}

/**
 * @brief 为每个波浪成分分配传播方向。
 *        Assign propagation direction to each wave component.
 *
 * 单向模式：所有成分方向 = dirMean，直接设置 cosDir 和 sinDir。
 *
 * 余弦扩散模式：
 * 1. 在 [-dirMax, +dirMax] 角度范围内以余弦幂分布构建方向 CDF
 * 2. 将 CDF 按方向分箱数进行等能量离散化，生成方向步长序列
 * 3. 打乱方向序列后循环分配给各波浪成分
 *
 * Unidirectional: all component directions = dirMean, directly set cosDir/sinDir.
 *
 * Cosine spreading mode:
 * 1. Build directional CDF within [-dirMax, +dirMax] per cosine power distribution
 * 2. Equal-energy discretize CDF into numDirBins direction steps
 * 3. Shuffle direction sequence and cyclically assign to wave components
 */
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
	// 归一化至 1 Normalize to 1
	for (double &value : cdf)
		value /= cdf.back();

	// 等能量离散化方向 CDF Equal-energy discretize directional CDF
	std::vector<double> directionSteps;
	directionSteps.reserve(static_cast<std::size_t>(input_.numDirBins));
	for (int i = 0; i < input_.numDirBins; ++i)
	{
		const double q = (static_cast<double>(i) + 0.5) / static_cast<double>(input_.numDirBins);
		directionSteps.push_back(QuantileFrequency(q, dirs, cdf));
	}

	// 随机打乱并循环分配给波浪成分 Shuffle and cyclically assign to wave components
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

/**
 * @brief 从波浪成分列表更新结果中的统计量。
 *        Update result statistics from the wave component list.
 *
 * 计算项包括：
 * - numComponents: 波浪成分总数
 * - fMin, fMax: 最小/最大频率
 * - m0 (zeroMoment): Σ 0.5×amplitude²（总能量）
 * - spectralArea: 等于 m0
 * - significantHeight: 4×√m0（若未预先设置）
 * - peakFrequency: 最大幅值成分的频率
 * - peakPeriod: 1/peakFrequency
 *
 * Computed items include:
 * - numComponents: total wave component count
 * - fMin, fMax: min/max frequency
 * - m0 (zeroMoment): Σ 0.5×amplitude² (total energy)
 * - spectralArea: equals m0
 * - significantHeight: 4×√m0 (if not preset)
 * - peakFrequency: frequency of max-amplitude component
 * - peakPeriod: 1/peakFrequency
 */
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

/**
 * @brief 构建运动学缓存并验证一致性。
 *        Build kinematics cache and verify consistency.
 *
 * 仅当 mode=GENERATE 时执行。调用 WaveKinematicsEngine::BuildCache 填充四维网格，
 * 然后比较缓存中心点插值结果与直接叠加结果。若差异大于 1e-8，追加警告。
 *
 * Executes only when mode=GENERATE. Calls WaveKinematicsEngine::BuildCache to fill the 4D grid,
 * then compares cache interpolation result at center point with direct superposition.
 * Appends warning if difference exceeds 1e-8.
 */
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

	// 一致性验证：比较缓存插值与直接叠加 Consistency check: compare cache interpolation vs direct superposition
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

/**
 * @brief 写入 .wfc 和 .wfm 文件。
 *        Write .wfc and .wfm files.
 *
 * 若缓存尚未构建且非外部导入，则先调用 BuildPrimaryCache()。对 GENERATE 模式
 * 或缺乏导入缓存的 IMPORT 模式，写出新的缓存文件和元数据文件。
 *
 * If cache is not yet built and not externally imported, calls BuildPrimaryCache() first.
 * For GENERATE mode or IMPORT mode lacking import cache, writes new cache and metadata files.
 */
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
		// 保留导入缓存路径，不覆写 Preserve imported cache path without overwriting
	}
	else
	{
		result_.cacheFilePath = !input_.importedCachePath.empty() ? input_.importedCachePath : input_.cachePath;
	}
	WriteWaveMetadataFile(input_.metadataPath, input_, result_, input_.mode == WaveMode::IMPORT ? "IMPORT" : "GENERATE");
	result_.metadataFilePath = input_.metadataPath;
}

/**
 * @brief 写入所有启用的输出文件。
 *        Write all enabled output files.
 *
 * 按以下顺序写入：
 * 1. 波浪成分文件 (.wvc) - 若 outputComponents=true 且 mode=GENERATE
 * 2. 自由表面时间序列文件 (.wts) - 若 outputTimeSeries=true 且 mode=GENERATE
 * 3. 运动学 VTU 快照 - 若 outputKinematicsGrid=true 且 mode=GENERATE
 * 4. 摘要文件 (.sum) - 若 summaryPath 非空
 * 5. 缓存文件 (.wfc) 和元数据文件 (.wfm) - 始终执行
 *
 * Written in order:
 * 1. Wave component file (.wvc) - if outputComponents=true and mode=GENERATE
 * 2. Free-surface time series file (.wts) - if outputTimeSeries=true and mode=GENERATE
 * 3. Kinematics VTU snapshots - if outputKinematicsGrid=true and mode=GENERATE
 * 4. Summary file (.sum) - if summaryPath is not empty
 * 5. Cache file (.wfc) and metadata file (.wfm) - always
 */
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
