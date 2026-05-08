//**********************************************************************************************************************************
// 许可证说明
// 版权所有(C) 2021, 2025  赵子祯
//
// 根据 Boost 软件许可证 - 版本 1.0 - 2003年8月17日
// 您不得使用此文件，除非符合许可证。
// 您可以在以下网址获得许可证副本
//
//     http://www.hawtc.cn/licenses.txt
//
// 该软件按"原样"提供，不提供任何明示或暗示的保证，包括但不限于适销性、特定用途的适用性、所有权和非侵权。在任何情况下，版权持有人或任何分
// 发软件的人都不对任何索赔、损害或其他责任负责，无论是在合同诉讼、侵权诉讼或其他诉讼中，还是在软件使用或其他交易中产生的。
//
//**********************************************************************************************************************************

// ───────────────────────────────── File Info ─────────────────────────────────
//
// 该文件声明了 WaveSpectrum 类，作为波浪频谱核心处理器，负责输入验证、频率离散化、
// 方向扩展、波浪成分合成、运动学缓存和文件 I/O 输出。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "WaveL/WaveKinematics.hpp"
#include "WaveL/WaveL_Type.hpp"

/**
 * @brief 波浪进度回调函数类型，用于向调用方报告波浪生成/导入进度。
 *        Wave progress callback function type, used to report wave generation/import progress to the caller.
 * @param message 进度消息文本。Progress message text.
 */
using WaveProgressCallback = std::function<void(const std::string &)>;

/**
 * @brief 波浪频谱核心类，负责频谱离散化、波浪成分合成、运动学求解及文件 I/O。
 *        Core wave spectrum class, responsible for spectrum discretization, wave component
 *        synthesis, kinematics solving, and file I/O.
 *
 * WaveSpectrum 是波浪模块的中央处理器，统一管理以下功能：
 * - 输入参数校验（ValidateInput）
 * - 路径自动派生（ResolveDerivedPaths）
 * - 频谱模型计算（JONSWAP、Torsethaugen、Ochi-Hubble、用户频谱）
 * - 频率离散化（等能量/等频率间隔）
 * - 方向分布分配（单向/余弦扩散）
 * - 运动学缓存构建与查询
 * - 各种格式的输出文件写入
 *
 * WaveSpectrum is the central processor of the wave module, managing:
 * - Input parameter validation (ValidateInput)
 * - Path auto-derivation (ResolveDerivedPaths)
 * - Spectral model computation (JONSWAP, Torsethaugen, Ochi-Hubble, user spectrum)
 * - Frequency discretization (equal energy / equal frequency)
 * - Directional distribution assignment (unidirectional / cosine spreading)
 * - Kinematics cache building and querying
 * - Output file writing in various formats
 */
class WaveSpectrum
{
public:
	/**
	 * @brief 从输入参数构造 WaveSpectrum 实例。
	 *        Construct a WaveSpectrum instance from input parameters.
	 * @param input      波浪输入参数。Wave input parameters.
	 * @param sourcePath .qoe 源文件路径（用于路径派生，空表示默认）。.qoe source file path (for path derivation, empty = default).
	 */
	explicit WaveSpectrum(const WaveLInput &input = WaveLInput{}, std::string sourcePath = {});

	/**
	 * @brief 执行完整的波浪频谱生成流程。
	 *        Execute the complete wave spectrum generation pipeline.
	 * @param progress 进度回调（可选）。Progress callback (optional).
	 *
	 * 生成流程分为以下步骤：
	 * 1. ValidateInput()   — 校验输入参数
	 * 2. ResolveDerivedPaths() — 派生输出路径
	 * 3. 根据 freqSpectrum 类型分支到对应的生成方法
	 * 4. ApplyDirectionalDistribution() — 分配波浪方向
	 * 5. UpdateResultStatistics() — 计算统计量
	 * 6. BuildPrimaryCache() — 构建运动学缓存
	 * 7. WriteOutputs() — 写入各类输出文件
	 *
	 * Generation pipeline steps:
	 * 1. ValidateInput() — validate input parameters
	 * 2. ResolveDerivedPaths() — derive output paths
	 * 3. Branch to corresponding generation method based on freqSpectrum type
	 * 4. ApplyDirectionalDistribution() — assign wave directions
	 * 5. UpdateResultStatistics() — compute statistics
	 * 6. BuildPrimaryCache() — build kinematics cache
	 * 7. WriteOutputs() — write various output files
	 */
	void Generate(WaveProgressCallback progress = {});

