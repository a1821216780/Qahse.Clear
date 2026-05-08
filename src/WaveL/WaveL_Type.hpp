#pragma once

#include <array>
#include <string>
#include <vector>

/**
 * @brief 波浪频率谱类型枚举，指定波浪生成所基于的频谱模型或数据源。
 *        Wave frequency spectrum type, specifying the spectral model or data source for wave generation.
 */
enum class WaveFreqSpectrum
{
	NONE = 0,              ///< 无频谱（无波浪）。No spectrum (no waves).
	REGULAR = 1,           ///< 规则波（单频正弦波）。Regular wave (single-frequency sinusoidal).
	JONSWAP = 2,           ///< JONSWAP 谱（北海联合波浪项目谱）。JONSWAP spectrum (Joint North Sea Wave Project).
	TORSETHAUGEN = 3,      ///< Torsethaugen 双峰谱（挪威大陆架经验谱）。Torsethaugen double-peaked spectrum (Norwegian shelf empirical).
	OCHI_HUBBLE = 4,       ///< Ochi-Hubble 双峰谱。Ochi-Hubble double-peaked spectrum.
	USER_SPECTRUM = 5,     ///< 用户自定义频谱（从文件导入频率-谱密度对）。User-defined spectrum (imported from file as frequency-density pairs).
	USER_TIMESERIES = 6    ///< 用户自定义时间序列（从文件导入，经 FFT 分解）。User-defined time series (imported from file, decomposed via FFT).
};

/**
 * @brief 波浪方向谱类型枚举，控制波浪能量在方向上的分布方式。
 *        Wave directional spectrum type, controlling how wave energy is distributed across directions.
 */
enum class WaveDirSpectrum
{
	UNIDIRECTIONAL = 0,    ///< 单向波（所有能量沿单一方向传播）。Unidirectional waves (all energy propagates in a single direction).
	COSINE_SPREAD = 1      ///< 余弦扩散分布（能量按 cos^N 分布在一角度扇区内）。Cosine spreading distribution (energy distributed as cos^N within an angular sector).
};

/**
 * @brief 波浪频率离散化方法枚举，决定如何将连续频谱离散为有限个波浪成分。
 *        Wave frequency discretization method, specifying how to discretize a continuous spectrum into finite wave components.
 */
enum class WaveDiscretization
{
	EQUAL_ENERGY = 0,      ///< 等能量法（每个波浪成分携带相等能量，频率间距不均）。Equal energy method (equal energy per component, uneven frequency spacing).
	EQUAL_FREQUENCY = 1    ///< 等频率间隔法（每个波浪成分频率间距相等，能量不均）。Equal frequency method (uniform frequency spacing, uneven energy per component).
};

/**
 * @brief 波浪运动学拉伸方法枚举，决定如何将线性波浪理论推广到自由表面以上区域。
 *        Wave kinematics stretching method, specifying how to extend linear wave theory above the free surface.
 */
enum class WaveStretching
{
	NONE = 0,              ///< 无拉伸（自由表面以上无运动学量）。No stretching (no kinematics above free surface).
	VERTICAL = 1,          ///< 垂直拉伸（自由表面以上速度设为零）。Vertical stretching (velocity above free surface zeroed).
	EXTRAPOLATION = 2,     ///< 外推拉伸（将自由表面处的速度梯度线性外推至以上区域）。Extrapolation stretching (linear extrapolation of velocity gradient above free surface).
	WHEELER = 3            ///< Wheeler 拉伸（对垂向坐标进行 Wheeler 变换，使瞬时波浪表面为坐标原面）。Wheeler stretching (Wheeler coordinate transform, using instantaneous wave surface as datum).
};

/**
 * @brief 波浪模块工作模式枚举。
 *        Wave module operation mode.
 */
enum class WaveMode
{
	GENERATE = 0,          ///< 生成模式（根据输入参数合成波浪场）。Generate mode (synthesize wave field from input parameters).
	IMPORT = 1,            ///< 导入模式（从缓存或成分文件导入已有波浪数据）。Import mode (load existing wave data from cache or component file).
	BATCH = 2              ///< 批处理模式（首版不支持）。Batch mode (not supported in first release).
};

