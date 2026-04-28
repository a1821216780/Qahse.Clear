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
	USER_WIND_SPEED = 7 ///< 用户自定义风速时间序列
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

/// @brief 风力机等级
enum class TurbineClass
{
	Class_I = 0,
	Class_II = 1,
	Class_III = 2,
	Class_S = 3
};

/// @brief 湍流等级
enum class TurbulenceClass
{
	Class_A = 0,
	Class_B = 1,
	Class_C = 2
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
	DEFAULT_PROFILE = 0,
	IEC = 1,
	PL = 2,
	LOG = 3,
	USER = 4
};

/// @brief 生成方法
enum class GenMethod
{
	AUTO = 0
};

/// @brief 插值方法
enum class InterpMethod
{
	TRILINEAR = 0,
	CUBIC = 1
};

/// @brief 相干模型
enum class CohModel
{
	IEC = 0,
	GENERAL = 1,
	DEFAULT_COH = 2
};

/// @brief EWM 类型
enum class EWMType
{
	Turbulent = 0,
	Steady = 1
};

/// @brief 事件符号
enum class EventSign
{
	POSITIVE = 0,
	NEGATIVE = 1
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
	Mode mode = Mode::GENERATE;
	TurbModel turbModel = TurbModel::IEC_KAIMAL;
	WindModel windModel = WindModel::NTM;

	// ---- 分量生成开关 ----
	bool calWu = true;
	bool calWv = true;
	bool calWw = true;

	// ---- 多格式输出开关 ----
	bool wrBlwnd = true;
	bool wrTrbts = true;
	bool wrTrwnd = true;

	// ---- IEC 标准参数 ----
	IecStandard iecEdition = IecStandard::ED3;
	TurbineClass turbineClass = TurbineClass::Class_I;
	TurbulenceClass turbClass = TurbulenceClass::Class_B;
	double vRef = 0.0;
	double rotorDiameter = 0.0;

	// ---- 平均风与剪切参数 ----
	double meanWindSpeed = 0.0;
	double hubHeight = 0.0;
	double refHeight = -1.0; // -1 表示与 HubHt 相同
	ShearType shearType = ShearType::PL;
	WindProfileType windProfileType = WindProfileType::DEFAULT_PROFILE;
	double shearExp = 0.2;	 // 默认幂律指数
	double roughness = 0.01; // 地表粗糙度 (m)
	double horAngle = 0.0;
	double vertAngle = 0.0;
	std::string userShearFile; // 用户自定义剪切文件路径

	// ---- 网格与时域参数 ----
	double turbIntensity = 0.0;
	int turbSeed = 0;
	int gridPtsY = 0;
	int gridPtsZ = 0;
	double fieldDimY = 0.0;
	double fieldDimZ = 0.0;
	double simTime = 0.0;
	double timeStep = 0.0;
	bool cycleWind = false;

	// ---- 生成算法控制 ----
	GenMethod genMethod = GenMethod::AUTO;
	bool useFFT = false;
	InterpMethod interpMethod = InterpMethod::TRILINEAR;

	// ---- 用户自定义谱/时间序列文件 ----
	std::string userTurbFile; // 用户自定义谱或时间序列 .dat 文件路径
	bool useIECSimmga = false;
	int scaleIEC = -1; // -1 derives from UseIECSimmga; 0/1/2 follow TurbSim ScaleIEC.
	double etmC = 2.0;
	double usableTime = 0.0;
	double analysisTime = 0.0;
	double richardson = 0.0;
	double uStar = 0.0;
	double zOverL = 0.0;
	double mixingLayerDepth = 0.0;
	double reynoldsUW = 0.0;
	double reynoldsUV = 0.0;
	double reynoldsVW = 0.0;

	// ---- von Kármán / Bladed 纵向 (x) 长度尺度 (default 表示由程序根据标准计算) ----
	double vkLu = 0.0;
	double vkLv = 0.0;
	double vkLw = 0.0;