	/**
	 * @brief 从波浪成分文件 (.wvc) 导入波浪数据。
	 *        Import wave data from a wave component file (.wvc).
	 * @param path     成分文件路径。Component file path.
	 * @param progress 进度回调（可选）。Progress callback (optional).
	 * @note 导入后自动调用 ValidateInput()、UpdateResultStatistics() 和 WriteOutputs()。
	 *       After import, automatically calls ValidateInput(), UpdateResultStatistics(), and WriteOutputs().
	 */
	void ImportComponents(const std::string &path, WaveProgressCallback progress = {});

	/**
	 * @brief 从缓存文件 (.wfc) 导入波浪数据，包含预计算的运动学网格。
	 *        Import wave data from a cache file (.wfc), including precomputed kinematics grid.
	 * @param path     缓存文件路径。Cache file path.
	 * @param progress 进度回调（可选）。Progress callback (optional).
	 * @note 导入缓存后，运动学查询将使用四维插值，大大加速随机采样。
	 *       After importing cache, kinematics queries use 4D interpolation, greatly accelerating random sampling.
	 */
	void ImportCache(const std::string &path, WaveProgressCallback progress = {});

	/**
	 * @brief 获取指定空间位置和时间的波浪运动学量。
	 *        Get wave kinematics at a specified spatial location and time.
	 * @return WaveKinematics 运动学结果。Wave kinematics result.
	 * @note 优先使用缓存插值（若已构建/导入）；否则通过规则波叠加直接计算。
	 *       Prefers cache interpolation if built/imported; otherwise direct superposition.
	 */
	WaveKinematics GetKinematics(double x, double y, double z, double time) const;

	/**
	 * @brief 获取指定水平位置和时间在静水面 (z=0) 处的自由表面高程。
	 *        Get free surface elevation at a specified horizontal location and time at still water level (z=0).
	 * @return 自由表面高程 [m]。Free surface elevation [m].
	 */
	double GetElevation(double x, double y, double time) const;

	/**
	 * @brief 获取自由表面高度（当前等价于 GetElevation）。
	 *        Get free surface height (currently equivalent to GetElevation).
	 */
	double GetFreeSurfaceZ(double x, double y, double time) const;

	/**
	 * @brief 判断指定空间位置在指定时刻是否浸没。
	 *        Check whether a specified spatial location is submerged at a given time.
	 * @return z <= 自由表面高度 + tolerance。z <= free surface height + tolerance.
	 */
	bool IsSubmerged(double x, double y, double z, double time, double tolerance = 0.0) const;

	/**
	 * @brief 对多个采样点在指定时刻批量计算运动学量。
	 *        Batch compute kinematics at multiple sample points for a given time.
	 * @param points 采样点列表。List of sample points.
	 * @param time   模拟时间 [s]。Simulation time [s].
	 * @return       各点的运动学向量。Kinematics vector for each point.
	 */
	std::vector<WaveKinematics> SampleKinematics(const std::vector<WaveSamplePoint> &points, double time) const;

	const WaveLInput &GetInput() const { return input_; }                 ///< 获取输入参数。Get input parameters.
	const WaveSpectrumResult &GetResult() const { return result_; }       ///< 获取频谱结果。Get spectrum result.
	const std::vector<WaveTrain> &GetWaveTrains() const { return result_.waveTrains; }  ///< 获取波浪成分列表。Get wave component list.
	double GetHs() const { return result_.significantHeight; }            ///< 获取有效波高 Hs [m]。Get significant wave height [m].
	double GetTp() const { return result_.peakPeriod; }                   ///< 获取谱峰周期 Tp [s]。Get peak spectral period [s].
	double GetFp() const { return result_.peakFrequency; }                ///< 获取谱峰频率 fp [Hz]。Get peak spectral frequency [Hz].
	double GetDepth() const { return input_.waterDepth; }                 ///< 获取水深 [m]。Get water depth [m].

	// ---- 静态工厂方法和参数校验 ----
	// Static factory methods and parameter validation

	/**
	 * @brief 仅校验输入参数，不执行波浪场生成（工厂方法）。
	 *        Validate input parameters only, without generating a wave field (factory method).
	 * @param input 波浪输入参数。Wave input parameters.
	 */
	static void ValidateInputOnly(const WaveLInput &input);

	/**
	 * @brief 根据输入参数生成波浪频谱（静态工厂方法）。
	 *        Generate a wave spectrum from input parameters (static factory method).
	 * @return 生成的 WaveSpectrum 实例。Generated WaveSpectrum instance.
	 */
	static WaveSpectrum Generate(const WaveLInput &input, WaveProgressCallback progress = {});