/**
 * @brief 单个规则波浪成分（波浪训练），包含其幅值、相位、圆频率、波数和方向等完整参数。
 *        Single regular wave component (wave train), with full parameters including amplitude, phase,
 *        angular frequency, wavenumber, and direction.
 *
 * A_omega 和 A_omega2 为预计算的乘积常量，用于运动学求解时的速度与加速度叠加：
 * - A_omega  = amplitude × omega  → 用于速度分量计算
 * - A_omega2 = A_omega × omega    → 用于加速度分量计算
 *
 * A_omega and A_omega2 are precomputed products for superposition in kinematics:
 * - A_omega  = amplitude × omega  → used for velocity component computation
 * - A_omega2 = A_omega × omega    → used for acceleration component computation
 */
struct WaveTrain
{
	double amplitude = 0.0;    ///< 波浪幅值 [m]。Wave amplitude [m].
	double phase = 0.0;        ///< 初始相位 [rad]。Initial phase [rad].
	double omega = 0.0;        ///< 圆频率 [rad/s]。Angular frequency [rad/s].
	double wavenumber = 0.0;   ///< 波数 [rad/m]。Wavenumber [rad/m].
	double direction = 0.0;    ///< 传播方向 [rad]。Propagation direction [rad].
	double cosDir = 1.0;       ///< 方向余弦 cos(direction)。Direction cosine cos(direction).
	double sinDir = 0.0;       ///< 方向正弦 sin(direction)。Direction sine sin(direction).
	double A_omega = 0.0;      ///< 幅值×圆频率 = amplitude × omega，用于速度叠加。Amplitude × angular frequency, used for velocity superposition.
	double A_omega2 = 0.0;     ///< A_omega × omega = amplitude × omega²，用于加速度叠加。A_omega × omega, used for acceleration superposition.
};

/**
 * @brief 波浪采样点空间坐标，用于运动学场采样。
 *        Wave sampling point spatial coordinates, used for kinematics field sampling.
 */
struct WaveSamplePoint
{
	std::string name;          ///< 采样点名称（可选标识）。Point name (optional identifier).
	double x = 0.0;            ///< X 坐标（顺浪向）[m]。X coordinate (down-wave direction) [m].
	double y = 0.0;            ///< Y 坐标（横向）[m]。Y coordinate (lateral) [m].
	double z = 0.0;            ///< Z 坐标（垂向，向上为正）[m]。Z coordinate (vertical, positive upward) [m].
};

/**
 * @brief 波浪运动学计算结果，包含自由表面高程、流体质点速度、加速度和动水压力。
 *        Wave kinematics result, containing free surface elevation, fluid particle velocity,
 *        acceleration, and dynamic pressure.
 *
 * 坐标系约定：eta 为自由表面相对于静水面 (z=0) 的位移，vel 和 acc 为流体质点的速度和加速度
 * （含方向），dynP 为动水压力。
 *
 * Coordinate convention: eta is the free surface displacement relative to still water level (z=0),
 * vel and acc are fluid particle velocity and acceleration (with direction), dynP is dynamic pressure.
 */
struct WaveKinematics
{
	double eta = 0.0;                                  ///< 自由表面高程 [m]。Free surface elevation [m].
	std::array<double, 3> vel{0.0, 0.0, 0.0};          ///< 流体速度 (u, v, w) [m/s]。Fluid velocity (u, v, w) [m/s].
	std::array<double, 3> acc{0.0, 0.0, 0.0};          ///< 流体加速度 (ax, ay, az) [m/s²]。Fluid acceleration (ax, ay, az) [m/s²].
	double dynP = 0.0;                                 ///< 动水压力 [Pa]。Dynamic pressure [Pa].
};

/**
 * @brief 波浪环境采样结果，包含运动学、自由表面高度、浸没深度和浸没状态。
 *        Wave environment sample result, containing kinematics, free surface height, immersion depth,
 *        and submersion state.
 *
 * 该结构体综合了单次时空采样的全部环境信息，常用于判断结构物在波浪中的浸没状态。
 *
 * This struct integrates all environmental information for a single spatio-temporal sample,
 * commonly used to assess structural submergence in waves.
 */