	// ---- Bladed 侧向 (y) 长度尺度 ----
	double vyLu = 0.0;
	double vyLv = 0.0;
	double vyLw = 0.0;

	// ---- Bladed 垂向 (z) 长度尺度 ----
	double vzLu = 0.0;
	double vzLv = 0.0;
	double vzLw = 0.0;

	// ---- Improved von Kármán 附加参数 ----
	double latitude = 0.0;
	double tiU = 0.0;
	double tiV = 0.0;
	double tiW = 0.0;

	// ---- Mann 参数 ----
	double mannAlphaEps = 0.0;
	double mannLength = 0.0; // MannScalelength
	double mannGamma = 0.0;
	double mannMaxL = 0.0;
	int mannNx = 0;
	int mannNy = 0;
	int mannNz = 0;

	// ---- 相干模型 ----
	CohModel cohMod1 = CohModel::DEFAULT_COH;
	CohModel cohMod2 = CohModel::DEFAULT_COH;
	CohModel cohMod3 = CohModel::DEFAULT_COH;

	// ---- 通用相干参数 ----
	double cohDecayU = 0.0;
	double cohDecayV = 0.0;
	double cohDecayW = 0.0;
	double cohScaleB = 0.0;
	double cohExp = 0.0;

	// ---- IEC 事件 / EWM 参数 ----
	EWMType ewmType = EWMType::Turbulent;
	int ewmReturn = 0; // 重现期 (年)
	double gustPeriod = 0.0;
	double eventStart = 0.0;
	EventSign eventSign = EventSign::POSITIVE;
	double ecdVcog = 0.0;

	// ---- 导入模式参数 ----
	std::string wndFilePath; // TurWindFile
	WndFormat wndFormat = WndFormat::BLADED_WND;

	// ---- 输出路径与文件名 ----
	std::string savePath; // WrWndPath
	std::string saveName; // WrWndName
	bool sumPrint = false;

	// ---- 批量模式参数 (Mode = BATCH) ----
	std::string batchExcelPath;
	std::string batchSheetName = "Cases";
	std::string batchOutputDir;
	int batchThreads = 0; // 0 = 自动
	std::string batchLauncher = "inproc";
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
	/// @brief 高度层数
	int numHeights = 0;

	/// @brief u 分量标准差缩放因子
	double stdScale1 = 1.0;
	/// @brief v 分量标准差缩放因子
	double stdScale2 = 1.0;
	/// @brief w 分量标准差缩放因子
	double stdScale3 = 1.0;

	/// @brief 高度 (m)
	std::vector<double> heights;
	/// @brief 风速 (m/s)
	std::vector<double> windSpeeds;
	/// @brief 风向 (deg, 逆时针)
	std::vector<double> windDirections;
	/// @brief 标准差 (m/s)
	std::vector<double> standardDeviations;
	/// @brief 长度尺度 (m)
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
	/// @brief 频率点数
	int numFrequencies = 0;

	/// @brief 频率 (Hz)
	std::vector<double> frequencies;
	/// @brief u 分量功率谱密度 (m^2/s)
	std::vector<double> uPsd;
	/// @brief v 分量功率谱密度 (m^2/s)
	std::vector<double> vPsd;
	/// @brief w 分量功率谱密度 (m^2/s)
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
	/// @brief 速度分量数（通常为 3: u, v, w）
	int nComp = 0;
	/// @brief 空间点数
	int nPoints = 0;
	/// @brief 参考点索引 (1-based)
	int refPtID = 0;

	/// @brief 空间点坐标列表
	std::vector<WindPoint> points;

	/// @brief 时间序列 (s)
	std::vector<double> time;

	/// @brief 风速分量矩阵: components[pointIndex][compIndex][timeIndex]
	/// 尺寸: nPoints × nComp × time.size()
	std::vector<std::vector<std::vector<double>>> components;
};