	/**
	 * @brief 导入已有波浪数据（静态工厂方法）。
	 *        Import existing wave data (static factory method).
	 * @return 导入的 WaveSpectrum 实例。Imported WaveSpectrum instance.
	 */
	static WaveSpectrum Import(const WaveLInput &input, WaveProgressCallback progress = {});

	/**
	 * @brief 从 .qoe 文件读取输入并生成频谱。
	 *        Read input from .qoe file and generate spectrum.
	 */
	static WaveSpectrum GenerateFromFile(const std::string &qoePath, WaveProgressCallback progress = {});

	/**
	 * @brief 从 .qoe 文件读取输入并导入频谱。
	 *        Read input from .qoe file and import spectrum.
	 */
	static WaveSpectrum ImportFromFile(const std::string &qoePath, WaveProgressCallback progress = {});

	// ---- 频谱模型静态评估函数 ----
	// Static spectral model evaluation functions

	/**
	 * @brief 评估 JONSWAP 谱密度 S(f) [m²/Hz]。
	 *        Evaluate JONSWAP spectral density S(f) [m²/Hz].
	 *
	 * 公式：S(f) = 0.3125×Hs²×Tp×f_r^(-5)×exp(-1.25×f_r^(-4))×(1-0.287×ln(γ))×γ^exp(-(f_r-1)²/(2σ²))
	 * 其中 f_r = f×Tp，γ 为谱峰提升因子，σ 为带宽参数。
	 *
	 * @param f         频率 [Hz]。Frequency [Hz].
	 * @param Hs        有效波高 [m]。Significant wave height [m].
	 * @param Tp        谱峰周期 [s]。Peak spectral period [s].
	 * @param autoGamma 是否根据 Tp/√Hs 自动计算 γ。Whether to auto-compute γ from Tp/√Hs.
	 * @param autoSigma 是否自动设置 σ（fp 左侧 0.07，右侧 0.09）。Whether to auto-set σ (0.07 left of fp, 0.09 right).
	 * @param gamma     谱峰提升因子（autoGamma=false 时有效）。Peak enhancement factor (effective when autoGamma=false).
	 * @param sigma1    左侧带宽参数（autoSigma=false 时有效）。Left bandwidth parameter (effective when autoSigma=false).
	 * @param sigma2    右侧带宽参数（autoSigma=false 时有效）。Right bandwidth parameter (effective when autoSigma=false).
	 * @return          谱密度值 [m²/Hz]。Spectral density [m²/Hz].
	 */
	static double EvaluateJonswapSpectrum(double f, double Hs, double Tp, bool autoGamma, bool autoSigma, double gamma, double sigma1, double sigma2);

	/**
	 * @brief 评估 Torsethaugen（双峰）谱密度 S(f) [m²/Hz]。
	 *        Evaluate Torsethaugen (double-peak) spectral density S(f) [m²/Hz].
	 *
	 * 根据 Tp 和 Tpf（由 Hs 估计的谱峰周期）的相对关系自动判断风浪主导或涌浪主导，
	 * 构造两个分谱 S1（风浪）和 S2（涌浪），可选叠加输出。
	 *
	 * @param doublePeak 是否输出 S1+S2 双峰叠加。Whether to output S1+S2 double-peak sum.
	 */
	static double EvaluateTorsethaugenSpectrum(double f, double Hs, double Tp, bool autoGamma, bool autoSigma, double gamma, double sigma1, double sigma2, bool doublePeak);

	/**
	 * @brief 评估 Ochi-Hubble（双峰）谱密度 S(f) [m²/Hz]。
	 *        Evaluate Ochi-Hubble (double-peak) spectral density S(f) [m²/Hz].
	 * @param f       频率 [Hz]。Frequency [Hz].
	 * @param Hs1     第一分谱有效波高 [m]。First component significant height [m].
	 * @param Hs2     第二分谱有效波高 [m]。Second component significant height [m].
	 * @param f1      第一分谱峰频 [Hz]。First component peak frequency [Hz].
	 * @param f2      第二分谱峰频 [Hz]。Second component peak frequency [Hz].
	 * @param lambda1 第一分谱形状参数。First component shape parameter.
	 * @param lambda2 第二分谱形状参数。Second component shape parameter.
	 * @return        谱密度 [m²/Hz]。Spectral density [m²/Hz].
	 */
	static double EvaluateOchiHubbleSpectrum(double f, double Hs1, double Hs2, double f1, double f2, double lambda1, double lambda2);

