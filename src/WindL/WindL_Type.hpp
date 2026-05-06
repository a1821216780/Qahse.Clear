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
// 该文件定义 WindL 模块的所有数据类型，包括：
// - 枚举：工作模式、湍流模型、风模型、IEC 标准参数等
// - 主输入结构体 WindLInput（对应 .qwd 文件）
// - 用户自定义剪切廓线 UserShearData（对应 User Shear .dat）
// - 用户自定义风谱 UserSpectraData（对应 User Spectra .dat）
// - 用户自定义风速时间序列 UserWindSpeedData（对应 User Wind Speed .dat）
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <string>
#include <vector>
#include <Eigen/Dense>

// ============================================================================
// 枚举定义
// ============================================================================

/// @brief 工作模式
enum class Mode
{
	GENERATE = 0, ///< 生成风文件
	IMPORT = 1,	  ///< 导入风文件
	BATCH = 2	  ///< 批量模式（从 Excel 参数表）
};

/// @brief 湍流风谱模型
enum class TurbModel
{
	IEC_KAIMAL = 0,		///< IEC Kaimal
	IEC_VKAIMAL = 1,	///< IEC von Kármán
	B_MANN = 2,			///< Bladed Mann
	B_KAL = 3,			///< Bladed Kaimal
	B_VKAL = 4,			///< Bladed von Kármán
	B_IVKAL = 5,		///< Bladed Improved von Kármán
	USER_SPECTRA = 6,	///< 用户自定义风谱
	USER_WIND_SPEED = 7, ///< 用户自定义风速时间序列
	USRVKM = 8 ///< 用户 profile 驱动 von Karman 风谱
};

/// @brief 湍流风模型（IEC 事件类型）
enum class WindModel
{
	NTM = 0,   ///< Normal Turbulence Model
	ETM = 1,   ///< Extreme Turbulence Model
	EWM1 = 2,  ///< Extreme Wind Model (1-year)
	EWM50 = 3, ///< Extreme Wind Model (50-year)
	EOG = 4,   ///< Extreme Operating Gust
	EDC = 5,   ///< Extreme Direction Change
	ECD = 6,   ///< Extreme Coherent Gust with Direction Change
	EWS = 7,   ///< Extreme Wind Shear
	UNIFORM = 8 ///< Steady uniform wind without turbulence
};

/// @brief IEC 标准版次
enum class IecStandard
{
	ED2 = 0, ///< IEC 61400-1 Edition 2
	ED3 = 1, ///< IEC 61400-1 Edition 3
	ED4 = 2	 ///< IEC 61400-1 Edition 4
};

/// @brief 风力机等级（IEC 61400-1 标准分类）
enum class TurbineClass
{
	Class_I = 0,   ///< IEC 风力机等级 I（高风速，Vref = 50 m/s）
	Class_II = 1,  ///< IEC 风力机等级 II（中风速，Vref = 42.5 m/s）
	Class_III = 2, ///< IEC 风力机等级 III（低风速，Vref = 37.5 m/s）
	Class_S = 3    ///< 特殊等级，由设计者指定（Site-specific）
};

/// @brief 湍流等级（IEC 61400-1 标准分类）
enum class TurbulenceClass
{
	Class_A = 0, ///< 较高湍流等级（Iref = 0.16）
	Class_B = 1, ///< 中等湍流等级（Iref = 0.14）
	Class_C = 2  ///< 较低湍流等级（Iref = 0.12）
};

/// @brief 风切变类型
enum class ShearType
{
	PL = 0,	 ///< 幂律 (Power Law)
	LOG = 1, ///< 对数律 (Logarithmic)
	USER = 2 ///< 用户自定义
};

/// @brief TurbSim-style wind profile selector used by SimWind.
enum class WindProfileType
{
	DEFAULT_PROFILE = 0, ///< 默认廓线（与风文件一致，不额外施加剪切）
	IEC = 1,             ///< IEC 标准风廓线（幂律指数 0.2）
	PL = 2,              ///< 幂律风廓线（Power Law）
	LOG = 3,             ///< 对数律风廓线（Logarithmic）
	USER = 4             ///< 用户自定义风廓线
};