struct WaveEnvironmentSample
{
	WaveKinematics kinematics;     ///< 波浪运动学结果。Wave kinematics result.
	double freeSurfaceZ = 0.0;     ///< 采样点处的自由表面高度 [m]。Free surface height at sample point [m].
	double immersionDepth = 0.0;   ///< 浸没深度 = max(0, freeSurfaceZ + tolerance - z) [m]。Immersion depth = max(0, freeSurfaceZ + tolerance - z) [m].
	bool submerged = false;        ///< 是否浸没（z <= freeSurfaceZ + tolerance）。Whether submerged (z <= freeSurfaceZ + tolerance).
};

/**
 * @brief 波浪频谱生成结果，包含波浪成分列表、统计量和输出文件路径。
 *        Wave spectrum generation result, containing wave component list, statistics, and output file paths.
 *
 * 该结构体汇总了频谱生成后的完整产出，包括波浪成分数组、关键统计参数（有效波高 Hs、
 * 谱峰周期 Tp、零阶矩 m0 等）、以及各类输出文件的路径。warnings 字段收集生成过程中的
 * 非致命警告。
 *
 * This struct summarizes the complete output after spectrum generation, including wave component
 * arrays, key statistical parameters (significant height Hs, peak period Tp, zero moment m0, etc.),
 * and paths to various output files. The warnings field collects non-fatal warnings during generation.
 */
struct WaveSpectrumResult
{
	std::vector<WaveTrain> waveTrains;            ///< 波浪成分（训练）列表。List of wave components (trains).
	double significantHeight = 0.0;               ///< 有效波高 Hs（4×√m0）[m]。Significant wave height Hs (4×√m0) [m].
	double peakPeriod = 0.0;                      ///< 谱峰周期 Tp [s]。Peak spectral period Tp [s].
	double peakFrequency = 0.0;                   ///< 谱峰频率 fp [Hz]。Peak spectral frequency fp [Hz].
	double zeroMoment = 0.0;                      ///< 零阶谱矩 m0（总能量）[m²]。Zero-order spectral moment m0 (total energy) [m²].
	int numComponents = 0;                        ///< 波浪成分总数。Total number of wave components.
	double fMin = 0.0;                            ///< 最小频率 [Hz]。Minimum frequency [Hz].
	double fMax = 0.0;                            ///< 最大频率 [Hz]。Maximum frequency [Hz].
	double spectralArea = 0.0;                    ///< 频谱面积（等于 m0）[m²]。Spectral area (equals m0) [m²].
	std::string cacheFilePath;                    ///< 缓存文件 (.wfc) 路径。Cache file (.wfc) path.
	std::string metadataFilePath;                 ///< 元数据文件 (.wfm) 路径。Metadata file (.wfm) path.
	std::string componentsFilePath;               ///< 成分文件 (.wvc) 路径。Components file (.wvc) path.
	std::string timeSeriesFilePath;               ///< 时间序列文件 (.wts) 路径。Time series file (.wts) path.
	std::string summaryFilePath;                  ///< 摘要文件 (.sum) 路径。Summary file (.sum) path.
	std::string kinematicsDirectory;              ///< 运动学 VTU 快照目录路径。Kinematics VTU snapshot directory path.
	std::vector<std::string> warnings;            ///< 生成过程中的警告信息列表。List of warning messages during generation.
};

/**
 * @brief 波浪模块顶层输入参数结构体，包含频谱参数、波浪参数、网格参数、输出控制等全部配置。
 *        Wave module top-level input parameter struct, containing all configuration including
 *        spectrum parameters, wave parameters, grid parameters, and output controls.
 *
 * 该结构体是波浪生成与导入的统一入口参数。支持多种操作模式（GENERATE / IMPORT / BATCH）、
 * 多种频谱模型（JONSWAP、Torsethaugen、Ochi-Hubble）、用户数据输入以及灵活的输出控制。
 * 所有路径字段支持自动派生：当为空时，系统根据 sourcePath 和默认规则自动生成相应路径。
 *
 * This struct is the unified entry for wave generation and import. It supports multiple operation
 * modes (GENERATE / IMPORT / BATCH), multiple spectral models (JONSWAP, Torsethaugen, Ochi-Hubble),
 * user data input, and flexible output control. All path fields support automatic derivation:
 * when empty, the system generates paths based on sourcePath and default rules.
 */