	/**
	 * @brief 评估余弦扩散方向分布函数权重 D(θ)。
	 *        Evaluate cosine spreading directional distribution function weight D(θ).
	 *
	 * 公式：D(θ) = C × |cos(π(θ-θ_mean)/(2×θ_max))|^(2s)，其中 C 为归一化常数，
	 * s = spreadExp 为扩散指数，θ 在 [θ_mean-θ_max, θ_mean+θ_max] 范围内非零。
	 *
	 * @param directionRad 目标方向角 [rad]。Target direction angle [rad].
	 * @param dirMeanDeg   主波向 [deg]。Mean direction [deg].
	 * @param dirMaxDeg    最大扩散角 [deg]。Maximum spread angle [deg].
	 * @param spreadExp    扩散指数（余弦幂次）。Spreading exponent (cosine power).
	 * @return             方向权重 D(θ)。Directional weight D(θ).
	 */
	static double EvaluateDirectionalWeight(double directionRad, double dirMeanDeg, double dirMaxDeg, double spreadExp);

	/**
	 * @brief 通过牛顿迭代法求解线性色散关系 ω² = gk×tanh(kd) 中的波数 k。
	 *        Solve for wavenumber k in the linear dispersion relation ω² = gk×tanh(kd) via Newton iteration.
	 *
	 * 初始猜测采用近似公式：k_approx = (ω²/g) × (1 - exp(-(ω√(d/g))^2.5))^(-0.4)
	 * 深水条件 (kd > 20) 下直接用近似公式 k_deep = ω²/g。
	 *
	 * @param omega      圆频率 [rad/s]。Angular frequency [rad/s].
	 * @param waterDepth 水深 [m]。Water depth [m].
	 * @return           波数 k [rad/m]。Wavenumber k [rad/m].
	 * @note 最大迭代 32 次，收敛容差 1e-12 × max(1, k)。
	 *       Max 32 iterations, convergence tolerance 1e-12 × max(1, k).
	 */
	static double SolveWaveNumber(double omega, double waterDepth);

private:
	WaveLInput input_;                          ///< 波浪输入参数（可能被自动派生修改）。Wave input parameters (may be modified by auto-derivation).
	WaveSpectrumResult result_;                 ///< 频谱生成结果。Spectrum generation result.
	std::string sourcePath_;                    ///< .qoe 源文件路径。.qoe source file path.
	wavel_detail::WaveFieldCache cache_;        ///< 运动学缓存。Kinematics cache.
	bool cacheBuilt_ = false;                   ///< 是否已构建缓存。Whether cache has been built.
	bool importedExternalKinematics_ = false;   ///< 是否从外部文件导入运动学数据。Whether kinematics data was imported from external file.

	/**
	 * @brief 校验所有输入参数的合法性。
	 *        Validate all input parameters.
	 * @throw std::runtime_error 当参数非法时抛出。Thrown when parameters are invalid.
	 */
	void ValidateInput() const;

	/**
	 * @brief 根据 sourcePath 自动派生未设置的文件输出路径。
	 *        Auto-derive unset file output paths from sourcePath.
	 * @note 派生规则：所有输出文件放在 sourcePath 同级的 result 目录下，
	 *       文件名为 sourcePath 的 stem（无后缀文件名）。
	 *       Derivation rule: all output files placed under result/ directory at sourcePath level,
	 *       filenames use the stem of sourcePath.
	 */
	void ResolveDerivedPaths();

	/**
	 * @brief 生成规则波（单频正弦波，波浪成分数 = 1）。
	 *        Generate regular wave (single-frequency sinusoidal, 1 wave component).
	 * @note 幅值 = Hs/2，圆频率 = 2π/Tp。
	 *       Amplitude = Hs/2, angular frequency = 2π/Tp.
	 */
	void GenerateRegular();

	/**
	 * @brief 从频谱模型（JONSWAP/Torsethaugen/Ochi-Hubble）生成波浪成分。
	 *        Generate wave components from a spectral model (JONSWAP/Torsethaugen/Ochi-Hubble).
	 *
	 * 流程：
	 * 1. 调用 ApplyAutoOchi() 自动推导 Ochi-Hubble 参数（若启用）
	 * 2. 确定频率范围 [fMin, fMax]（自动或手动指定）
	 * 3. 构造频谱函数 spectrumFn(f)，在密集网格上采样并计算累积分布函数 CDF
	 * 4. 按离散化方法（等能量/等频率）将 CDF 划分为 numFreqBins 个区间
	 * 5. 每个区间生成一个 WaveTrain：幅值=√(2×E)，相位随机
	 * 6. 调用 SolveWaveNumber 计算每个成分的波数
	 *
	 * Flow:
	 * 1. ApplyAutoOchi() to auto-derive Ochi-Hubble params (if enabled)
	 * 2. Determine frequency range [fMin, fMax] (auto or manual)
	 * 3. Build spectrum function spectrumFn(f), sample on dense grid, compute CDF
	 * 4. Divide CDF into numFreqBins intervals per discretization method (equal energy / equal frequency)
	 * 5. Generate one WaveTrain per interval: amplitude=√(2×E), random phase
	 * 6. SolveWaveNumber for each component's wavenumber
	 */
	void GenerateSpectrumDriven(WaveProgressCallback progress);