/// @brief 生成方法
enum class GenMethod
{
	AUTO = 0 ///< 自动选择最优生成算法
};

/// @brief 插值方法
enum class InterpMethod
{
	TRILINEAR = 0, ///< 三线性插值（速度快）
	CUBIC = 1      ///< 三次样条插值（更光滑）
};

/// @brief 相干模型
enum class CohModel
{
	IEC = 0,         ///< IEC 标准相干模型（基于 IEC 61400-1）
	GENERAL = 1,     ///< 通用指数相干模型（Coh = exp(-a * f * d / U)）
	DEFAULT_COH = 2, ///< 默认相干模型（按湍流模型自动选取）
	NONE = 3,        ///< 无相干（各点完全独立）
	API = 4          ///< API 外部自定义相干模型
};

/// @brief EWM 类型
enum class EWMType
{
	Turbulent = 0, ///< 湍流极端风（含湍流脉动分量）
	Steady = 1     ///< 稳态极端风（仅确定性时变分量）
};

/// @brief 事件符号
enum class EventSign
{
	POSITIVE = 0, ///< 正向事件（+ 方向）
	NEGATIVE = 1  ///< 负向事件（- 方向）
};

/// @brief 风文件格式
enum class WndFormat
{
	BLADED_WND = 0,	 ///< Bladed .wnd
	TURBSIM_BTS = 1, ///< TurbSim .bts
	TURBSIM_WND = 2	 ///< TurbSim .wnd
};

// ============================================================================
// 主输入参数结构体 — 对应 .qwd 文件
// ============================================================================

/**
 * @brief WindL 主输入参数（对应 Qahse_WindL_Main_DEMO.qwd）。
 *
 * 使用 Serializer 进行读写时：
 * - 设置 SetValueFirst(true) 以匹配 "值 关键字 - 注释" 格式
 * - 遇到 "default" 值时，对应字段保持 C++ 默认值
 */
struct WindLInput
{
	// ---- 模式与双轴模型 ----
	Mode mode = Mode::GENERATE;                      ///< 工作模式 (GENERATE/IMPORT/BATCH)，对应 .qwd 关键字 Mode
	TurbModel turbModel = TurbModel::IEC_KAIMAL;      ///< 湍流风谱模型，对应 .qwd 关键字 TurbModel
	WindModel windModel = WindModel::NTM;             ///< IEC 事件类型（NTM/ETM/EWM/EOG/EDC/ECD/EWS/UNIFORM），对应 .qwd 关键字 WindModel

	// ---- 分量生成开关 ----
	bool calWu = true;   ///< 是否生成 u 分量（纵向），对应 .qwd 关键字 CalWu
	bool calWv = true;   ///< 是否生成 v 分量（横向），对应 .qwd 关键字 CalWv
	bool calWw = true;   ///< 是否生成 w 分量（垂向），对应 .qwd 关键字 CalWw

	// ---- 多格式输出开关 ----
	bool wrBlwnd = true; ///< 是否输出 Bladed .wnd 格式，对应 .qwd 关键字 WrBlWnd
	bool wrTrbts = true; ///< 是否输出 TurbSim .bts 格式，对应 .qwd 关键字 WrTrBts
	bool wrTrwnd = true; ///< 是否输出 TurbSim .wnd 格式，对应 .qwd 关键字 WrTrWnd

	// ---- IEC 标准参数 ----
	IecStandard iecEdition = IecStandard::ED3;           ///< IEC 标准版次（ED2/ED3/ED4），对应 .qwd 关键字 IECstandard
	TurbineClass turbineClass = TurbineClass::Class_I;    ///< 风力机等级（Class_I/II/III/S），对应 .qwd 关键字 TurbineClass
	TurbulenceClass turbClass = TurbulenceClass::Class_B; ///< 湍流等级（Class_A/B/C），对应 .qwd 关键字 TurbulenceClass
	double vRef = 0.0;         ///< 参考风速 (m/s)，对应 .qwd 关键字 Vref
	double rotorDiameter = 0.0; ///< 风轮直径 (m)，对应 .qwd 关键字 RotorDiameter