struct WaveLInput
{
	WaveMode mode = WaveMode::GENERATE;              ///< 工作模式。Operation mode.

	WaveFreqSpectrum freqSpectrum = WaveFreqSpectrum::JONSWAP;   ///< 频率谱模型类型。Frequency spectrum model type.
	WaveDirSpectrum dirSpectrum = WaveDirSpectrum::UNIDIRECTIONAL; ///< 方向谱类型。Directional spectrum type.
	WaveDiscretization discretization = WaveDiscretization::EQUAL_ENERGY; ///< 频率离散化方法。Frequency discretization method.
	WaveStretching stretching = WaveStretching::EXTRAPOLATION;   ///< 运动学拉伸方法。Kinematics stretching method.

	double Hs = 3.0;               ///< 有效波高 [m]。Significant wave height [m].
	double Tp = 10.0;              ///< 谱峰周期 [s]。Peak spectral period [s].
	double timeOffset = 0.0;       ///< 时间偏移量 [s]（用于多工况相位偏移）。Time offset [s] (for phase offset across load cases).
	double dirMean = 0.0;          ///< 主波向 [deg]。Mean wave direction [deg].
	double dirMax = 30.0;          ///< 方向扩散最大角度 [deg]。Maximum directional spreading angle [deg].
	double dirSpreadExp = 2.0;     ///< 方向扩散指数（余弦幂次）。Directional spreading exponent (cosine power).

	double fCutIn = 0.0;           ///< 频率下限 [Hz]（0=自动）。Lower frequency cut-off [Hz] (0=auto).
	double fCutOut = 0.0;          ///< 频率上限 [Hz]（0=自动）。Upper frequency cut-off [Hz] (0=auto).
	double dfMax = 0.0;            ///< 最大频率带宽 [Hz]（0=不限制）。Maximum frequency bandwidth [Hz] (0=unlimited).
	int numFreqBins = 200;         ///< 频率分箱数（波浪成分目标数）。Number of frequency bins (target wave component count).
	int numDirBins = 24;           ///< 方向分箱数。Number of directional bins.
	int randomSeed = 0;            ///< 随机种子（0=用系统时间）。Random seed (0=use system time).

	double gamma = 0.0;            ///< JONSWAP 谱峰提升因子 (0=自动)。JONSWAP peak enhancement factor (0=auto).
	double sigma1 = 0.0;           ///< JONSWAP 左侧带宽参数 (0=自动=0.07)。JONSWAP left bandwidth parameter (0=auto=0.07).
	double sigma2 = 0.0;           ///< JONSWAP 右侧带宽参数 (0=自动=0.09)。JONSWAP right bandwidth parameter (0=auto=0.09).
	bool autoGamma = true;         ///< 是否自动计算 gamma。Whether to auto-compute gamma.
	bool autoSigma = true;         ///< 是否自动计算 sigma。Whether to auto-compute sigma.
	bool autoFreqRange = true;     ///< 是否自动计算频率范围。Whether to auto-compute frequency range.

	double Hs1 = 2.0;              ///< Ochi-Hubble 第一峰有效波高 [m]。Ochi-Hubble first peak significant height [m].
	double Hs2 = 1.5;              ///< Ochi-Hubble 第二峰有效波高 [m]。Ochi-Hubble second peak significant height [m].
	double f1 = 0.1;               ///< Ochi-Hubble 第一峰频率 [Hz]。Ochi-Hubble first peak frequency [Hz].
	double f2 = 0.2;               ///< Ochi-Hubble 第二峰频率 [Hz]。Ochi-Hubble second peak frequency [Hz].
	double lambda1 = 2.0;          ///< Ochi-Hubble 第一峰形状参数。Ochi-Hubble first peak shape parameter.
	double lambda2 = 2.0;          ///< Ochi-Hubble 第二峰形状参数。Ochi-Hubble second peak shape parameter.
	bool autoOchi = false;         ///< 是否根据 Hs/Tp 自动推导 Ochi-Hubble 参数。Whether to auto-derive Ochi-Hubble params from Hs/Tp.

	bool doublePeak = false;       ///< Torsethaugen 是否输出双峰叠加。Whether Torsethaugen outputs double-peak sum.
	double regularPhase = 0.0;     ///< 规则波初始相位 [rad]。Regular wave initial phase [rad].