	/**
	 * @brief 从用户自定义频谱文件生成波浪成分。
	 *        Generate wave components from a user-defined spectrum file.
	 * @note 读取频域数据后执行等能量离散化。若 Hs>0，按 Hs²/16 缩放谱面积。
	 *       After reading frequency-domain data, performs equal-energy discretization.
	 *       If Hs>0, scales spectral area by Hs²/16.
	 */
	void GenerateFromUserSpectrum(WaveProgressCallback progress);

	/**
	 * @brief 从用户自定义时间序列文件通过 FFT 分解为波浪成分。
	 *        Decompose user-defined time series into wave components via FFT.
	 *
	 * 利用 fftw3 的 r2c（实数到复数）一维 DFT 将时域自由表面高程序列变换为频域，
	 * 从频谱幅值重建波浪成分（幅值、频率、相位）。频率分辨率 = 1/(n×timeStep)。
	 *
	 * Uses fftw3 r2c (real-to-complex) 1D DFT to transform the time-domain free-surface
	 * elevation sequence into the frequency domain, reconstructing wave components (amplitude,
	 * frequency, phase) from spectral magnitudes. Frequency resolution = 1/(n×timeStep).
	 */
	void GenerateFromUserTimeSeries(WaveProgressCallback progress);

	/**
	 * @brief 为每个波浪成分分配传播方向（单向或余弦扩散）。
	 *        Assign propagation direction to each wave component (unidirectional or cosine spreading).
	 *
	 * 单向模式：所有成分方向 = dirMean。
	 * 余弦扩散模式：在 [-dirMax, +dirMax] 上按余弦幂分布随机分配方向。
	 *
	 * Unidirectional: all components direction = dirMean.
	 * Cosine spreading: randomly assign directions within [-dirMax, +dirMax] per cosine power distribution.
	 */
	void ApplyDirectionalDistribution();

	/**
	 * @brief 根据波浪成分列表更新结果统计量（Hs、Tp、fp、m0、频率范围）。
	 *        Update result statistics (Hs, Tp, fp, m0, frequency range) from the wave component list.
	 *
	 * 计算项：
	 * - m0 = Σ 0.5×amplitude²（零阶谱矩/总能量）
	 * - Hs = 4×√m0（若未预设）
	 * - fp = 最大幅值成分的频率
	 * - Tp = 1/fp
	 *
	 * Computed items:
	 * - m0 = Σ 0.5×amplitude² (zero-order spectral moment / total energy)
	 * - Hs = 4×√m0 (if not preset)
	 * - fp = frequency of component with max amplitude
	 * - Tp = 1/fp
	 */
	void UpdateResultStatistics();

	/**
	 * @brief 构建主运动学缓存（当 mode=GENERATE 时自动执行）。
	 *        Build primary kinematics cache (auto-executed when mode=GENERATE).
	 *
	 * 委托给 WaveKinematicsEngine::BuildCache，完成后设置 cacheBuilt_=true
	 * 并验证缓存与直接叠加的一致性。
	 *
	 * Delegates to WaveKinematicsEngine::BuildCache, then sets cacheBuilt_=true
	 * and verifies cache consistency with direct superposition.
	 */
	void BuildPrimaryCache();

	/**
	 * @brief 写入 .wfc 和 .wfm 缓存及元数据文件。
	 *        Write .wfc and .wfm cache and metadata files.
	 * @note 若缓存未构建且非外部导入，先调用 BuildPrimaryCache()。
	 *       If cache is not built and not externally imported, calls BuildPrimaryCache() first.
	 */
	void WriteCacheFiles();

	/**
	 * @brief 写入所有启用的输出文件（成分、时间序列、运动学快照、摘要、缓存）。
	 *        Write all enabled output files (components, time series, kinematics snapshots, summary, cache).
	 */
	void WriteOutputs(WaveProgressCallback progress);
};