	// ---- 平均风与剪切参数 ----
	double meanWindSpeed = 0.0;      ///< 平均风速 (m/s)，对应 .qwd 关键字 MeanWindSpeed
	double hubHeight = 0.0;          ///< 轮毂高度 (m)，对应 .qwd 关键字 HubHt
	double refHeight = -1.0;         ///< 参考高度 (m)；-1 表示与 HubHt 相同，对应 .qwd 关键字 RefHt
	ShearType shearType = ShearType::PL;                ///< 风切变类型 (PL/LOG/USER)，对应 .qwd 关键字 ShearType
	WindProfileType windProfileType = WindProfileType::DEFAULT_PROFILE; ///< 风速廓线类型，对应 .qwd 关键字 WindProfileType
	double shearExp = 0.2;           ///< 幂律切变指数，默认 0.2，对应 .qwd 关键字 PLExp
	double roughness = 0.01;         ///< 地表粗糙度 (m)，对应 .qwd 关键字 Z0
	double horAngle = 0.0;           ///< 水平入流角 (deg)，对应 .qwd 关键字 HorAngle
	double vertAngle = 0.0;          ///< 垂直入流角 (deg)，对应 .qwd 关键字 VertAngle
	std::string userShearFile;       ///< 用户自定义剪切廓线文件路径 (.dat)，对应 .qwd 关键字 USRShearFIle

	// ---- 网格与时域参数 ----
	double turbIntensity = 0.0;      ///< 湍流强度，对应 .qwd 关键字 TurbIntensity
	int turbSeed = 0;                ///< 湍流随机种子，对应 .qwd 关键字 TurbRandSeed
	int gridPtsY = 0;                ///< Y 方向（水平向）网格点数，对应 .qwd 关键字 GridPtsY
	int gridPtsZ = 0;                ///< Z 方向（垂向）网格点数，对应 .qwd 关键字 GridPtsZ
	double fieldDimY = 0.0;          ///< 风场 Y 方向尺寸 (m)，对应 .qwd 关键字 FieldDimY
	double fieldDimZ = 0.0;          ///< 风场 Z 方向尺寸 (m)，对应 .qwd 关键字 FieldDimZ
	double simTime = 0.0;            ///< 模拟时长 (s)，对应 .qwd 关键字 Time
	double timeStep = 0.0;           ///< 时间步长 (s)，对应 .qwd 关键字 TimeStep
	bool cycleWind = false;          ///< 是否循环风场（周期性边界），对应 .qwd 关键字 CycleWind

	// ---- 生成算法控制 ----
	GenMethod genMethod = GenMethod::AUTO;               ///< 生成方法，对应 .qwd 关键字 GenMethod
	bool useFFT = false;                                  ///< 是否使用 FFT 加速，对应 .qwd 关键字 UseFFT
	InterpMethod interpMethod = InterpMethod::TRILINEAR; ///< 插值方法 (TRILINEAR/CUBIC)，对应 .qwd 关键字 InterpMethod

	// ---- 用户自定义谱/时间序列文件 ----
	std::string userTurbFile;        ///< 用户自定义湍流文件路径 (.dat)，对应 .qwd 关键字 UserTurbFile
	bool useIECSimmga = false;       ///< 是否使用 IEC 标准差缩放，对应 .qwd 关键字 UseIECSimmga
	int scaleIEC = -1;               ///< IEC 缩放模式；-1 由 UseIECSimmga 自动推导（false→0, true→1），0/1/2 对应 TurbSim ScaleIEC，对应 .qwd 关键字 ScaleIEC
	double etmC = 2.0;               ///< ETM 常数 c，默认 2.0，对应 .qwd 关键字 ETMc
	double usableTime = 0.0;         ///< 可用时间 (s)，须 ≤ simTime - 启动时间，对应 .qwd 关键字 UsableTime
	double analysisTime = 0.0;       ///< 分析时间 (s)，对应 .qwd 关键字 AnalysisTime
	double richardson = 0.0;         ///< Richardson 数，对应 .qwd 关键字 RICH_NUMBER
	double uStar = 0.0;              ///< 摩擦速度 u* (m/s)，对应 .qwd 关键字 UStar
	double zOverL = 0.0;             ///< 莫宁-奥布霍夫稳定度参数 z/L，对应 .qwd 关键字 Z0_over_L
	double mixingLayerDepth = 0.0;   ///< 混合层深度 (m)，对应 .qwd 关键字 ZI
	double reynoldsUW = 0.0;         ///< 雷诺应力 <uw> (m²/s²)，对应 .qwd 关键字 REYNOLDS_UW
	double reynoldsUV = 0.0;         ///< 雷诺应力 <uv> (m²/s²)，对应 .qwd 关键字 REYNOLDS_UV
	double reynoldsVW = 0.0;         ///< 雷诺应力 <vw> (m²/s²)，对应 .qwd 关键字 REYNOLDS_VW