	double timeStep = 0.1;         ///< 时间步长 [s]。Time step [s].
	double simDuration = 600.0;    ///< 模拟总时长 [s]。Simulation duration [s].
	double waterDepth = 100.0;     ///< 水深 [m]。Water depth [m].

	double mcfDiameter = 0.0;      ///< MacCamy-Fuchs 大直径修正的构件直径 [m] (0=不修正)。Member diameter for MacCamy-Fuchs large-diameter correction [m] (0=disabled).

	bool outputComponents = true;       ///< 是否输出波浪成分文件 (.wvc)。Whether to output wave component file (.wvc).
	bool outputTimeSeries = true;       ///< 是否输出自由表面时间序列文件 (.wts)。Whether to output free-surface time series file (.wts).
	bool outputKinematicsGrid = false;  ///< 是否输出运动学 VTU 快照文件。Whether to output kinematics VTU snapshot files.
	std::string cachePath;              ///< 缓存文件 (.wfc) 输出路径（空=自动）。Cache file (.wfc) output path (empty=auto).
	std::string metadataPath;           ///< 元数据文件 (.wfm) 输出路径（空=自动）。Metadata file (.wfm) output path (empty=auto).
	std::string componentsPath;         ///< 成分文件 (.wvc) 输出路径（空=自动）。Components file (.wvc) output path (empty=auto).
	std::string timeSeriesPath;         ///< 时间序列文件 (.wts) 输出路径（空=自动）。Time series file (.wts) output path (empty=auto).
	std::string kinematicsPath;         ///< 运动学输出目录路径（空=自动）。Kinematics output directory path (empty=auto).
	std::string summaryPath;            ///< 摘要文件 (.sum) 输出路径（空=自动）。Summary file (.sum) output path (empty=auto).

	int gridNX = 1;                    ///< 运动学缓存网格 X 方向点数。Kinematics cache grid point count in X.
	int gridNY = 1;                    ///< 运动学缓存网格 Y 方向点数。Kinematics cache grid point count in Y.
	int gridNZ = 10;                   ///< 运动学缓存网格 Z 方向点数。Kinematics cache grid point count in Z.
	double gridDX = 0.0;               ///< 运动学缓存网格 X 方向间距 [m] (0=自动=1.0)。Kinematics cache grid X spacing [m] (0=auto=1.0).
	double gridDY = 0.0;               ///< 运动学缓存网格 Y 方向间距 [m] (0=自动=1.0)。Kinematics cache grid Y spacing [m] (0=auto=1.0).
	double gridDZ = 0.0;               ///< 运动学缓存网格 Z 方向间距 [m] (0=自动=depth/(nz-1))。Kinematics cache grid Z spacing [m] (0=auto=depth/(nz-1)).

	std::string importedSpectrumPath;       ///< 导入自定义频谱的文件路径。File path for importing user-defined spectrum.
	std::string importedTimeSeriesPath;     ///< 导入自定义时间序列的文件路径。File path for importing user-defined time series.
	std::string importedComponentsPath;     ///< 导入波浪成分 (.wvc) 的文件路径。File path for importing wave components (.wvc).
	std::string importedCachePath;          ///< 导入缓存 (.wfc) 的文件路径。File path for importing cache (.wfc).
};

/**
 * @brief 用户自定义波浪频谱数据结构，存储频率-谱密度对。
 *        User-defined wave spectrum data structure, storing frequency-spectral density pairs.
 */
struct WaveUserSpectrumData
{
	std::vector<double> frequencies;       ///< 频率序列 [Hz]。Frequency sequence [Hz].
	std::vector<double> spectralDensity;   ///< 谱密度序列 [m²/Hz]。Spectral density sequence [m²/Hz].
};

/**
 * @brief 用户自定义波浪时间序列数据结构，存储时间-自由表面高程对。
 *        User-defined wave time series data structure, storing time-free surface elevation pairs.
 */
struct WaveTimeSeriesData
{
	std::vector<double> times;            ///< 时间序列 [s]。Time sequence [s].
	std::vector<double> elevations;       ///< 自由表面高程序列 [m]。Free surface elevation sequence [m].
};