	// ---- von Kármán / Bladed 纵向 (x) 长度尺度 (0 表示由程序根据标准自动计算) ----
	double vkLu = 0.0;               ///< von Kármán u 分量纵向长度尺度 (m)，0 = 自动计算，对应 .qwd 关键字 VKLu
	double vkLv = 0.0;               ///< von Kármán v 分量纵向长度尺度 (m)，0 = 自动计算，对应 .qwd 关键字 VKLv
	double vkLw = 0.0;               ///< von Kármán w 分量纵向长度尺度 (m)，0 = 自动计算，对应 .qwd 关键字 VKLw

	// ---- Bladed 侧向 (y) 长度尺度 ----
	double vyLu = 0.0;               ///< Bladed u 分量侧向长度尺度 (m)，0 = 自动计算，对应 .qwd 关键字 VYLU
	double vyLv = 0.0;               ///< Bladed v 分量侧向长度尺度 (m)，0 = 自动计算，对应 .qwd 关键字 VYLV
	double vyLw = 0.0;               ///< Bladed w 分量侧向长度尺度 (m)，0 = 自动计算，对应 .qwd 关键字 VYLW

	// ---- Bladed 垂向 (z) 长度尺度 ----
	double vzLu = 0.0;               ///< Bladed u 分量垂向长度尺度 (m)，0 = 自动计算，对应 .qwd 关键字 VZLU
	double vzLv = 0.0;               ///< Bladed v 分量垂向长度尺度 (m)，0 = 自动计算，对应 .qwd 关键字 VZLV
	double vzLw = 0.0;               ///< Bladed w 分量垂向长度尺度 (m)，0 = 自动计算，对应 .qwd 关键字 VZLW

	// ---- Improved von Kármán 附加参数 ----
	double latitude = 0.0;           ///< 纬度 (deg)，用于 Coriolis 效应，对应 .qwd 关键字 Latitude
	double tiU = 0.0;                ///< u 分量湍流强度，0 = 自动计算，对应 .qwd 关键字 TIU
	double tiV = 0.0;                ///< v 分量湍流强度，0 = 自动计算，对应 .qwd 关键字 TIV
	double tiW = 0.0;                ///< w 分量湍流强度，0 = 自动计算，对应 .qwd 关键字 TIW

	// ---- Mann 参数 ----
	double mannAlphaEps = 0.0;       ///< Mann 模型 αε^(2/3) 参数 (m^(4/3)/s²)，对应 .qwd 关键字 MannAlphaEps
	double mannLength = 0.0;         ///< Mann 模型长度尺度 (m)，对应 .qwd 关键字 MannScalelength
	double mannGamma = 0.0;          ///< Mann 模型 Gamma 各向异性参数，对应 .qwd 关键字 MannGamma
	double mannMaxL = 0.0;           ///< Mann 模型最大波数截断，对应 .qwd 关键字 MannMaxL
	int mannNx = 0;                  ///< Mann 模型 x 方向网格数，对应 .qwd 关键字 MannNx
	int mannNy = 0;                  ///< Mann 模型 y 方向网格数，对应 .qwd 关键字 MannNy
	int mannNz = 0;                  ///< Mann 模型 z 方向网格数，对应 .qwd 关键字 MannNz

	// ---- 相干模型 ----
	CohModel cohMod1 = CohModel::DEFAULT_COH;  ///< u 分量相干模型，对应 .qwd 关键字 CohMod1
	CohModel cohMod2 = CohModel::DEFAULT_COH;  ///< v 分量相干模型，对应 .qwd 关键字 CohMod2
	CohModel cohMod3 = CohModel::DEFAULT_COH;  ///< w 分量相干模型，对应 .qwd 关键字 CohMod3

	// ---- 通用相干参数 ----
	double cohDecayU = 0.0;          ///< u 分量相干衰减系数，对应 .qwd 关键字 CoDecayU
	double cohDecayV = 0.0;          ///< v 分量相干衰减系数，对应 .qwd 关键字 CoDecayV
	double cohDecayW = 0.0;          ///< w 分量相干衰减系数，对应 .qwd 关键字 CoDecayW
	double cohScaleB = 0.0;          ///< 相干尺度参数 b，对应 .qwd 关键字 CoScaleB
	double cohExp = 0.0;             ///< 相干指数，对应 .qwd 关键字 CoExp
	bool allowCohApprox = true;      ///< 是否允许相干近似加速，对应 .qwd 关键字 AllowCohApprox

	// ---- IEC 事件 / EWM 参数 ----
	EWMType ewmType = EWMType::Turbulent;     ///< EWM 类型 (Turbulent/Steady)，对应 .qwd 关键字 EWMType
	double gustPeriod = 0.0;          ///< 阵风周期 (s)，用于 EOG/EDC，对应 .qwd 关键字 GustPeriod
	double eventStart = 0.0;          ///< 事件开始时间 (s)，对应 .qwd 关键字 EventStart
	EventSign eventSign = EventSign::POSITIVE; ///< 事件符号 (POSITIVE/NEGATIVE)，对应 .qwd 关键字 EventSign
	double ecdVcog = 0.0;             ///< ECD 事件相干阵风幅值 (m/s)，对应 .qwd 关键字 ECD_VCOG

	// ---- 导入模式参数 ----
	std::string wndFilePath;          ///< 导入风文件路径，对应 .qwd 关键字 TurWindFile
	WndFormat wndFormat = WndFormat::BLADED_WND; ///< 导入风文件格式 (BLADED_WND/TURBSIM_BTS/TURBSIM_WND)，对应 .qwd 关键字 WndFormat

	// ---- 输出路径与文件名 ----
	std::string savePath;             ///< 输出路径（目录），对应 .qwd 关键字 WrWndPath
	std::string saveName;             ///< 输出文件名（不含扩展名），对应 .qwd 关键字 WrWndName
	bool sumPrint = false;            ///< 是否输出统计摘要（平均值、标准差等），对应 .qwd 关键字 SumPrint

	// ---- 批量模式参数 (Mode = BATCH) ----
	std::string batchExcelPath;       ///< 批量模式 Excel 参数文件路径 (.xlsx)，对应 .qwd 关键字 BatchExcelPath
	std::string batchSheetName = "Cases"; ///< 批量模式工作表名称，默认 "Cases"，对应 .qwd 关键字 BatchSheetName
	std::string batchOutputDir;       ///< 批量模式输出目录，对应 .qwd 关键字 BatchOutputDir
	int batchThreads = 0;             ///< 批量模式并行线程数；0 = 自动检测 CPU 核心数，对应 .qwd 关键字 BatchThreads
	std::string batchLauncher = "subprocess"; ///< 批量模式启动器类型，默认 "subprocess"，对应 .qwd 关键字 BatchLauncher
	bool batchValidateOnly = false;   ///< 仅验证批量参数不执行生成，对应 .qwd 关键字 BatchValidateOnly
};

// ============================================================================
// 用户自定义剪切廓线 — 对应 User_Defined_Shear .dat
// ============================================================================

/**
 * @brief 用户自定义风切变廓线数据（对应 Qahse_WindL_User_Defined_Shear_DEMO.dat）。
 *
 * 文件结构：
 * - 头部关键字行 (NumUSRz, StdScale1, StdScale2, StdScale3)
 * - !Begin 标记后为数据行：Height  WindSpeed  WindDirection  StdDev  LengthScale
 */
struct UserShearData
{
	/// @brief 高度层数，对应 .dat 文件头部关键字 NumUSRz
	int numHeights = 0;

	/// @brief u 分量标准差缩放因子，对应 .dat 文件头部关键字 StdScale1
	double stdScale1 = 1.0;
	/// @brief v 分量标准差缩放因子，对应 .dat 文件头部关键字 StdScale2
	double stdScale2 = 1.0;
	/// @brief w 分量标准差缩放因子，对应 .dat 文件头部关键字 StdScale3
	double stdScale3 = 1.0;

	/// @brief 高度 (m)，!Begin 标记后第 1 列数据
	std::vector<double> heights;
	/// @brief 风速 (m/s)，!Begin 标记后第 2 列数据
	std::vector<double> windSpeeds;
	/// @brief 风向 (deg，逆时针为正)，!Begin 标记后第 3 列数据
	std::vector<double> windDirections;
	/// @brief 标准差 (m/s)，!Begin 标记后第 4 列数据
	std::vector<double> standardDeviations;
	/// @brief 湍流长度尺度 (m)，!Begin 标记后第 5 列数据
	std::vector<double> lengthScales;
};

// ============================================================================
// 用户自定义风谱 — 对应 User_Defined_Spectra .dat
// ============================================================================

/**
 * @brief 用户自定义风谱数据（对应 Qahse_WindL_User_Defined_Spectra_DEMO.dat）。
 *
 * 文件结构：
 * - 头部关键字行 (NumUSRf)
 * - !Begin 标记后为数据行：Frequency  u_PSD  v_PSD  w_PSD
 */
struct UserSpectraData
{
	/// @brief 频率点数，对应 .dat 文件头部关键字 NumUSRf
	int numFrequencies = 0;

	/// @brief u 分量风谱缩放因子，对应 .dat 文件头部关键字 SpecScale1
	double specScale1 = 1.0;
	/// @brief v 分量风谱缩放因子，对应 .dat 文件头部关键字 SpecScale2
	double specScale2 = 1.0;
	/// @brief w 分量风谱缩放因子，对应 .dat 文件头部关键字 SpecScale3
	double specScale3 = 1.0;

	/// @brief 频率序列 (Hz)，!Begin 标记后第 1 列数据
	std::vector<double> frequencies;
	/// @brief u 分量功率谱密度 (m²/s)，!Begin 标记后第 2 列数据
	std::vector<double> uPsd;
	/// @brief v 分量功率谱密度 (m²/s)，!Begin 标记后第 3 列数据
	std::vector<double> vPsd;
	/// @brief w 分量功率谱密度 (m²/s)，!Begin 标记后第 4 列数据
	std::vector<double> wPsd;
};

// ============================================================================
// 用户自定义风速时间序列 — 对应 User_Defined_Wind_Speed .dat
// ============================================================================

/**
 * @brief 空间点坐标。
 */
struct WindPoint
{
	double y = 0.0; ///< 水平坐标 (m)
	double z = 0.0; ///< 垂直坐标 (m)
};

/**
 * @brief 用户自定义风速时间序列数据（对应 Qahse_WindL_User_Defined_Wind_Speed_DEMO.dat）。
 *
 * 文件结构：
 * - 头部关键字行 (nComp, nPoints, RefPtID)
 * - 空间点坐标列表 (nPoints 行)
 * - !Begin 标记后为时间序列数据：
 *   Time  Point01u  Point01v  Point01w  Point02u  Point02v  Point02w  ...
 *
 * 数据矩阵 (nPoints × nComp) 按列优先存储：
 * components[p][c] = 点 p 的第 c 个速度分量
 */
struct UserWindSpeedData
{
	/// @brief 速度分量数（通常为 3: u, v, w），对应 .dat 文件头部关键字 nComp
	int nComp = 0;
	/// @brief 空间点数，对应 .dat 文件头部关键字 nPoints
	int nPoints = 0;
	/// @brief 参考点索引（1-based），对应 .dat 文件头部关键字 RefPtID
	int refPtID = 0;

	/// @brief 空间点坐标列表（nPoints 个），位于文件头部关键字行之后
	std::vector<WindPoint> points;

	/// @brief 时间序列 (s)，!Begin 标记后第 1 列数据
	std::vector<double> time;

	/// @brief 风速分量三维矩阵: components[pointIndex][compIndex][timeIndex]
	/// 按点优先 (point-major) 存储：第 2 列起依次为 Point01u, Point01v, Point01w, Point02u, ...
	/// 尺寸: nPoints × nComp × time.size()
	std::vector<std::vector<std::vector<double>>> components;
};
