#include "WindL/SimWind.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <fftw/fftw3.h>

#include "WindL/IO/WindL_IO_Subs.hpp"
#include "IO/LocaleString_WindL.hpp"

namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kRad = kPi / 180.0;
constexpr double kTiny = 1.0e-12;
constexpr double kHugeDecay = 1.0e9;
constexpr double kOmega = 7.2921159e-5;

/** @brief 雷诺应力目标设置，存储目标雷诺应力值及分量对应的省略标志 */
struct ReynoldsStressSetup
{
	std::array<double, 3> target{0.0, 0.0, 0.0};  ///< 目标雷诺应力值 (uu, uv=uw, vw) [N/m²]
	std::array<bool, 3> skip{true, true, true};   ///< 各分量对的跳过标志 (uu, uv, vw)，true 表示不参与合成
	bool active = false;                          ///< 是否启用雷诺应力约束合成
};

/** @brief 气象闭合参数包，汇总所有由风廓线与大气稳定度导出的气象学参数 */
struct MeteorologyClosure
{
	double richardson = 0.0;                                                   ///< 梯度理查德森数 (Ri)
	double zL = 0.0;                                                           ///< 无量纲稳定度参数 z/L
	double moninLength = std::numeric_limits<double>::infinity();               ///< Monin-Obukhov 长度 [m]，无穷大表示中性层结
	double uStar = 0.0;                                                        ///< 摩擦速度（中性层结） [m/s]
	double uStarDiab = 0.0;                                                    ///< 非绝热摩擦速度（考虑稳定度修正后） [m/s]
	double mixingLayerDepth = 0.0;                                             ///< 混合层高度（边界层厚度） [m]
	double coriolis = 0.0;                                                     ///< Coriolis 参数 [1/s]
	std::array<double, 3> defaultGeneralCohDecay{0.0, 0.0, 0.0};              ///< 默认广义相干衰减系数 (uu, uv, uw)
	std::array<double, 3> defaultGeneralCohB{0.0, 0.0, 0.0};                  ///< 默认广义相干指数参数 B (uu, uv, uw)
	ReynoldsStressSetup reynoldsStress;                                        ///< 雷诺应力目标设置
};

/** @brief 湍流风场仿真总配置，贯穿整个风场生成流水线，整合输入参数、网格划分、湍流模型、气象闭合及诊断信息 */
struct SimWindConfig
{
	// ---- 原始输入 ----
	WindLInput input;                                          ///< 原始输入参数结构体（用户/文件来源）
	// ---- 网格与时间维度 ----
	int ny = 0;                                                ///< Y 方向（水平横向）网格点数
	int nz = 0;                                                ///< Z 方向（垂直）网格点数
	int nPoints = 0;                                           ///< 空间点总数（ny × nz）
	int nSteps = 0;                                            ///< 时间步数
	int nFreq = 0;                                             ///< 频率点数（nSteps/2，Nyquist 折返）
	// ---- 时间参数 ----
	double dt = 0.0;                                           ///< 时间步长 [s]
	double duration = 0.0;                                     ///< 模拟总时长 [s]
	double df = 0.0;                                           ///< 频率分辨率 [Hz]（1/duration）
	// ---- 空间步长 ----
	double dy = 0.0;                                           ///< Y 方向网格间距 [m]
	double dz = 0.0;                                           ///< Z 方向网格间距 [m]
	double dx = 0.0;                                           ///< X 方向（顺风向）有效间距 [m]（Taylor 冻结假设）
	// ---- 网格几何 ----
	double gridWidth = 0.0;                                    ///< Y 方向网格总宽度 [m]
	double gridHeight = 0.0;                                   ///< Z 方向网格总高度 [m]
	double zBottom = 0.0;                                      ///< 网格底部高程 [m]
	// ---- 轮毂与参考高度参数 ----
	double hubHeight = 0.0;                                    ///< 轮毂高度 [m]
	double uHub = 0.0;                                         ///< 轮毂高度处的平均风速 [m/s]
	double refHeight = 0.0;                                    ///< 参考高度（用于风廓线推算） [m]
	// ---- 空间相关参数 ----
	double lambda = 0.0;                                       ///< 湍流积分尺度 Lam [m]（纵向分量）
	double lc = 0.0;                                           ///< 相干长度 Lc [m]
	double effectiveRotorDiameter = 0.0;                       ///< 等效风轮直径 [m]（用于 IEC 缩放）
	// ---- Mann 模型参数 ----
	double mannLength = 0.0;                                   ///< Mann 模型长度尺度 [m]
	double mannGamma = 0.0;                                    ///< Mann 模型各向异性参数 Gamma
	double mannMaxL = 0.0;                                     ///< Mann 模型最大涡尺度 Lmax [m]
	int mannFftPoints = 0;                                     ///< Mann 3D FFT 每方向点数
	int mannGridY = 0;                                         ///< Mann 3D FFT Y 方向网格数
	int mannGridZ = 0;                                         ///< Mann 3D FFT Z 方向网格数
	// ---- IEC 缩放 ----
	int scaleIEC = 0;                                          ///< IEC 湍流强度缩放模式 (0=不缩放, 1=按 IEC 标准缩放)
	// ---- 湍流统计量（标量） ----
	std::array<double, 3> sigma{0.0, 0.0, 0.0};               ///< 三个速度分量的标准差 (u, v, w) [m/s]
	std::array<double, 3> integralScale{0.0, 0.0, 0.0};       ///< 三向积分长度尺度 (Lu, Lv, Lw) [m]
	std::array<double, 3> lateralScale{0.0, 0.0, 0.0};        ///< 三向横向长度尺度 (yu_Lu, yv_Lv, yw_Lw) [m]
	std::array<double, 3> verticalScale{0.0, 0.0, 0.0};       ///< 三向垂向长度尺度 (zu_Lu, zv_Lv, zw_Lw) [m]
	// ---- 相干模型参数 ----
	std::array<double, 3> cohDecay{0.0, 0.0, 0.0};            ///< 相干衰减系数 (uu, uv, uw) [1/m]
	std::array<double, 3> cohB{0.0, 0.0, 0.0};                ///< 相干指数参数 B (uu, uv, uw)
	std::array<CohModel, 3> cohModel{CohModel::DEFAULT_COH, CohModel::DEFAULT_COH, CohModel::DEFAULT_COH};  ///< 各分量的相干模型类型
	std::array<bool, 3> useKronecker{false, false, false};    ///< 各分量是否使用 Kronecker 近似加速相关生成
	std::array<int, 3> kroneckerFreqLimit{0, 0, 0};           ///< Kronecker 模式下的频率截断点（0=自动）
	std::array<bool, 3> hasExplicitCohDecay{false, false, false};  ///< 各分量是否由用户显式指定相干衰减系数
	bool hasExplicitCohB = false;                              ///< 是否由用户显式指定相干指数 B
	// ---- 坐标与风廓线向量 ----
	std::vector<double> yCoords;                               ///< Y 坐标向量（水平横向） [m]
	std::vector<double> zCoords;                               ///< Z 坐标向量（垂直） [m]
	std::vector<double> meanUByZ;                              ///< 各 Z 高度的平均风速 [m/s]
	std::vector<double> y;                                     ///< 展开后的 Y 坐标（逐点） [m]
	std::vector<double> z;                                     ///< 展开后的 Z 坐标（逐点） [m]
	std::vector<double> meanU;                                 ///< 展开后的平均风速（逐点） [m/s]
	std::vector<double> directionByZ;                          ///< 各 Z 高度的风向 [deg]
	// ---- 分层湍流参数向量 ----
	std::array<std::vector<double>, 3> sigmaByZ;               ///< 各高度层的三个速度分量标准差 [m/s]
	std::array<std::vector<double>, 3> lengthScaleByZ;         ///< 各高度层的三向积分长度尺度 [m]
	std::array<std::vector<double>, 3> lateralScaleByZ;        ///< 各高度层的三向横向长度尺度 [m]
	std::array<std::vector<double>, 3> verticalScaleByZ;       ///< 各高度层的三向垂向长度尺度 [m]
	// ---- 输出路径 ----
	std::filesystem::path outputBase;                          ///< 输出文件的基路径（目录 + 前缀）
	// ---- 用户自定义数据 ----
	UserSpectraData userSpectra;                               ///< 用户自定义频谱数据（频率 × 分量矩阵）
	UserShearData userShear;                                   ///< 用户自定义风切变数据（高度 × 风速对）
	// ---- 用户数据可用标志 ----
	bool hasUserSpectra = false;                               ///< 是否提供了用户自定义频谱
	bool hasUserShear = false;                                 ///< 是否提供了用户自定义风切变
	bool hasUserDirectionProfile = false;                      ///< 是否提供了用户自定义风向廓线
	bool hasUserSigmaProfile = false;                          ///< 是否提供了用户自定义标准差廓线
	bool hasUserLengthScaleProfile = false;                    ///< 是否提供了用户自定义长度尺度廓线
	// ---- 分层参数标志 ----
	bool hasSigmaProfileByZ = false;                           ///< 是否存在按高度分层的标准差廓线
	bool hasLengthScaleProfileByZ = false;                     ///< 是否存在按高度分层的长度尺度廓线
	bool hasImprovedVkProfile = false;                         ///< 是否启用 Improved von Kármán 廓线模型
	// ---- 资源估算与诊断 ----
	double estimatedPeakMemoryGiB = 0.0;                       ///< 预估峰值内存占用 [GiB]
	double estimatedCholeskyFlops = 0.0;                       ///< 预估 Cholesky 分解浮点运算量 [FLOPs]
	int strictCoherenceComponents = 0;                         ///< 需要严格相干矩阵的湍流分量数（0-3）
	// ---- 子系统 ----
	MeteorologyClosure met;                                    ///< 气象闭合参数包
	SimWindProgressCallback progress;                          ///< 进度回调函数
	std::vector<std::string> warnings;                         ///< 配置构建过程中的警告信息列表
};

/** @brief 三维湍流风场容器，以交错存储方式管理 (u, v, w) 三个分量在全部时间步和空间点上的速度值 */
struct WindField
{
	int nSteps = 0;                                    ///< 时间步数
	int nPoints = 0;                                   ///< 空间点数
	std::array<std::vector<double>, 3> component;      ///< 三个速度分量向量，每个分量按 [step * nPoints + point] 交错存储

	/** @brief 获取指定分量在指定时间步和空间点处的风速值（可写引用）
	 *  @param comp  速度分量索引：0=u（纵向），1=v（横向），2=w（竖向）
	 *  @param step  时间步索引，范围 [0, nSteps-1]
	 *  @param point 空间点索引，范围 [0, nPoints-1]
	 *  @return      对应位置的可写引用，允许直接赋值修改风场
	 *  @note        内存布局为交错存储：component[comp][step * nPoints + point]，
	 *               其中第一维为时间步（步长 nPoints），第二维为空间点连续排列，
	 *               即同一时间步的所有空间点连续存放。
	 */
	double &At(int comp, int step, int point)
	{
		// 交错存储：每时间步内所有空间点连续排列，索引 = step × nPoints + point
		return component[static_cast<std::size_t>(comp)][static_cast<std::size_t>(step) * nPoints + point];
	}

	/** @brief 获取指定分量在指定时间步和空间点处的风速值（只读）
	 *  @param comp  速度分量索引：0=u（纵向），1=v（横向），2=w（竖向）
	 *  @param step  时间步索引，范围 [0, nSteps-1]
	 *  @param point 空间点索引，范围 [0, nPoints-1]
	 *  @return      对应位置的风速值（const 副本）
	 *  @note        与可写版本共享相同的内存布局：component[comp][step * nPoints + point]，
	 *               即按时间步优先、空间点连续的交错存储方式。
	 */
	double At(int comp, int step, int point) const
	{
		// 交错存储：每时间步内所有空间点连续排列，索引 = step × nPoints + point
		return component[static_cast<std::size_t>(comp)][static_cast<std::size_t>(step) * nPoints + point];
	}
};

/** @brief 批量一维 FFTW 逆变换计划 RAII 封装，管理复数数据缓冲区和 FFTW plan 的生命周期，提供频谱写入与时域实部读取接口 */
struct FftwBatchPlan1D
{
	int n = 0;                           ///< 每个变换的频点数
	int nTransforms = 0;                 ///< 批量变换个数
	fftw_complex *data = nullptr;        ///< FFTW 复数数据缓冲区，大小为 n × nTransforms（行列交错的 batch 布局）
	fftw_plan plan = nullptr;            ///< FFTW 逆 DFT 计划句柄（FFTW_BACKWARD，ESTIMATE 模式）

	/** @brief 构造批量一维 IFFT 计划，分配复数缓冲区并创建 FFTW 逆 DFT 计划
	 *  @param n           每个变换的频点数（通常为时间步数 nSteps）
	 *  @param nTransforms 批量变换个数（通常为空间点数 nPoints）
	 *  @throw             当 FFTW 内存分配或计划创建失败时抛出 std::runtime_error
	 *  @note  RAII 语义：构造时分配资源，析构时自动释放。
	 *         使用 fftw_plan_many_dft 创建批量计划：
	 *         - rank=1：一维 DFT（沿频点方向）
	 *         - howmany=nTransforms：同时执行 nTransforms 个独立的一维变换
	 *         - FFTW_BACKWARD：逆变换，将频域复数谱转换回时域实信号
	 *         - FFTW_ESTIMATE：快速计划生成，不追求极致性能优化
	 *         内存布局为行交错：data[step * nTransforms + transform]，
	 *         即每个频点 step 下连续排列 nTransforms 个空间点的复数值，
	 *         这种布局使得 stride=1 为空间点维度，适合批量 IFFT 的 howmany 参数。
	 *  @warning 该计划为 ESTIMATE 模式，未使用 FFTW_MEASURE，大型网格初次运行耗时可能偏长。
	 */
	FftwBatchPlan1D(int n, int nTransforms)
	    : n(n), nTransforms(nTransforms)
	{
		const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(nTransforms);
		data = static_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * total));
		if (data == nullptr)
			throw std::runtime_error(L_WIND_FFTWMallocFail);

		std::memset(data, 0, sizeof(fftw_complex) * total);
		// fftw_plan_many_dft 参数说明：
		//   rank=1           : 一维 DFT，每个变换沿频点方向
		//   dims={n}         : 每个一维变换的长度为 n
		//   howmany=nTransforms : 批量执行 nTransforms 个独立变换
		//   data (inembed)   : 输入数组，stride=1（空间点维连续），dist=nTransforms（频点间步长）
		//   data (onembed)   : 输出覆盖输入（原地变换），布局相同
		//   FFTW_BACKWARD    : 逆 DFT，频谱→时域
		//   FFTW_ESTIMATE    : 快速估算计划参数
		int dims[1] = {n};
		plan = fftw_plan_many_dft(1,
		                          dims,
		                          nTransforms,
		                          data,
		                          nullptr,
		                          nTransforms,
		                          1,
		                          data,
		                          nullptr,
		                          nTransforms,
		                          1,
		                          FFTW_BACKWARD,
		                          FFTW_ESTIMATE);
		if (plan == nullptr)
			throw std::runtime_error(L_WIND_FFTWPlanFail);
	}

	/** @brief 析构 FFTW 批量计划，销毁计划句柄并释放复数缓冲区
	 *  @note  RAII 资源清理：先销毁 FFTW plan（若存在），再释放 fftw_malloc 分配的内存。
	 *         对空指针安全——指针为空时跳过对应释放操作。
	 */
	~FftwBatchPlan1D()
	{
		if (plan != nullptr)
			fftw_destroy_plan(plan);
		if (data != nullptr)
			fftw_free(data);
	}

	/** @brief 禁止拷贝构造与拷贝赋值（RAII 资源独占所有权）
	 *  @note  FFTW plan 和 data 指针不可共享，拷贝会导致双重释放。
	 *         仅支持移动语义的场景请使用 std::unique_ptr 包装。
	 */
	FftwBatchPlan1D(const FftwBatchPlan1D &) = delete;
	FftwBatchPlan1D &operator=(const FftwBatchPlan1D &) = delete;

	/** @brief 将整个复数缓冲区清零（memset 为 0）
	 *  @note  数据清零后所有频点频谱幅值为零，适用于新分量合成前的初始化。
	 *          缓冲区大小为 sizeof(fftw_complex) × n × nTransforms 字节。
	 */
	void ZeroAll()
	{
		std::memset(data, 0, sizeof(fftw_complex) * static_cast<std::size_t>(n) * static_cast<std::size_t>(nTransforms));
	}

	/** @brief 执行批量逆 FFT（FFTW_BACKWARD），将频域复数谱转换回时域实信号
	 *  @note  调用 fftw_execute 触发原地逆变换，所有 nTransforms 个变换同时完成。
	 *          执行后 data 缓冲区内容由频域变为时域（未归一化），需手动除以 n 缩放。
	 *          该函数无返回值——结果直接写入 data 缓冲区。
	 */
	void Execute()
	{
		fftw_execute(plan);
	}

	/** @brief 向频谱缓冲区写入复数值（用于设置正频率点的湍流振幅）
	 *  @param step      频点索引（0 到 n-1），通常对应傅里叶频率 k
	 *  @param transform 空间点/变换索引（0 到 nTransforms-1）
	 *  @param value     要写入的复数值（包含幅值和相位信息）
	 *  @note  内存布局：index = step × nTransforms + transform。
	 *         频谱半共轭对称性要求：正频率 k 与负频率 N-k 处互为共轭，
	 *         即 SetSpectrum(k, p, value) 的同时应调用
	 *         SetSpectrum(N-k, p, std::conj(value)) 以确保 IFFT 输出为实数。
	 *         实部和虚部分别写入 data[index][0] 和 data[index][1]。
	 */
	void SetSpectrum(int step, int transform, const std::complex<double> &value)
	{
		// 半谱共轭对称：k 频点处的复幅值与 N-k 频点处的复幅值互为共轭
		// 调用方需确保对应负频率位置也被正确填充
		const std::size_t index = static_cast<std::size_t>(step) * static_cast<std::size_t>(nTransforms) + static_cast<std::size_t>(transform);
		data[index][0] = value.real();
		data[index][1] = value.imag();
	}

	/** @brief 读取 IFFT 后的时域实部值（用于从 data 中提取风速时间序列）
	 *  @param step      时间步索引（0 到 n-1），IFFT 后对应时域采样点
	 *  @param transform 空间点/变换索引（0 到 nTransforms-1）
	 *  @return          IFFT 输出在 (step, transform) 处的实部值（未归一化）
	 *  @note  读取的是 data[index][0]（复数的实部）；IFFT 后虚部理论上为零（舍入误差）。
	 *         调用方需手动乘以 1.0/n 进行归一化缩放。
	 *         内存布局与 SetSpectrum 相同：index = step × nTransforms + transform。
	 */
	double OutputReal(int step, int transform) const
	{
		const std::size_t index = static_cast<std::size_t>(step) * static_cast<std::size_t>(nTransforms) + static_cast<std::size_t>(transform);
		return data[index][0];
	}
};

/** @brief 三维 FFTW 复数体 RAII 封装，管理 Mann 三维湍流场 FFT 所需的复数数据缓冲区，提供按坐标读写及 3D 逆变换执行接口 */
struct FftwComplexVolume
{
	int nx = 0;                      ///< X 方向（顺风向）维度点数
	int ny = 0;                      ///< Y 方向（水平横向）维度点数
	int nz = 0;                      ///< Z 方向（垂直）维度点数
	fftw_complex *data = nullptr;    ///< FFTW 复数数据缓冲区，行优先 (ix, iy, iz) 布局，大小为 nx × ny × nz

	/** @brief 构造三维 FFTW 复数体，分配并清零 nx×ny×nz 个复数的缓冲区
	 *  @param nx X 方向（顺风向/时间维）维度点数
	 *  @param ny Y 方向（水平横向）维度点数
	 *  @param nz Z 方向（垂直）维度点数
	 *  @throw 当 FFTW 内存分配失败时抛出 std::runtime_error
	 *  @note   RAII 语义：构造时通过 fftw_malloc 分配 sizeof(fftw_complex)×nx×ny×nz 字节，
	 *          并 memset 清零。用于 Mann 3D 湍流谱的频域复振幅存储。
	 *          内存布局为行优先：(ix × ny + iy) × nz + iz。
	 */
	FftwComplexVolume(int nx, int ny, int nz)
	    : nx(nx), ny(ny), nz(nz)
	{
		const std::size_t total = static_cast<std::size_t>(nx) * ny * nz;
		data = static_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * total));
		if (data == nullptr)
			throw std::runtime_error(L_WIND_FFTWMalloc3D);
		std::memset(data, 0, sizeof(fftw_complex) * total);
	}

	/** @brief 析构复数体，释放 FFTW 分配的复数缓冲区
	 *  @note  RAII 资源清理：对空指针安全（data==nullptr 时跳过 free）。
	 */
	~FftwComplexVolume()
	{
		if (data != nullptr)
			fftw_free(data);
	}

	/** @brief 禁止拷贝构造与拷贝赋值（RAII 资源独占所有权）
	 *  @note  data 指针不可共享，拷贝会导致双重释放 fftw_free。
	 */
	FftwComplexVolume(const FftwComplexVolume &) = delete;
	FftwComplexVolume &operator=(const FftwComplexVolume &) = delete;

	/** @brief 将三维坐标 (ix, iy, iz) 映射为一维线性索引
	 *  @param ix X 方向索引，范围 [0, nx-1]
	 *  @param iy Y 方向索引，范围 [0, ny-1]
	 *  @param iz Z 方向索引，范围 [0, nz-1]
	 *  @return  一维线性索引，范围 [0, nx×ny×nz-1]
	 *  @note    行优先布局：(ix × ny + iy) × nz + iz，其中 iz 连续变化（最内层维度）。
	 *           即先按 ix、再按 iy、最后按 iz 排列，与 C 多维数组默认存储顺序一致。
	 */
	std::size_t Index(int ix, int iy, int iz) const
	{
		// 行优先存储：(ix × ny + iy) × nz + iz，iz 为最内层连续维度
		return (static_cast<std::size_t>(ix) * ny + static_cast<std::size_t>(iy)) * nz + static_cast<std::size_t>(iz);
	}

	/** @brief 设置三维指定位置的复数值
	 *  @param ix    X 方向索引
	 *  @param iy    Y 方向索引
	 *  @param iz    Z 方向索引
	 *  @param value 要写入的复数值
	 *  @note  写入实部到 data[index][0]、虚部到 data[index][1]。
	 *         内部通过 Index() 计算线性索引，遵循行优先存储。
	 */
	void Set(int ix, int iy, int iz, const std::complex<double> &value)
	{
		const std::size_t index = Index(ix, iy, iz);
		data[index][0] = value.real();
		data[index][1] = value.imag();
	}

	/** @brief 读取指定位置的实部值（用于从 IFFT 输出提取物理风速分量）
	 *  @param ix X 方向索引
	 *  @param iy Y 方向索引
	 *  @param iz Z 方向索引
	 *  @return  该位置的实部值（IFFT 后虚部理论上为零）
	 *  @note    读取 data[Index(ix,iy,iz)][0]；调用前需确保已执行 ExecuteBackward()。
	 *           返回值为未归一化的 IFFT 输出，需手动除以 nx×ny×nz 缩放。
	 */
	double Real(int ix, int iy, int iz) const
	{
		return data[Index(ix, iy, iz)][0];
	}

	/** @brief 创建并执行三维逆 FFT（FFTW_BACKWARD），然后立即销毁计划
	 *  @throw 当 FFTW 计划创建失败时抛出 std::runtime_error
	 *  @note  调用 fftw_plan_dft_3d 创建原地 3D 逆变换计划（ESTIMATE 模式），
	 *          执行 fftw_execute 后立即 fftw_destroy_plan 销毁计划。
	 *          执行后 data 缓冲区由频域转换为时域空间场。
	 *  @warning 每次调用均重新创建并销毁 FFTW 计划，是一种次优实现策略。
	 *           对于需要多次执行逆变换的场景（如迭代优化），应缓存 plan 句柄复用。
	 *           当前设计下反复调用会导致不必要的计划创建开销。
	 */
	void ExecuteBackward()
	{
		// 注意：每次调用都重新创建和销毁 3D FFTW plan（ESTIMATE 模式）
		// 这意味着重复调用会产生计划创建开销，不适合频繁使用场景
		fftw_plan plan = fftw_plan_dft_3d(nx, ny, nz, data, data, FFTW_BACKWARD, FFTW_ESTIMATE);
		if (plan == nullptr)
			throw std::runtime_error(L_WIND_FFTWPlan3D);
		fftw_execute(plan);
		fftw_destroy_plan(plan);
	}
};

/** @brief 将标量值以二进制形式写入输出流
 *  @param out 二进制输出流引用，流须处于正常状态
 *  @param value 要写入的标量值（POD 类型，如 int/double/float）
 *  @note 写入失败时抛出 std::runtime_error
 *  @code
 *  WriteScalar(stream, 3.14);
 *  @endcode
 */
template <typename T>
void WriteScalar(std::ofstream &out, T value)
{
	out.write(reinterpret_cast<const char *>(&value), sizeof(T));
	if (!out)
		throw std::runtime_error(L_WIND_BinaryWriteFail);
}

/** @brief 将二维网格坐标 (iz, iy) 转换为一维索引
 *  @param cfg 风场配置（取其 ny 维度）
 *  @param iz Z 方向（垂直）网格索引，范围 [0, nz-1]
 *  @param iy Y 方向（水平）网格索引，范围 [0, ny-1]
 *  @return 一维索引值 iz * ny + iy
 *  @note 按行主序（iy 连续）排列
 *  @code
 *  int idx = GridIndex(cfg, 2, 3);
 *  @endcode
 */
int GridIndex(const SimWindConfig &cfg, int iz, int iy)
{
	return iz * cfg.ny + iy;
}

/** @brief 返回正值，否则返回回退值
 *  @param value 待检查的数值
 *  @param fallback 当 value <= 0 时的回退值
 *  @return value > 0 ? value : fallback
 *  @code
 *  double v = ClampPositive(x, 1.0);
 *  @endcode
 */
double ClampPositive(double value, double fallback)
{
	return value > 0.0 ? value : fallback;
}

/** @brief 通过回调转发进度消息
 *  @param cfg 风场配置（持有 progress 回调函数指针）
 *  @param message 要转发的进度消息文本
 *  @note 若 cfg.progress 为空则静默丢弃消息
 *  @code
 *  Report(cfg, "正在计算湍流谱...");
 *  @endcode
 */
void Report(const SimWindConfig &cfg, const std::string &message)
{
	if (cfg.progress)
		cfg.progress(message);
}

/** @brief 检查高度剖面与数值剖面的尺寸是否匹配
 *  @param heights 高度序列
 *  @param values 对应高度上的数值序列
 *  @return 两地容器非空且长度相等时返回 true
 *  @code
 *  bool ok = HasProfileColumn(h, v);
 *  @endcode
 */
bool HasProfileColumn(const std::vector<double> &heights, const std::vector<double> &values)
{
	return !heights.empty() && heights.size() == values.size();
}

/** @brief 获取用户自定义标准差缩放因子
 *  @param data 用户剪切数据（包含三个分量的 stdScale）
 *  @param comp 分量索引：0=U, 1=V, 2=W
 *  @return 若对应分量缩放因子 > 0 则返回该值，否则返回 1.0
 *  @code
 *  double s = UserStdScale(shear, 0);
 *  @endcode
 */
double UserStdScale(const UserShearData &data, int comp)
{
	switch (comp)
	{
	case 0: return data.stdScale1 > 0.0 ? data.stdScale1 : 1.0;
	case 1: return data.stdScale2 > 0.0 ? data.stdScale2 : 1.0;
	default: return data.stdScale3 > 0.0 ? data.stdScale3 : 1.0;
	}
}

/** @brief 在高度-数值剖面上执行线性插值（前向声明，实现见后）
 *  @param heights 高度序列，须单调递增
 *  @param values 对应高度上的数值序列
 *  @param z 待插值的目标高度
 *  @return 插值结果
 */
double InterpolateProfile(const std::vector<double> &heights, const std::vector<double> &values, double z);
/** @brief 将角度归一化到 [0°, 360°) 区间
 *  @param value 输入角度（度），可为任意实数
 *  @return 归一化到 [0, 360) 的角度
 *  @code
 *  double a = NormalizeDirectionDegrees(-45.0); // 返回 315.0
 *  @endcode
 */
double NormalizeDirectionDegrees(double value)
{
	double wrapped = std::fmod(value, 360.0);
	if (wrapped < 0.0)
		wrapped += 360.0;
	return wrapped;
}

/** @brief 风向插值，处理圆周环绕（0°/360° 连续）
 *  @param heights 高度序列，须单调递增
 *  @param directions 对应各高度的风向角度（度）
 *  @param z 待插值的目标高度
 *  @return 插值后的风向角度，已归一化到 [0, 360)
 *  @note 插值时使用最短弧差，避免 359°→1° 绕大圈
 *  @code
 *  double wd = InterpolateWrappedDirection(h, dir, 30.0);
 *  @endcode
 */
double InterpolateWrappedDirection(const std::vector<double> &heights,
                                   const std::vector<double> &directions,
                                   double z)
{
	if (heights.empty() || directions.empty())
		return 0.0;
	const std::size_t n = std::min(heights.size(), directions.size());
	if (n == 1 || z <= heights.front())
		return NormalizeDirectionDegrees(directions.front());
	if (z >= heights[n - 1])
		return NormalizeDirectionDegrees(directions[n - 1]);

	const auto upper = std::upper_bound(heights.begin(), heights.begin() + static_cast<std::ptrdiff_t>(n), z);
	const std::size_t i1 = static_cast<std::size_t>(std::distance(heights.begin(), upper));
	const std::size_t i0 = i1 - 1;
	const double span = std::max(heights[i1] - heights[i0], kTiny);
	const double a = (z - heights[i0]) / span;
	double d0 = NormalizeDirectionDegrees(directions[i0]);
	double d1 = NormalizeDirectionDegrees(directions[i1]);
	double delta = d1 - d0;
	if (delta > 180.0)
		delta -= 360.0;
	else if (delta < -180.0)
		delta += 360.0;
	return NormalizeDirectionDegrees(d0 + a * delta);
}

/** @brief 向警告列表去重追加一条警告
 *  @param warnings 警告字符串列表（将被修改）
 *  @param text 要追加的警告文本
 *  @note 若 text 已在 warnings 中存在则忽略，保证同一条警告只出现一次
 *  @code
 *  AppendWarning(warnings, "剪切不满足...");
 *  @endcode
 */
void AppendWarning(std::vector<std::string> &warnings, const std::string &text)
{
	if (std::find(warnings.begin(), warnings.end(), text) == warnings.end())
		warnings.push_back(text);
}

/** @brief 获取可变的警告列表引用（const_cast 包装）
 *  @param cfg const 风场配置
 *  @return 内部 warnings 向量的非 const 引用
 *  @note 此函数为设计缺陷：通过 const_cast 突破 const 约束。仅用于向后兼容遗留代码
 *  @code
 *  auto &w = MutableWarnings(cfg);
 *  @endcode
 */
std::vector<std::string> &MutableWarnings(const SimWindConfig &cfg)
{
	return const_cast<std::vector<std::string> &>(cfg.warnings);
}

/** @brief 将双精度浮点数格式化为带两位小数的 GiB 字符串
 *  @param value 数值（以 GiB 为单位）
 *  @return 如 "1.50 GiB" 的格式化字符串
 *  @code
 *  std::string s = FormatGiB(1.5);
 *  @endcode
 */
std::string FormatGiB(double value)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << value << " GiB";
	return out.str();
}

/** @brief 将双精度浮点数格式化为科学计数字符串（3 位有效数字）
 *  @param value 要格式化的数值
 *  @return 如 "1.235e+06" 的科学计数字符串
 *  @code
 *  std::string s = FormatScientific(1234567.89);
 *  @endcode
 */
std::string FormatScientific(double value)
{
	std::ostringstream out;
	out << std::scientific << std::setprecision(3) << value;
	return out.str();
}

/** @brief 将秒数格式化为两位小数的定点数字符串
 *  @param value 秒数（双精度）
 *  @return 如 "15.23" 的定点数字符
 *  @code
 *  std::string s = FormatSeconds(15.23);
 *  @endcode
 */
std::string FormatSeconds(double value)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << value;
	return out.str();
}

/** @brief 将秒数格式化为人类可读的时长字符串 "Xd Yh Zm Ss"
 *  @param seconds 秒数，须 >= 0 且有限
 *  @return 如 "1d 2h 30m 15s" 的时长字符串，非法输入返回 "unknown"
 *  @note 高位为零的字段自动省略（如 120s → "2m 0s" 而非 "0d 0h 2m 0s"）
 *  @code
 *  std::string s = FormatDuration(90061.0);
 *  @endcode
 */
std::string FormatDuration(double seconds)
{
	if (!std::isfinite(seconds) || seconds < 0.0)
		return "unknown";

	const auto total = static_cast<long long>(std::llround(seconds));
	const long long days = total / 86400;
	const long long hours = (total % 86400) / 3600;
	const long long minutes = (total % 3600) / 60;
	const long long secs = total % 60;

	std::ostringstream out;
	if (days > 0)
		out << days << "d ";
	if (days > 0 || hours > 0)
		out << hours << "h ";
	if (days > 0 || hours > 0 || minutes > 0)
		out << minutes << "m ";
	out << secs << "s";
	return out.str();
}

/** @brief 根据 Cholesky 分解 FLOPs 估算初始运行时间范围
 *  @param flops Cholesky 分解的浮点运算量（FLOPs）
 *  @return "Xs to Ys" 形式的时间范围字符串，flops <= 0 返回 "0s"
 *  @note 快速端按 1e12 FLOP/s 估算，慢速端按 1e11 FLOP/s 估算
 *  @code
 *  std::string s = InitialRuntimeEstimate(1.0e15);
 *  @endcode
 */
std::string InitialRuntimeEstimate(double flops)
{
	if (flops <= 0.0)
		return "0s";

	const double slow = flops / 1.0e11;
	const double fast = flops / 1.0e12;
	return FormatDuration(fast) + " to " + FormatDuration(slow) + " for Cholesky work only";
}

/** @brief 计算偶数时间步数
 *  @param duration 模拟总时长（秒），须 >= 0
 *  @param dt 时间步长（秒），须 > 0
 *  @return 向上取整后的步数，最小返回 2
 *  @note 偶数步数有利于 FFT 计算
 *  @code
 *  int n = EvenStepCount(600.0, 0.05);
 *  @endcode
 */
int EvenStepCount(double duration, double dt)
{
	return std::max(2, static_cast<int>(std::ceil(duration / dt)));
}

/** @brief 将湍流强度值归一化为小数形式
 *  @param value 湍流强度，百分比（>1）或小数（<=1）
 *  @return 小数形式的湍流强度；若 value <= 0 返回 0
 *  @note >1 视为百分比（除以 100），<= 1 视为已归一化值
 *  @code
 *  double ti = FractionalTI(12.0); // 返回 0.12
 *  @endcode
 */
double FractionalTI(double value)
{
	if (value <= 0.0)
		return 0.0;
	return value > 1.0 ? value / 100.0 : value;
}

/** @brief 返回 IEC 湍流等级的参考湍流强度 Iref
 *  @param value 湍流等级枚举
 *  @return Class_A→0.16, Class_B→0.14, Class_C→0.12，其他默认 0.14
 *  @code
 *  double iref = TurbulenceClassIref(TurbulenceClass::Class_A);
 *  @endcode
 */
double TurbulenceClassIref(TurbulenceClass value)
{
	switch (value)
	{
	case TurbulenceClass::Class_A: return 0.16;
	case TurbulenceClass::Class_B: return 0.14;
	case TurbulenceClass::Class_C: return 0.12;
	default: return 0.14;
	}
}

/** @brief 返回风机等级的默认参考风速 Vref
 *  @param value 风机等级枚举
 *  @return Class_I→50.0, Class_II→42.5, Class_III→37.5, Class_S→50.0 m/s，默认 50.0
 *  @code
 *  double vref = DefaultVRef(TurbineClass::Class_I);
 *  @endcode
 */
double DefaultVRef(TurbineClass value)
{
	switch (value)
	{
	case TurbineClass::Class_I: return 50.0;
	case TurbineClass::Class_II: return 42.5;
	case TurbineClass::Class_III: return 37.5;
	case TurbineClass::Class_S: return 50.0;
	default: return 50.0;
	}
}

/** @brief IEC 50 年一遇极端风速 Ve50
 *  @param cfg 风场配置（取 cfg.input.vRef 及风机等级）
 *  @return Ve50 = 1.4 × max(vRef, DefaultVRef)，单位 m/s
 *  @note 当用户 vRef <= 0 时自动取默认等级参考值
 *  @code
 *  double ve50 = ExtremeWindSpeed50(cfg);
 *  @endcode
 */
double ExtremeWindSpeed50(const SimWindConfig &cfg)
{
	return 1.4 * ClampPositive(cfg.input.vRef, DefaultVRef(cfg.input.turbineClass));
}

/** @brief IEC 1 年一遇极端风速 Ve1
 *  @param cfg 风场配置
 *  @return Ve1 = 0.8 × Ve50，单位 m/s
 *  @code
 *  double ve1 = ExtremeWindSpeed1(cfg);
 *  @endcode
 */
double ExtremeWindSpeed1(const SimWindConfig &cfg)
{
	return 0.8 * ExtremeWindSpeed50(cfg);
}

/** @brief 根据风模型分派极风风速
 *  @param cfg 风场配置
 *  @param model 风模型枚举
 *  @return EWM1→Ve1, 其他（包括 EWM50）→Ve50
 *  @code
 *  double v = ExtremeWindSpeedForWindModel(cfg, WindModel::EWM50);
 *  @endcode
 */
double ExtremeWindSpeedForWindModel(const SimWindConfig &cfg, WindModel model)
{
	return model == WindModel::EWM1 ? ExtremeWindSpeed1(cfg) : ExtremeWindSpeed50(cfg);
}

/** @brief NTM 参考湍流标准差 Sigma1
 *  @param cfg 风场配置（取湍流等级、IEC 版本、轮毂处平均风速）
 *  @return 轮毂高度处的参考湍流标准差，单位 m/s
 *  @note ED2 版本使用轮毂风速线性模型；其他版本使用 Iref×K 公式
 *  @code
 *  double s1 = ReferenceSigma1Ntm(cfg);
 *  @endcode
 */
double ReferenceSigma1Ntm(const SimWindConfig &cfg)
{
	const double iref = TurbulenceClassIref(cfg.input.turbClass);
	if (cfg.input.iecEdition == IecStandard::ED2)
	{
		const double slope = cfg.input.turbClass == TurbulenceClass::Class_A ? 2.0 : 3.0;
		const double ti15 = cfg.input.turbClass == TurbulenceClass::Class_A ? 0.18 : 0.16;
		return ti15 * ((15.0 + slope * cfg.uHub) / (slope + 1.0));
	}
	return iref * (0.75 * cfg.uHub + 5.6);
}

/** @brief IEC 极端运行阵风 (EOG) 幅值
 *  @param cfg 风场配置（取 Sigma1、Ve1、轮毂风速等）
 *  @return EOG 阵风幅值 Veag = min(1.35×(Ve1-Uhub), 3.3×Sigma1/denom)，单位 m/s
 *  @code
 *  double amp = ExtremeOperatingGustAmplitude(cfg);
 *  @endcode
 */
double ExtremeOperatingGustAmplitude(const SimWindConfig &cfg)
{
	const double sigma1 = ReferenceSigma1Ntm(cfg);
	const double denom = 1.0 + 0.1 * cfg.effectiveRotorDiameter / std::max(cfg.lambda, kTiny);
	const double a = 1.35 * (ExtremeWindSpeed1(cfg) - cfg.uHub);
	const double b = 3.3 * sigma1 / denom;
	return std::min(a, b);
}

/** @brief IEC 极端风向变化 (EDC) 角度
 *  @param cfg 风场配置
 *  @return EDC 风向变化角 θe = min(4×atan(Sigma1/denom), 180°)，已转回度
 *  @code
 *  double deg = ExtremeDirectionChangeDegrees(cfg);
 *  @endcode
 */
double ExtremeDirectionChangeDegrees(const SimWindConfig &cfg)
{
	const double sigma1 = ReferenceSigma1Ntm(cfg);
	const double denom = cfg.uHub * (1.0 + 0.1 * cfg.effectiveRotorDiameter / std::max(cfg.lambda, kTiny));
	const double theta = 4.0 * std::atan(sigma1 / std::max(denom, kTiny)) / kRad;
	return std::min(theta, 180.0);
}

/** @brief 判断是否为 Von Karman 型湍流模型
 *  @param value 湍流模型枚举
 *  @return 当为 IEC_VKAIMAL / B_VKAL / B_IVKAL / USRVKM 时返回 true
 *  @code
 *  bool vk = IsVonKarman(TurbModel::IEC_VKAIMAL);
 *  @endcode
 */
bool IsVonKarman(TurbModel value)
{
	return value == TurbModel::IEC_VKAIMAL ||
	       value == TurbModel::B_VKAL ||
	       value == TurbModel::B_IVKAL ||
	       value == TurbModel::USRVKM;
}

/** @brief 判断是否为 Mann 湍流模型
 *  @param value 湍流模型枚举
 *  @return B_MANN 返回 true
 *  @code
 *  bool mann = IsMann(TurbModel::B_MANN);
 *  @endcode
 */
bool IsMann(TurbModel value)
{
	return value == TurbModel::B_MANN;
}

/** @brief 判断是否为改进型 Von Karman 模型
 *  @param value 湍流模型枚举
 *  @return B_IVKAL 返回 true
 *  @code
 *  bool ivk = IsImprovedVonKarman(TurbModel::B_IVKAL);
 *  @endcode
 */
bool IsImprovedVonKarman(TurbModel value)
{
	return value == TurbModel::B_IVKAL;
}

/** @brief 构建 Mann 盒重复警告信息
 *  @param cfg 风场配置（取 MannNx、nSteps、duration 等）
 *  @return 描述 Mann 盒时间维度不足的警告文本
 *  @note 当 MannNx < nSteps 时，合成风场会周期性循环
 *  @code
 *  std::string w = BuildMannRepeatWarning(cfg);
 *  @endcode
 */
std::string BuildMannRepeatWarning(const SimWindConfig &cfg)
{
	std::ostringstream warning;
	warning << "MannNx=" << cfg.mannFftPoints
	        << " is smaller than the resolved output step count " << cfg.nSteps
	        << " for duration " << ZString::FormatDouble(cfg.duration)
	        << " s at dt=" << ZString::FormatDouble(cfg.dt)
	        << " s; the synthesized Mann box will repeat every "
	        << ZString::FormatDouble(cfg.mannFftPoints * cfg.dt)
	        << " s because the time sampling wraps with t % MannNx. Increase MannNx to at least "
	        << cfg.nSteps << " (preferably a power of two above that value) to avoid periodic repetition.";
	return warning.str();
}

using Matrix3 = std::array<std::array<double, 3>, 3>;

/** @brief Improved von Kármán 湍流廓线单点参数，存储特定高度处的三向湍流标准差、三向长度尺度及边界层特征参数 */
struct ImprovedVkProfilePoint
{
	bool valid = false;                      ///< 该点廓线计算是否有效（风能领域公式适用范围内）
	double sigma[3]{0.0, 0.0, 0.0};         ///< 三个速度分量的标准差 (u, v, w) [m/s]
	double xScale[3]{0.0, 0.0, 0.0};        ///< 三个分量的纵向（x 方向）湍流长度尺度 [m]
	double yScale[3]{0.0, 0.0, 0.0};        ///< 三个分量的横向（y 方向）湍流长度尺度 [m]
	double zScale[3]{0.0, 0.0, 0.0};        ///< 三个分量的垂向（z 方向）湍流长度尺度 [m]
	double a = 1.0;                          ///< 轴向感应因子 a（用于风轮平面风速修正）
	double boundaryLayerHeight = 0.0;        ///< 大气边界层高度 [m]
	double frictionVelocity = 0.0;           ///< 摩擦速度 u* [m/s]
};

/** @brief 返回全零的 3×3 矩阵
 *  @return 所有元素均为 0.0 的 3×3 矩阵
 *  @code
 *  auto m = ZeroMatrix3();
 *  @endcode
 */
Matrix3 ZeroMatrix3()
{
	return {{{0.0, 0.0, 0.0},
	         {0.0, 0.0, 0.0},
	         {0.0, 0.0, 0.0}}};
}

/** @brief 获取雷诺应力张量分量修正目标值（仅 uw/uv/vw 有意义）
 *  @param setup 雷诺应力配置（持有各分量 target 值）
 *  @param compA 第一个分量索引（0=u, 1=v, 2=w）
 *  @param compB 第二个分量索引
 *  @return 对应分量的 target 值（跨分量映射到固定的 0/1/2 三个位置），对角分量返回 0
 *  @code
 *  double t = TargetReynoldsStress(rs, 0, 2); // renoldsUW target
 *  @endcode
 */
double TargetReynoldsStress(const ReynoldsStressSetup &setup, int compA, int compB)
{
	if ((compA == 0 && compB == 2) || (compA == 2 && compB == 0))
		return setup.target[0];
	if ((compA == 0 && compB == 1) || (compA == 1 && compB == 0))
		return setup.target[1];
	if ((compA == 1 && compB == 2) || (compA == 2 && compB == 1))
		return setup.target[2];
	return 0.0;
}

/** @brief 检查某分量对是否已设置了显式雷诺应力目标
 *  @param setup 雷诺应力配置（持有 skip 标志）
 *  @param compA 第一个分量索引
 *  @param compB 第二个分量索引
 *  @return 若对应分量未跳过则返回 true（即存在目标值）
 *  @code
 *  bool has = HasTargetReynoldsStress(rs, 0, 2);
 *  @endcode
 */
bool HasTargetReynoldsStress(const ReynoldsStressSetup &setup, int compA, int compB)
{
	if ((compA == 0 && compB == 2) || (compA == 2 && compB == 0))
		return !setup.skip[0];
	if ((compA == 0 && compB == 1) || (compA == 1 && compB == 0))
		return !setup.skip[1];
	if ((compA == 1 && compB == 2) || (compA == 2 && compB == 1))
		return !setup.skip[2];
	return false;
}

/** @brief 判断是否为 EWM 极端风速模型
 *  @param model 风模型枚举
 *  @return EWM1 或 EWM50 时返回 true
 *  @code
 *  bool ewm = IsEwmWindModel(WindModel::EWM50);
 *  @endcode
 */
bool IsEwmWindModel(WindModel model)
{
	return model == WindModel::EWM1 || model == WindModel::EWM50;
}

/** @brief 判断是否为稳态 EWM 风模型
 *  @param input 风场输入参数
 *  @return EWM 模型且 ewmType == Steady 时返回 true
 *  @code
 *  bool s = IsSteadyEwmWindModel(input);
 *  @endcode
 */
bool IsSteadyEwmWindModel(const WindLInput &input)
{
	return IsEwmWindModel(input.windModel) && input.ewmType == EWMType::Steady;
}

/** @brief 判断是否为 IEC 标准谱模型（Kaimal / Von Karman）
 *  @param value 湍流模型枚举
 *  @return IEC_KAIMAL 或 IEC_VKAIMAL 时返回 true
 *  @code
 *  bool iec = IsIecSpectralModel(TurbModel::IEC_KAIMAL);
 *  @endcode
 */
bool IsIecSpectralModel(TurbModel value)
{
	return value == TurbModel::IEC_KAIMAL || value == TurbModel::IEC_VKAIMAL;
}

/** @brief 判断是否需要频谱湍流合成（前向声明，实现见后）
 *  @param input 风场输入参数
 *  @return 非确定性事件风模型且非均匀风模型时返回 true
 */
bool IsSyntheticStochasticModel(const WindLInput &input);

/** @brief 大气稳定度函数 Ψm（动量）
 *  @param zOverL 无量纲稳定度参数 z/L
 *  @return Ψm 函数值；稳定层结 (z/L≥0) 返回 5×min(z/L,1)；不稳定层结返回对数积分近似
 *  @note 适用 Businger-Dyer 型关系
 *  @code
 *  double psi = StabilityPsiM(-0.5);
 *  @endcode
 */
double StabilityPsiM(double zOverL)
{
	if (zOverL >= 0.0)
		return -5.0 * std::min(zOverL, 1.0);

	const double tmp = std::pow(1.0 - 15.0 * zOverL, 0.25);
	double psiM = -std::log(0.125 * std::pow(1.0 + tmp, 2.0) * (1.0 + tmp * tmp)) +
	              2.0 * std::atan(tmp) - 0.5 * kPi;
	return -psiM;
}

/** @brief 由高度和 z/L 反算 Monin-Obukhov 长度
 *  @param hubHeight 参考高度（通常为轮毂高度），单位 m
 *  @param zL 无量纲参数 z/L
 *  @return L = hubHeight / zL；当 |zL| 趋近于零时返回无穷大（中性层结）
 *  @code
 *  double L = MoninLengthFromZL(90.0, -0.1);
 *  @endcode
 */
double MoninLengthFromZL(double hubHeight, double zL)
{
	if (std::abs(zL) <= kTiny)
		return std::numeric_limits<double>::infinity();
	return hubHeight / zL;
}

/** @brief 计算指定高度处的 z/L 参数
 *  @param height 目标高度，单位 m
 *  @param moninLength Monin-Obukhov 长度 L（非有限值或趋于零按中性处理）
 *  @return height / L；中性层结返回 0
 *  @code
 *  double zl = ZOverLAtHeight(50.0, 200.0);
 *  @endcode
 */
double ZOverLAtHeight(double height, double moninLength)
{
	if (!std::isfinite(moninLength) || std::abs(moninLength) <= kTiny)
		return 0.0;
	return height / moninLength;
}

/** @brief 由 Richardson 数推导 z/L
 *  @param richardson 梯度 Richardson 数 Ri
 *  @return z/L 近似值：Ri≤0→Ri；0<Ri<0.167→Ri/(1-5Ri)；Ri≥0.167→1.0
 *  @code
 *  double zl = DeriveZLFromRichardson(0.1);
 *  @endcode
 */
double DeriveZLFromRichardson(double richardson)
{
	if (richardson <= 0.0)
		return richardson;
	if (richardson < 0.16667)
		return std::min(richardson / (1.0 - 5.0 * richardson), 1.0);
	return 1.0;
}

/** @brief 非绝热对数风廓线：由参考高度风速推算目标高度风速
 *  @param height 目标高度，单位 m
 *  @param refHeight 参考高度，单位 m
 *  @param refSpeed 参考高度处风速，单位 m/s
 *  @param roughness 空气动力学粗糙长度 z0，单位 m
 *  @param moninLength Monin-Obukhov 长度 L，单位 m
 *  @return 目标高度处风速，单位 m/s；参数非法时返回 0
 *  @code
 *  double u = DiabaticLogWindSpeed(120.0, 90.0, 10.0, 0.03, 200.0);
 *  @endcode
 */
double DiabaticLogWindSpeed(double height,
                            double refHeight,
                            double refSpeed,
                            double roughness,
                            double moninLength)
{
	if (height <= 0.0 || refHeight <= 0.0 || roughness <= 0.0)
		return 0.0;
	const double z0 = std::clamp(roughness, 1.0e-5, refHeight * 0.95);
	const double psiHt = StabilityPsiM(ZOverLAtHeight(height, moninLength));
	const double psiRef = StabilityPsiM(ZOverLAtHeight(refHeight, moninLength));
	const double denom = std::log(refHeight / z0) - psiRef;
	if (std::abs(denom) <= kTiny)
		return 0.0;
	return refSpeed * (std::log(height / z0) - psiHt) / denom;
}

/** @brief 非绝热摩擦速度 u*（动量通量尺度）
 *  @param uRef 参考高度风速，单位 m/s
 *  @param refHeight 参考高度，单位 m
 *  @param roughness 粗糙长度 z0，单位 m
 *  @param zLAtRef 参考高度处的 z/L
 *  @return u* = 0.4·uRef / (ln(refHeight/z0) - Ψm)，单位 m/s；参数非法返回 0
 *  @code
 *  double us = UstarDiabatic(10.0, 90.0, 0.03, -0.1);
 *  @endcode
 */
double UstarDiabatic(double uRef, double refHeight, double roughness, double zLAtRef)
{
	const double z0 = std::clamp(roughness, 1.0e-5, refHeight * 0.95);
	const double psiRef = StabilityPsiM(zLAtRef);
	const double denom = std::log(refHeight / z0) - psiRef;
	if (uRef <= 0.0 || refHeight <= 0.0 || std::abs(denom) <= kTiny)
		return 0.0;
	return 0.4 * uRef / denom;
}

/** @brief 估算大气边界层默认混合层高度
 *  @param uStar 摩擦速度，单位 m/s
 *  @param uStarDiab 非绝热摩擦速度，单位 m/s
 *  @param uRef 参考风速，单位 m/s
 *  @param refHeight 参考高度，单位 m
 *  @param roughness 粗糙长度，单位 m
 *  @param coriolis 科氏参数 f，单位 1/s
 *  @return 混合层高度，单位 m
 *  @note 稳定层结 (uStar<uStarDiab) 或无科氏力时使用经验公式；不稳定层结且有科氏力时使用 Ekman 层理论 h = uStar / (6f)
 *  @code
 *  double h = DefaultMixingLayerDepth(0.5, 0.6, 10.0, 90.0, 0.03, 1.0e-4);
 *  @endcode
 */
double DefaultMixingLayerDepth(double uStar,
                               double uStarDiab,
                               double uRef,
                               double refHeight,
                               double roughness,
                               double coriolis)
{
	const double z0 = std::clamp(roughness, 1.0e-5, refHeight * 0.95);
	if (uStar < uStarDiab || std::abs(coriolis) <= 1.0e-8)
		return (0.04 * uRef) / (1.0e-4 * std::max(std::log10(refHeight / z0), 1.0e-3));
	return uStar / (6.0 * std::abs(coriolis));
}

/** @brief 检查是否具备改进 Von Karman 大气边界层输入
 *  @param input 风场输入参数
 *  @return roughness > 0 且 |latitude| > 0.001 时返回 true
 *  @code
 *  bool ok = HasImprovedVkAtmosphericInputs(input);
 *  @endcode
 */
bool HasImprovedVkAtmosphericInputs(const WindLInput &input)
{
	return input.roughness > 0.0 && std::abs(input.latitude) > 1.0e-3;
}

/** @brief 改进型 Von Karman 模型摩擦速度
 *  @param meanU 平均风速，单位 m/s
 *  @param z 计算高度，单位 m
 *  @param roughness 粗糙长度，单位 m
 *  @param coriolis 科氏参数绝对值，单位 1/s
 *  @return u* = (0.4·U - 34.5·f·z) / ln(z/z0)，单位 m/s；负值时截断返回 0
 *  @note 包含 Ekman 层修正项 -34.5·f·z
 *  @code
 *  double us = ImprovedVkFrictionVelocity(10.0, 90.0, 0.03, 1.0e-4);
 *  @endcode
 */
double ImprovedVkFrictionVelocity(double meanU, double z, double roughness, double coriolis)
{
	const double zEval = std::max(z, roughness * 1.01);
	const double denom = std::log(std::max(zEval / std::max(roughness, 1.0e-5), 1.000001));
	if (meanU <= 0.0 || denom <= kTiny)
		return 0.0;
	const double value = (0.4 * meanU - 34.5 * std::abs(coriolis) * zEval) / denom;
	return std::max(value, 0.0);
}

/** @brief 计算改进型 Von Karman 模型在单高度上的湍流参数
 *  @param cfg 风场配置（取输入参数、轮毂信息）
 *  @param z 计算高度，单位 m
 *  @param meanU 该高度的平均风速，单位 m/s
 *  @return ImprovedVkProfilePoint 结构体，包含三个分量的 Sigma、三向长度尺度、边界层参数等
 *  @note 实现 ESDU 85020 / IEC 61400-1 Annex B 改进 Von Karman 剖面模型
 *  @code
 *  auto pt = ComputeImprovedVkPoint(cfg, 90.0, 10.0);
 *  @endcode
 */
ImprovedVkProfilePoint ComputeImprovedVkPoint(const SimWindConfig &cfg, double z, double meanU)
{
	ImprovedVkProfilePoint point;
	if (!HasImprovedVkAtmosphericInputs(cfg.input))
		return point;

	const double z0 = std::max(cfg.input.roughness, 1.0e-5);
	const double zEval = std::max(z, z0 * 1.01);
	const double coriolis = 2.0 * kOmega * std::sin(std::abs(cfg.input.latitude) * kRad);
	const double coriolisAbs = std::abs(coriolis);
	if (coriolisAbs <= 1.0e-8 || meanU <= 0.1)
		return point;

	const double uStar = ImprovedVkFrictionVelocity(meanU, zEval, z0, coriolisAbs);
	if (uStar <= kTiny)
		return point;

	const double boundaryLayerHeight = uStar / (6.0 * coriolisAbs);
	if (boundaryLayerHeight <= zEval + kTiny)
		return point;

	const double eta = std::clamp(1.0 - 6.0 * coriolisAbs * zEval / uStar, 0.05, 1.0);
	const double p = std::pow(eta, 16.0);
	const double logZ = std::log(std::max(zEval / z0, 1.000001));
	const double R = std::max(uStar / (coriolisAbs * z0), 1.000001);
	const double sigmaU = uStar * 7.5 * eta * std::pow(std::max(0.538 + 0.09 * logZ, 1.0e-6), p) /
	                      std::max(1.0 + 0.156 * std::log(R), 1.0e-6);
	if (sigmaU <= kTiny)
		return point;

	const double zh = std::clamp(zEval / boundaryLayerHeight, 1.0e-6, 0.999);
	const double cosTerm = std::pow(std::cos(0.5 * kPi * zh), 4.0);
	const double iu = sigmaU / meanU;
	const double iv = iu * (1.0 - 0.22 * cosTerm);
	const double iw = iu * (1.0 - 0.45 * cosTerm);

	const double k0 = 0.39 / std::pow(R, 0.11);
	const double b = 24.0 * std::pow(R, 0.155);
	const double n = 1.24 * std::pow(R, 0.008);
	const double kz = 0.19 - (0.19 - k0) * std::exp(-b * std::pow(zh, n));
	const double a = 0.535 + 2.76 * std::pow(std::max(0.138 - 0.115 * std::pow(1.0 + 0.315 * std::pow(1.0 - zh, 6.0), 2.0 / 3.0), 0.0), 0.68);
	const double factorA = 0.115 * std::pow(1.0 + 0.315 * std::pow(1.0 - zh, 6.0), 2.0 / 3.0);
	const double luX = factorA > 0.0 && kz > 0.0
	                       ? std::pow(factorA, 1.5) * (sigmaU / uStar) * zEval /
	                             std::max(2.5 * std::pow(kz, 1.5) * std::pow(1.0 - zh, 2.0) * (1.0 + 5.75 * zh), 1.0e-6)
	                       : 0.0;
	if (luX <= kTiny || !std::isfinite(luX))
		return point;

	const double luY = 0.5 * luX * (1.0 - 0.68 * std::exp(-35.0 * zh));
	const double luZ = 0.5 * luX * (1.0 - 0.46 * std::exp(-35.0 * std::pow(zh, 1.7)));
	const double ratioV = std::pow(std::max(iv / std::max(iu, 1.0e-9), 1.0e-6), 3.0);
	const double ratioW = std::pow(std::max(iw / std::max(iu, 1.0e-9), 1.0e-6), 3.0);

	point.valid = true;
	point.boundaryLayerHeight = boundaryLayerHeight;
	point.frictionVelocity = uStar;
	point.a = std::max(a, 1.0e-3);
	point.sigma[0] = cfg.input.tiU > 0.0 ? cfg.input.tiU * 0.01 * meanU : sigmaU;
	point.sigma[1] = cfg.input.tiV > 0.0 ? cfg.input.tiV * 0.01 * meanU : iv * meanU;
	point.sigma[2] = cfg.input.tiW > 0.0 ? cfg.input.tiW * 0.01 * meanU : iw * meanU;
	point.xScale[0] = cfg.input.vkLu > 0.0 ? cfg.input.vkLu : luX;
	point.yScale[0] = cfg.input.vyLu > 0.0 ? cfg.input.vyLu : std::max(luY, kTiny);
	point.zScale[0] = cfg.input.vzLu > 0.0 ? cfg.input.vzLu : std::max(luZ, kTiny);
	point.xScale[1] = cfg.input.vkLv > 0.0 ? cfg.input.vkLv : std::max(0.5 * luX * ratioV, kTiny);
	point.yScale[1] = cfg.input.vyLv > 0.0 ? cfg.input.vyLv : std::max(2.0 * luY * ratioV, kTiny);
	point.zScale[1] = cfg.input.vzLv > 0.0 ? cfg.input.vzLv : std::max(luZ * ratioV, kTiny);
	point.xScale[2] = cfg.input.vkLw > 0.0 ? cfg.input.vkLw : std::max(0.5 * luX * ratioW, kTiny);
	point.yScale[2] = cfg.input.vyLw > 0.0 ? cfg.input.vyLw : std::max(luY * ratioW, kTiny);
	point.zScale[2] = cfg.input.vzLw > 0.0 ? cfg.input.vzLw : std::max(2.0 * luZ * ratioW, kTiny);
	return point;
}

/** @brief 返回默认的IEC风廓线指数。
    @param cfg SimWind配置对象，用于判断风模型和IEC版本。
    @return 风廓线指数 (EWM: 0.11, ED3/ED4: 0.14, 其他: 0.2)。
    @note 指数用于幂律风廓线计算：u(z) = uHub * (z/zRef)^exponent。
    @code
    double exp = DefaultIecProfileExponent(cfg); // 0.11, 0.14, or 0.2
    @endcode */
double DefaultIecProfileExponent(const SimWindConfig &cfg)
{
	if (IsSteadyEwmWindModel(cfg.input))
		return 0.11;
	if (cfg.input.iecEdition == IecStandard::ED3 || cfg.input.iecEdition == IecStandard::ED4)
		return 0.14;
	return 0.2;
}

/** @brief 检查指定湍流分量是否启用。
    @param input WindLInput输入参数。
    @param comp 分量索引 (0=u, 1=v, 2=w)。
    @return 如果对应分量已启用返回true，否则返回false。
    @code
    if (ComponentEnabled(input, 0)) { u分量已启用  }
    @endcode */
bool ComponentEnabled(const WindLInput &input, int comp)
{
	if (comp == 0)
		return input.calWu;
	if (comp == 1)
		return input.calWv;
	return input.calWw;
}

/** @brief 检查是否为确定性事件风模型 (EOG/EDC/ECD/EWS)。
    @param model 风模型枚举值。
    @return 如果模型是EOG、EDC、ECD或EWS之一返回true。
    @note 确定性事件模型不需要湍流生成。
    @code
    if (IsDeterministicEventWindModel(model)) {  确定性事件  }
    @endcode */
bool IsDeterministicEventWindModel(WindModel model)
{
	return model == WindModel::EOG ||
	       model == WindModel::EDC ||
	       model == WindModel::ECD ||
	       model == WindModel::EWS;
}

/** @brief 检查是否为均匀风模型。
    @param model 风模型枚举值。
    @return 如果模型为UNIFORM返回true。
    @code
    if (IsUniformWindModel(model)) { 均匀风  }
    @endcode */
bool IsUniformWindModel(WindModel model)
{
	return model == WindModel::UNIFORM;
}

/** @brief 获取稳态极限风模型(EWM)的轮毂高度风速。
    @param cfg SimWind配置对象。
    @return 极限风速对应的轮毂高度风速值。
    @note 调用 ExtremeWindSpeedForWindModel 计算。
    @code
    double vHub = SteadyEwmHubSpeed(cfg);
    @endcode */
double SteadyEwmHubSpeed(const SimWindConfig &cfg)
{
	return ExtremeWindSpeedForWindModel(cfg, cfg.input.windModel);
}

/** @brief 检查是否使用谱方法生成湍流。
    @param input WindLInput输入参数。
    @return 如果不是确定性事件模型、非均匀风和用户风速，则返回true。
    @note 谱方法需要频率域合成。
    @code
    if (UsesSpectralTurbulenceGeneration(input)) {  谱方法  }
    @endcode */
bool UsesSpectralTurbulenceGeneration(const WindLInput &input)
{
	return !IsDeterministicEventWindModel(input.windModel) &&
	       !IsUniformWindModel(input.windModel) &&
	       input.turbModel != TurbModel::USER_WIND_SPEED;
}

/** @brief 检查是否为合成随机模型（等同 UsesSpectralTurbulenceGeneration）。
    @param input WindLInput输入参数。
    @return 如果使用谱湍流生成返回true。
    @code
    if (IsSyntheticStochasticModel(input)) { // 随机合成
    @endcode */
bool IsSyntheticStochasticModel(const WindLInput &input)
{
	return UsesSpectralTurbulenceGeneration(input);
}

/** @brief 检查是否显式设置了雷诺应力目标值。
    @param value 雷诺应力值。
    @return 如果绝对值大于kTiny返回true。
    @note 零值或极小值视为未显式设置。
    @code
    if (HasExplicitReynoldsTarget(input.reynoldsUW)) { // 显式设置
    @endcode */
bool HasExplicitReynoldsTarget(double value)
{
	return std::abs(value) > kTiny;
}

/** @brief 检查是否有任何气象输入提示（理查森数、摩擦速度、z/L等）。
    @param input WindLInput输入参数。
    @return 如果至少有一个气象参数被设置返回true。
    @note 用于判断是否需要气象修正。
    @code
    if (HasAnyMeteorologyHints(input)) { // 存在气象输入
    }
    @endcode */
bool HasAnyMeteorologyHints(const WindLInput &input)
{
	return std::abs(input.richardson) > kTiny ||
	       input.uStar > 0.0 ||
	       std::abs(input.zOverL) > kTiny ||
	       input.mixingLayerDepth > 0.0 ||
	       HasExplicitReynoldsTarget(input.reynoldsUW) ||
	       HasExplicitReynoldsTarget(input.reynoldsUV) ||
	       HasExplicitReynoldsTarget(input.reynoldsVW);
}

/** @brief 检查某分量是否使用严格相干模型。
    @param cfg SimWind配置对象。
    @param comp 分量索引 (0=u, 1=v, 2=w)。
    @return 如果分量使用谱生成、已启用且设置了非NONE的相干模型返回true。
    @note API相干模型仅对u分量有效；相干衰减系数极大时视为不使用。
    @code
    bool strict = UsesStrictCoherence(cfg, 0);
    @endcode */
bool UsesStrictCoherence(const SimWindConfig &cfg, int comp)
{
	if (!UsesSpectralTurbulenceGeneration(cfg.input))
		return false;
	if (!ComponentEnabled(cfg.input, comp))
		return false;

	const CohModel model = cfg.cohModel[static_cast<std::size_t>(comp)];
	if (model == CohModel::NONE)
		return false;
	if (model == CohModel::API)
		return comp == 0;

	return cfg.cohDecay[static_cast<std::size_t>(comp)] < kHugeDecay * 0.5;
}

/** @brief 检查某分量是否支持Kronecker近似。
    @param cfg SimWind配置对象。
    @param comp 分量索引。
    @return 如果满足严格相干、多维网格、允许近似且非API/GENERAL含指数模型返回true。
    @note Kronecker近似可显著降低计算成本，但会损失部分相干精度。
    @code
    if (SupportsKroneckerApproximation(cfg, 0)) { // 可近似
    @endcode */
bool SupportsKroneckerApproximation(const SimWindConfig &cfg, int comp)
{
	if (!UsesStrictCoherence(cfg, comp))
		return false;
	if (cfg.ny <= 1 || cfg.nz <= 1)
		return false;
	if (!cfg.input.allowCohApprox)
		return false;
	if (cfg.cohModel[static_cast<std::size_t>(comp)] == CohModel::API)
		return false;
	if (cfg.cohModel[static_cast<std::size_t>(comp)] == CohModel::GENERAL && cfg.input.cohExp > 0.0)
		return false;
	return true;
}

/** @brief 判断是否应该使用Kronecker近似。
    @param cfg SimWind配置对象。
    @param comp 分量索引。
    @return 如果网格点数>=256 或 Cholesky FLOPs >= 2e8 且支持近似返回true。
    @note 自动根据计算规模决定是否启用近似，平衡精度与性能。
    @code
    bool use = ShouldUseKroneckerApproximation(cfg, 0);
    @endcode */
bool ShouldUseKroneckerApproximation(const SimWindConfig &cfg, int comp)
{
	if (!SupportsKroneckerApproximation(cfg, comp))
		return false;

	const double nFreq = static_cast<double>(std::max(cfg.nFreq - 1, 0));
	const double nPoints = static_cast<double>(cfg.nPoints);
	const double denseFlops = nFreq * nPoints * nPoints * nPoints / 3.0;
	return cfg.nPoints >= 256 || denseFlops >= 2.0e8;
}

/** @brief 估算Kronecker近似的频率上限。
    @param cfg SimWind配置对象。
    @param comp 分量索引。
    @return Kronecker处理的最大频率索引（0表示不使用Kronecker）。
    @note 基于网格最小间距、相干衰减系数和代表性风速估算相干阈值频率。
    @code
    int fLimit = EstimateKroneckerFreqLimit(cfg, 0);
    @endcode */
int EstimateKroneckerFreqLimit(const SimWindConfig &cfg, int comp)
{
	if (!ShouldUseKroneckerApproximation(cfg, comp))
		return 0;

	double minDist = std::numeric_limits<double>::max();
	for (int iy = 1; iy < cfg.ny; ++iy)
	{
		const double delta = std::abs(cfg.yCoords[static_cast<std::size_t>(iy)] - cfg.yCoords[static_cast<std::size_t>(iy - 1)]);
		if (delta > 0.01)
			minDist = std::min(minDist, delta);
	}
	for (int iz = 1; iz < cfg.nz; ++iz)
	{
		const double delta = std::abs(cfg.zCoords[static_cast<std::size_t>(iz)] - cfg.zCoords[static_cast<std::size_t>(iz - 1)]);
		if (delta > 0.01)
			minDist = std::min(minDist, delta);
	}

	if (!std::isfinite(minDist) || minDist <= 0.0)
		return std::max(cfg.nFreq - 1, 0);

	const double decay = std::max(cfg.cohDecay[static_cast<std::size_t>(comp)], kTiny);
	const double representativeU = cfg.meanUByZ.empty()
	                                   ? std::max(cfg.uHub, 0.1)
	                                   : std::max(cfg.meanUByZ[static_cast<std::size_t>(cfg.nz / 2)], 0.1);
	const double cohEpsilon = 1.0e-3;
	const double fThresh = representativeU * std::log(1.0 / cohEpsilon) / (decay * std::max(minDist, 0.1));
	return std::clamp(static_cast<int>(fThresh / cfg.df), 1, std::max(cfg.nFreq - 1, 1));
}

/** @brief 在两个有序向量之间进行线性插值。
    @param x 自变量坐标向量（必须单调递增）。
    @param y 因变量值向量。
    @param xi 插值点。
    @return xi处的插值结果，越界时返回边界值，空向量返回0。
    @note 使用std::upper_bound二分查找，外推时取边界值（端点保持）。
    @code
    double val = InterpolateProfile(heights, speeds, hubHeight);
    @endcode */
double InterpolateProfile(const std::vector<double> &x, const std::vector<double> &y, double xi)
{
	if (x.empty() || y.empty())
		return 0.0;
	const std::size_t n = std::min(x.size(), y.size());
	if (n == 1 || xi <= x.front())
		return y.front();
	if (xi >= x[n - 1])
		return y[n - 1];

	const auto upper = std::upper_bound(x.begin(), x.begin() + static_cast<std::ptrdiff_t>(n), xi);
	const std::size_t i1 = static_cast<std::size_t>(std::distance(x.begin(), upper));
	const std::size_t i0 = i1 - 1;
	const double span = std::max(x[i1] - x[i0], kTiny);
	const double a = (xi - x[i0]) / span;
	return y[i0] * (1.0 - a) + y[i1] * a;
}

/** @brief InterpolateProfile 的别名，进行线性插值。
    @param x 自变量坐标向量。
    @param y 因变量值向量。
    @param xi 插值点。
    @return 插值结果。
    @code
    double val = LinearInterpolate(heights, speeds, hubHeight);
    @endcode */
double LinearInterpolate(const std::vector<double> &x, const std::vector<double> &y, double xi)
{
	return InterpolateProfile(x, y, xi);
}

/** @brief 验证 WindLInput 输入参数的有效性。
    @param input WindLInput输入参数。
    @throws std::runtime_error 当任何必填参数无效时抛出异常。
    @note 检查模式、网格、时间参数、风速、轮毂高度、分量选择、相干模型等约束。
    @code
    ValidateInput(input); // throws on invalid input
    @endcode */
void ValidateInput(const WindLInput &input)
{
	if (input.mode != Mode::GENERATE)
		throw std::runtime_error(L_WIND_OnlyGenerateMode);
	if (input.gridPtsY <= 0)
		throw std::runtime_error(L_WIND_NumPointYPositive);
	if (input.gridPtsZ <= 0)
		throw std::runtime_error(L_WIND_NumPointZPositive);
	if (input.fieldDimY <= 0.0)
		throw std::runtime_error(L_WIND_LenWidthPositive);
	if (input.fieldDimZ <= 0.0)
		throw std::runtime_error(L_WIND_LenHeightPositive);
	if (input.simTime <= 0.0 && input.analysisTime <= 0.0)
		throw std::runtime_error(L_WIND_DurOrAnaPositive);
	if (input.timeStep <= 0.0)
		throw std::runtime_error(L_WIND_TimeStepPositive);
	if (input.meanWindSpeed <= 0.0)
		throw std::runtime_error(L_WIND_SpeedPositive);
	if (input.hubHeight <= 0.0)
		throw std::runtime_error(L_WIND_HubHtPositive);
	if (!input.calWu && !input.calWv && !input.calWw)
		throw std::runtime_error(L_WIND_OneCompTrue);
	if (input.cohMod2 == CohModel::API || input.cohMod3 == CohModel::API)
		throw std::runtime_error(L_WIND_APIOnlyForU);
	if (input.turbModel == TurbModel::USER_SPECTRA && input.userTurbFile.empty())
		throw std::runtime_error(L_WIND_SpectraNeedFile);
	if (input.turbModel == TurbModel::USER_WIND_SPEED && input.userTurbFile.empty())
		throw std::runtime_error(L_WIND_WindSpeedNeedFile);
	if (input.turbModel == TurbModel::USRVKM && input.userShearFile.empty())
		throw std::runtime_error(L_WIND_USRVKMNeedShear);
}

/** @brief 根据IEC标准版本应用默认湍流参数。
    @param cfg SimWind配置对象（将被修改）。
    @note 设置 lambda, lc, cohDecay, cohB, sigma, integralScale, lateralScale, verticalScale。
    确定性事件和均匀风模型会清零湍流参数。
    @code
    ApplyIecDefaults(cfg);
    @endcode */
void ApplyIecDefaults(SimWindConfig &cfg)
{
	const auto &in = cfg.input;
	const bool ed2 = in.iecEdition == IecStandard::ED2;
	const bool deterministicEvent = IsDeterministicEventWindModel(in.windModel);
	const bool uniformWind = IsUniformWindModel(in.windModel);

	if (ed2)
	{
		cfg.lambda = cfg.hubHeight < 30.0 ? 0.7 * cfg.hubHeight : 21.0;
		cfg.lc = 3.5 * cfg.lambda;
		cfg.cohDecay = {8.8, kHugeDecay, kHugeDecay};
	}
	else
	{
		cfg.lambda = cfg.hubHeight < 60.0 ? 0.7 * cfg.hubHeight : 42.0;
		cfg.lc = 8.1 * cfg.lambda;
		cfg.cohDecay = {12.0, kHugeDecay, kHugeDecay};
	}
	cfg.cohB = {0.12 / std::max(cfg.lc, kTiny), 0.0, 0.0};

	const double tiOverride = FractionalTI(in.turbIntensity);
	if (tiOverride > 0.0)
	{
		cfg.sigma[0] = tiOverride * cfg.uHub;
	}
	else if (in.tiU > 0.0)
	{
		cfg.sigma[0] = FractionalTI(in.tiU) * cfg.uHub;
	}
	else
	{
		const double iref = TurbulenceClassIref(in.turbClass);
		if (ed2)
		{
			const double slope = in.turbClass == TurbulenceClass::Class_A ? 2.0 : 3.0;
			const double ti15 = in.turbClass == TurbulenceClass::Class_A ? 0.18 : 0.16;
			cfg.sigma[0] = ti15 * ((15.0 + slope * cfg.uHub) / (slope + 1.0));
		}
		else if (in.windModel == WindModel::ETM)
		{
			const double vRef = ClampPositive(in.vRef, DefaultVRef(in.turbineClass));
			const double vAve = 0.2 * vRef;
			const double c = ClampPositive(in.etmC, 2.0);
			cfg.sigma[0] = c * iref * (0.072 * (vAve / c + 3.0) * (cfg.uHub / c - 4.0) + 10.0);
		}
		else if (IsEwmWindModel(in.windModel))
		{
			cfg.sigma[0] = in.ewmType == EWMType::Turbulent ? 0.11 * ExtremeWindSpeedForWindModel(cfg, in.windModel) : 0.0;
		}
		else
		{
			cfg.sigma[0] = iref * (0.75 * cfg.uHub + 5.6);
		}
	}

	if (in.turbModel == TurbModel::IEC_VKAIMAL)
	{
		cfg.sigma[1] = in.tiV > 0.0 ? FractionalTI(in.tiV) * cfg.uHub : cfg.sigma[0];
		cfg.sigma[2] = in.tiW > 0.0 ? FractionalTI(in.tiW) * cfg.uHub : cfg.sigma[0];
		cfg.integralScale = {3.5 * cfg.lambda, 3.5 * cfg.lambda, 3.5 * cfg.lambda};
	}
	else if (in.turbModel == TurbModel::B_VKAL)
	{
		cfg.sigma[1] = in.tiV > 0.0 ? FractionalTI(in.tiV) * cfg.uHub : 0.8 * cfg.sigma[0];
		cfg.sigma[2] = in.tiW > 0.0 ? FractionalTI(in.tiW) * cfg.uHub : 0.5 * cfg.sigma[0];
		cfg.integralScale = {3.5 * cfg.lambda, 3.5 * cfg.lambda, 3.5 * cfg.lambda};
	}
	else if (in.turbModel == TurbModel::B_IVKAL)
	{
		cfg.sigma[1] = in.tiV > 0.0 ? FractionalTI(in.tiV) * cfg.uHub : 0.8 * cfg.sigma[0];
		cfg.sigma[2] = in.tiW > 0.0 ? FractionalTI(in.tiW) * cfg.uHub : 0.5 * cfg.sigma[0];
		cfg.integralScale = {3.5 * cfg.lambda, 3.5 * cfg.lambda, 3.5 * cfg.lambda};
	}
	else
	{
		cfg.sigma[1] = in.tiV > 0.0 ? FractionalTI(in.tiV) * cfg.uHub : 0.8 * cfg.sigma[0];
		cfg.sigma[2] = in.tiW > 0.0 ? FractionalTI(in.tiW) * cfg.uHub : 0.5 * cfg.sigma[0];
		cfg.integralScale = {8.1 * cfg.lambda, 2.7 * cfg.lambda, 0.66 * cfg.lambda};
	}

	if (in.vkLu > 0.0) cfg.integralScale[0] = in.vkLu;
	if (in.vkLv > 0.0) cfg.integralScale[1] = in.vkLv;
	if (in.vkLw > 0.0) cfg.integralScale[2] = in.vkLw;
	cfg.lateralScale = {0.3 * cfg.integralScale[0], 0.3 * cfg.integralScale[1], 0.3 * cfg.integralScale[2]};
	cfg.verticalScale = cfg.lateralScale;
	if (in.vyLu > 0.0) cfg.lateralScale[0] = in.vyLu;
	if (in.vyLv > 0.0) cfg.lateralScale[1] = in.vyLv;
	if (in.vyLw > 0.0) cfg.lateralScale[2] = in.vyLw;
	if (in.vzLu > 0.0) cfg.verticalScale[0] = in.vzLu;
	if (in.vzLv > 0.0) cfg.verticalScale[1] = in.vzLv;
	if (in.vzLw > 0.0) cfg.verticalScale[2] = in.vzLw;
	if (in.cohDecayU > 0.0) cfg.cohDecay[0] = in.cohDecayU;
	if (in.cohDecayV > 0.0) cfg.cohDecay[1] = in.cohDecayV;
	if (in.cohDecayW > 0.0) cfg.cohDecay[2] = in.cohDecayW;
	if (in.cohScaleB > 0.0) cfg.cohB = {in.cohScaleB, in.cohScaleB, in.cohScaleB};

	if (deterministicEvent || uniformWind)
	{
		cfg.sigma = {0.0, 0.0, 0.0};
		cfg.cohDecay = {kHugeDecay, kHugeDecay, kHugeDecay};
		cfg.cohB = {0.0, 0.0, 0.0};
	}
}

/** @brief 将 DEFAULT_COH 相干模型解析为实际模型。
    @param input WindLInput输入参数。
    @return 三个分量的 CohModel 数组。
    @note IEC/IEC_VKAIMAL/USRVKM: u=IEC, v=NONE, w=NONE;
    USER_SPECTRA: u=GENERAL, v=NONE, w=NONE;
    其他: 全部 GENERAL。
    @code
    auto models = ResolveCoherenceModels(input);
    @endcode */
std::array<CohModel, 3> ResolveCoherenceModels(const WindLInput &input)
{
	std::array<CohModel, 3> models{input.cohMod1, input.cohMod2, input.cohMod3};
	for (int comp = 0; comp < 3; ++comp)
	{
		auto &model = models[static_cast<std::size_t>(comp)];
		if (model != CohModel::DEFAULT_COH)
			continue;

		switch (input.turbModel)
		{
		case TurbModel::IEC_KAIMAL:
		case TurbModel::IEC_VKAIMAL:
		case TurbModel::USRVKM:
			model = comp == 0 ? CohModel::IEC : CohModel::NONE;
			break;
		case TurbModel::USER_SPECTRA:
			model = comp == 0 ? CohModel::GENERAL : CohModel::NONE;
			break;
		default:
			model = CohModel::GENERAL;
			break;
		}
	}
	return models;
}

/** @brief 如果需要，从文件加载用户风剪切剖面数据。
    @param cfg SimWind配置对象（将被修改）。
    @note 仅在 ShearType::USER, WindProfileType::USER 或 USRVKM 且文件非空时加载。
    @code
    LoadUserShearIfNeeded(cfg);
    @endcode */
void LoadUserShearIfNeeded(SimWindConfig &cfg)
{
	const auto &in = cfg.input;
	if (!(in.shearType == ShearType::USER ||
	      in.windProfileType == WindProfileType::USER ||
	      in.turbModel == TurbModel::USRVKM))
		return;
	if (in.userShearFile.empty())
		return;
	cfg.userShear = ReadUserShear(in.userShearFile);
	cfg.hasUserShear = !cfg.userShear.heights.empty();
}

/** @brief 根据模型配置追加消耗/限制警告信息。
    @param cfg SimWind配置对象（将被修改）。
    @note 对 USER_WIND_SPEED 有气象输入时、非随机模型有雷诺应力目标时发出警告。
    @code
    AppendModelConsumptionNotes(cfg);
    @endcode */
void AppendModelConsumptionNotes(SimWindConfig &cfg)
{
	if (cfg.input.turbModel == TurbModel::USER_WIND_SPEED && HasAnyMeteorologyHints(cfg.input))
	{
		AppendWarning(cfg.warnings, L_WARN_UserWindMeteoSkip);
	}

	if (!IsSyntheticStochasticModel(cfg.input) &&
	    (HasExplicitReynoldsTarget(cfg.input.reynoldsUW) ||
	     HasExplicitReynoldsTarget(cfg.input.reynoldsUV) ||
	     HasExplicitReynoldsTarget(cfg.input.reynoldsVW)))
	{
		AppendWarning(cfg.warnings, L_WARN_ReynoldsStressSkipNonSynthetic);
	}
}

/** @brief 在轮毂高度处插值剖面值。
    @param cfg SimWind配置对象。
    @param valuesByZ 按高度分布的剖面值。
    @return 轮毂高度处的插值结果，空剖面返回0。
    @note 内部调用 LinearInterpolate。
    @code
    double sigmaHub = ProfileValueAtHub(cfg, sigmaByZ[0]);
    @endcode */
double ProfileValueAtHub(const SimWindConfig &cfg, const std::vector<double> &valuesByZ)
{
	if (cfg.zCoords.empty() || valuesByZ.empty())
		return 0.0;
	return LinearInterpolate(cfg.zCoords, valuesByZ, cfg.hubHeight);
}

/** @brief 获取指定高度处的局部湍流标准差。
    @param cfg SimWind配置对象。
    @param comp 分量索引 (0=u, 1=v, 2=w)。
    @param iz 高度索引。
    @return 局部sigma值，若无剖面则返回全局sigma。
    @code
    double sigma = LocalSigmaAtZ(cfg, 0, iz);
    @endcode */
double LocalSigmaAtZ(const SimWindConfig &cfg, int comp, int iz)
{
	if (cfg.hasSigmaProfileByZ &&
	    iz >= 0 &&
	    static_cast<std::size_t>(iz) < cfg.sigmaByZ[static_cast<std::size_t>(comp)].size())
		return std::max(cfg.sigmaByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)], 0.0);
	return cfg.sigma[static_cast<std::size_t>(comp)];
}

/** @brief 获取指定高度处的局部湍流长度尺度。
    @param cfg SimWind配置对象。
    @param comp 分量索引。
    @param iz 高度索引。
    @return 局部长度尺度值，若无剖面则返回全局integralScale。
    @code
    double L = LocalLengthScaleAtZ(cfg, 0, iz);
    @endcode */
double LocalLengthScaleAtZ(const SimWindConfig &cfg, int comp, int iz)
{
	if (cfg.hasLengthScaleProfileByZ &&
	    iz >= 0 &&
	    static_cast<std::size_t>(iz) < cfg.lengthScaleByZ[static_cast<std::size_t>(comp)].size())
		return std::max(cfg.lengthScaleByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)], kTiny);
	return std::max(cfg.integralScale[static_cast<std::size_t>(comp)], kTiny);
}

/** @brief 使用剖面数据的轮毂高度值覆盖全局湍流参数。
    @param cfg SimWind配置对象（将被修改）。
    @note 当存在 sigmaByZ 或 lengthScaleByZ 剖面时，将 hub 高度处的插值赋给全局面 sigma/integralScale。
    @code
    ApplyUserProfileTurbulenceOverrides(cfg);
    @endcode */
void ApplyUserProfileTurbulenceOverrides(SimWindConfig &cfg)
{
	if (cfg.hasSigmaProfileByZ)
	{
		for (int comp = 0; comp < 3; ++comp)
			cfg.sigma[static_cast<std::size_t>(comp)] = ProfileValueAtHub(cfg, cfg.sigmaByZ[static_cast<std::size_t>(comp)]);
	}

	if (cfg.hasLengthScaleProfileByZ)
	{
		for (int comp = 0; comp < 3; ++comp)
			cfg.integralScale[static_cast<std::size_t>(comp)] = ProfileValueAtHub(cfg, cfg.lengthScaleByZ[static_cast<std::size_t>(comp)]);
	}
}

/** @brief 验证并排序用户自定义频谱数据。
    @param data UserSpectraData 对象（将被修改）。
    @throws std::runtime_error 数据不足或无效时抛出。
    @note 要求至少3个频点，所有PSD值严格为正，频率唯一且升序。
    @code
    NormalizeAndValidateUserSpectra(userSpectra);
    @endcode */
void NormalizeAndValidateUserSpectra(UserSpectraData &data)
{
	const std::size_t n = data.frequencies.size();
	if (n < 3 || data.uPsd.size() != n || data.vPsd.size() != n || data.wPsd.size() != n)
		throw std::runtime_error(L_WIND_Spectra3Rows);
	if (data.specScale1 <= 0.0 || data.specScale2 <= 0.0 || data.specScale3 <= 0.0)
		throw std::runtime_error(L_WIND_SpectraScalePos);

	struct UserSpectraRow
	{
		double f = 0.0;
		double u = 0.0;
		double v = 0.0;
		double w = 0.0;
	};

	std::vector<UserSpectraRow> rows(n);
	for (std::size_t i = 0; i < n; ++i)
	{
		if (data.uPsd[i] <= 0.0 || data.vPsd[i] <= 0.0 || data.wPsd[i] <= 0.0)
			throw std::runtime_error(L_WIND_SpectraPosValues);
		rows[i] = {data.frequencies[i], data.uPsd[i], data.vPsd[i], data.wPsd[i]};
	}

	std::sort(rows.begin(), rows.end(), [](const UserSpectraRow &a, const UserSpectraRow &b) { return a.f < b.f; });
	for (std::size_t i = 1; i < rows.size(); ++i)
	{
		if (rows[i].f <= rows[i - 1].f + kTiny)
			throw std::runtime_error(L_WIND_SpectraUniqueFreq);
	}

	for (std::size_t i = 0; i < rows.size(); ++i)
	{
		data.frequencies[i] = rows[i].f;
		data.uPsd[i] = rows[i].u;
		data.vPsd[i] = rows[i].v;
		data.wPsd[i] = rows[i].w;
	}
	data.numFrequencies = static_cast<int>(rows.size());
}

/** @brief 解析所有气象参数，完成气象闭包。
    @param cfg SimWind配置对象（将被修改）。
    @note 计算理查森数、z/L、Monin-Obukhov长度、科氏力、摩擦速度、混合层深度、相干参数和雷诺应力。
    不稳定条件下若无有效混合层深度则抛出异常。
    @code
    ResolveMeteorologyClosure(cfg);
    @endcode */
void ResolveMeteorologyClosure(SimWindConfig &cfg)
{
	cfg.met.richardson = cfg.input.richardson;
	cfg.met.zL = std::abs(cfg.input.zOverL) > kTiny
	                 ? cfg.input.zOverL
	                 : DeriveZLFromRichardson(cfg.input.richardson);
	cfg.met.moninLength = MoninLengthFromZL(cfg.hubHeight, cfg.met.zL);
	cfg.met.coriolis = 2.0 * kOmega * std::sin(std::abs(cfg.input.latitude) * kRad);

	const double zLAtRef = ZOverLAtHeight(cfg.refHeight, cfg.met.moninLength);
	cfg.met.uStarDiab = UstarDiabatic(cfg.input.meanWindSpeed,
	                                  cfg.refHeight,
	                                  cfg.input.roughness,
	                                  zLAtRef);
	cfg.met.uStar = cfg.input.uStar > 0.0 ? cfg.input.uStar : cfg.met.uStarDiab;

	if (cfg.input.mixingLayerDepth > 0.0)
		cfg.met.mixingLayerDepth = cfg.input.mixingLayerDepth;
	else
		cfg.met.mixingLayerDepth = DefaultMixingLayerDepth(cfg.met.uStar,
		                                                   cfg.met.uStarDiab,
		                                                   cfg.input.meanWindSpeed,
		                                                   cfg.refHeight,
		                                                   cfg.input.roughness,
		                                                   cfg.met.coriolis);

	if (IsSyntheticStochasticModel(cfg.input) && cfg.met.zL < 0.0 &&
	    (!std::isfinite(cfg.met.mixingLayerDepth) || cfg.met.mixingLayerDepth <= 0.0))
	{
		throw std::runtime_error(L_WIND_MixingDepthNeed);
	}

	const double stabilityFactor = std::clamp(1.0 + 0.35 * cfg.met.zL, 0.65, 1.50);
	const double baseDecay = std::max(cfg.uHub, 0.1) * stabilityFactor;
	cfg.met.defaultGeneralCohDecay = {baseDecay, 0.75 * baseDecay, 0.75 * baseDecay};

	const double mixingScale = std::max({cfg.lc,
	                                     cfg.hubHeight,
	                                     cfg.met.mixingLayerDepth > 0.0 ? 0.25 * cfg.met.mixingLayerDepth : 0.0,
	                                     1.0});
	const double baseB = 0.12 / mixingScale;
	cfg.met.defaultGeneralCohB = {baseB, baseB, baseB};

	const bool explicitUW = HasExplicitReynoldsTarget(cfg.input.reynoldsUW);
	const bool explicitUV = HasExplicitReynoldsTarget(cfg.input.reynoldsUV);
	const bool explicitVW = HasExplicitReynoldsTarget(cfg.input.reynoldsVW);

	cfg.met.reynoldsStress.skip = {true, true, true};
	cfg.met.reynoldsStress.target = {0.0, 0.0, 0.0};
	if (IsSyntheticStochasticModel(cfg.input))
	{
		cfg.met.reynoldsStress.target[0] = explicitUW ? cfg.input.reynoldsUW : -(cfg.met.uStar * cfg.met.uStar);
		cfg.met.reynoldsStress.skip[0] = !(explicitUW || cfg.met.uStar > kTiny);
		cfg.met.reynoldsStress.target[1] = explicitUV ? cfg.input.reynoldsUV : 0.0;
		cfg.met.reynoldsStress.target[2] = explicitVW ? cfg.input.reynoldsVW : 0.0;
		cfg.met.reynoldsStress.skip[1] = !explicitUV;
		cfg.met.reynoldsStress.skip[2] = !explicitVW;
	}
	cfg.met.reynoldsStress.active = !cfg.met.reynoldsStress.skip[0] ||
	                                !cfg.met.reynoldsStress.skip[1] ||
	                                !cfg.met.reynoldsStress.skip[2];
}

/** @brief 对未显式设置的相干参数应用气象默认值。
    @param cfg SimWind配置对象（将被修改）。
    @note GENERAL模型使用气象推导的衰减系数和偏移参数；NONE模型清零相干参数。
    @code
    ApplyMeteorologyCoherenceDefaults(cfg);
    @endcode */
void ApplyMeteorologyCoherenceDefaults(SimWindConfig &cfg)
{
	for (int comp = 0; comp < 3; ++comp)
	{
		const std::size_t cs = static_cast<std::size_t>(comp);
		const CohModel model = cfg.cohModel[cs];
		if (model == CohModel::NONE)
		{
			cfg.cohDecay[cs] = kHugeDecay;
			cfg.cohB[cs] = 0.0;
			continue;
		}
		if (model == CohModel::GENERAL)
		{
			if (!cfg.hasExplicitCohDecay[cs])
				cfg.cohDecay[cs] = cfg.met.defaultGeneralCohDecay[cs];
			if (!cfg.hasExplicitCohB)
				cfg.cohB[cs] = cfg.met.defaultGeneralCohB[cs];
		}
	}
}

/** @brief 在数据加载后验证用户输入的完整性。
    @param cfg SimWind配置对象。
    @throws std::runtime_error 当USRVKM缺少必要的剖面列时抛出。
    @note 主要验证USRVKM模式的用户剪切文件是否包含所需所有列。
    @code
    ValidateResolvedUserInputs(cfg);
    @endcode */
void ValidateResolvedUserInputs(const SimWindConfig &cfg)
{
	if (cfg.input.turbModel == TurbModel::USRVKM)
	{
		if (!cfg.hasUserShear)
			throw std::runtime_error(L_WIND_USRVKMNoData);
		if (!HasProfileColumn(cfg.userShear.heights, cfg.userShear.windSpeeds) ||
		    !HasProfileColumn(cfg.userShear.heights, cfg.userShear.windDirections) ||
		    !HasProfileColumn(cfg.userShear.heights, cfg.userShear.standardDeviations) ||
		    !HasProfileColumn(cfg.userShear.heights, cfg.userShear.lengthScales))
		{
			throw std::runtime_error(L_WIND_UserShearNoProfile);
		}
	}
}

/** @brief 构建空间网格坐标和风场剖面。
    @param cfg SimWind配置对象（将被修改）。
    @note 计算y/z坐标、平均风速剖面、风向剖面、湍流剖面；
    支持用户剪切文件、对数律、IEC幂律、改进von Karman等剖面类型。
    @code
    BuildGridAndProfiles(cfg);
    @endcode */
void BuildGridAndProfiles(SimWindConfig &cfg)
{
	const auto &in = cfg.input;
	cfg.yCoords.resize(static_cast<std::size_t>(cfg.ny));
	cfg.zCoords.resize(static_cast<std::size_t>(cfg.nz));
	cfg.meanUByZ.resize(static_cast<std::size_t>(cfg.nz));
	cfg.directionByZ.assign(static_cast<std::size_t>(cfg.nz), 0.0);
	for (auto &values : cfg.sigmaByZ)
		values.assign(static_cast<std::size_t>(cfg.nz), 0.0);
	for (auto &values : cfg.lengthScaleByZ)
		values.assign(static_cast<std::size_t>(cfg.nz), 0.0);
	for (auto &values : cfg.lateralScaleByZ)
		values.assign(static_cast<std::size_t>(cfg.nz), 0.0);
	for (auto &values : cfg.verticalScaleByZ)
		values.assign(static_cast<std::size_t>(cfg.nz), 0.0);
	cfg.y.resize(static_cast<std::size_t>(cfg.nPoints));
	cfg.z.resize(static_cast<std::size_t>(cfg.nPoints));
	cfg.meanU.resize(static_cast<std::size_t>(cfg.nPoints));

	for (int iy = 0; iy < cfg.ny; ++iy)
		cfg.yCoords[static_cast<std::size_t>(iy)] = -0.5 * cfg.gridWidth + cfg.dy * iy;

	for (int iz = 0; iz < cfg.nz; ++iz)
	{
		const double z = cfg.zBottom + cfg.dz * iz;
		cfg.zCoords[static_cast<std::size_t>(iz)] = z;
		for (int iy = 0; iy < cfg.ny; ++iy)
		{
			const int p = GridIndex(cfg, iz, iy);
			cfg.y[static_cast<std::size_t>(p)] = cfg.yCoords[static_cast<std::size_t>(iy)];
			cfg.z[static_cast<std::size_t>(p)] = z;
		}
	}

	LoadUserShearIfNeeded(cfg);
	cfg.hasUserDirectionProfile = cfg.hasUserShear && HasProfileColumn(cfg.userShear.heights, cfg.userShear.windDirections);
	cfg.hasUserSigmaProfile = cfg.hasUserShear && HasProfileColumn(cfg.userShear.heights, cfg.userShear.standardDeviations);
	cfg.hasUserLengthScaleProfile = cfg.hasUserShear && HasProfileColumn(cfg.userShear.heights, cfg.userShear.lengthScales);
	cfg.hasSigmaProfileByZ = cfg.hasUserSigmaProfile;
	cfg.hasLengthScaleProfileByZ = cfg.hasUserLengthScaleProfile;
	cfg.hasImprovedVkProfile = false;

	if (cfg.hasUserShear)
	{
		if (!cfg.userShear.windDirections.empty() && !cfg.hasUserDirectionProfile)
			AppendWarning(cfg.warnings, L_WARN_UserShearDirMismatch);
		if (!cfg.userShear.standardDeviations.empty() && !cfg.hasUserSigmaProfile)
			AppendWarning(cfg.warnings, L_WARN_UserShearSigmaMismatch);
		if (!cfg.userShear.lengthScales.empty() && !cfg.hasUserLengthScaleProfile)
			AppendWarning(cfg.warnings, L_WARN_UserShearLengthMismatch);
	}

	const double luBase = std::max(cfg.integralScale[0], kTiny);
	const std::array<double, 3> lengthRatios{
	    1.0,
	    std::max(cfg.integralScale[1], kTiny) / luBase,
	    std::max(cfg.integralScale[2], kTiny) / luBase};

	for (int iz = 0; iz < cfg.nz; ++iz)
	{
		const double z = std::max(cfg.zCoords[static_cast<std::size_t>(iz)], 0.05);
		double u = cfg.uHub;

		if (cfg.hasUserShear)
		{
			u = LinearInterpolate(cfg.userShear.heights, cfg.userShear.windSpeeds, z);
		}
		else if (IsUniformWindModel(in.windModel))
		{
			u = cfg.uHub;
		}
		else if (IsSteadyEwmWindModel(in))
		{
			u = cfg.uHub * std::pow(z / std::max(cfg.hubHeight, kTiny), 0.11);
		}
		else if (in.shearType == ShearType::LOG || in.windProfileType == WindProfileType::LOG)
		{
			u = DiabaticLogWindSpeed(z,
			                         cfg.refHeight,
			                         cfg.input.meanWindSpeed,
			                         in.roughness,
			                         cfg.met.moninLength);
		}
		else if (in.windProfileType == WindProfileType::IEC)
		{
			u = cfg.uHub * std::pow(z / std::max(cfg.refHeight, kTiny), DefaultIecProfileExponent(cfg));
		}
		else
		{
			u = cfg.uHub * std::pow(z / std::max(cfg.refHeight, kTiny), in.shearExp);
		}

		const double uClamped = std::max(0.0, u);
		cfg.meanUByZ[static_cast<std::size_t>(iz)] = uClamped;
		if (cfg.hasUserDirectionProfile)
			cfg.directionByZ[static_cast<std::size_t>(iz)] =
			    InterpolateWrappedDirection(cfg.userShear.heights, cfg.userShear.windDirections, z);
		if (cfg.hasUserSigmaProfile)
		{
			const double sigmaBase = std::max(0.0, InterpolateProfile(cfg.userShear.heights, cfg.userShear.standardDeviations, z));
			for (int comp = 0; comp < 3; ++comp)
				cfg.sigmaByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] = sigmaBase * UserStdScale(cfg.userShear, comp);
		}
		if (cfg.hasUserLengthScaleProfile)
		{
			const double lengthBase = std::max(kTiny, InterpolateProfile(cfg.userShear.heights, cfg.userShear.lengthScales, z));
			for (int comp = 0; comp < 3; ++comp)
				cfg.lengthScaleByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] =
				    std::max(lengthBase * lengthRatios[static_cast<std::size_t>(comp)], kTiny);
		}
		if (IsImprovedVonKarman(in.turbModel))
		{
			const ImprovedVkProfilePoint ivk = ComputeImprovedVkPoint(cfg, z, uClamped > 0.1 ? uClamped : std::max(cfg.uHub, 0.1));
			if (ivk.valid)
			{
				cfg.hasImprovedVkProfile = true;
				cfg.hasSigmaProfileByZ = true;
				cfg.hasLengthScaleProfileByZ = true;
				for (int comp = 0; comp < 3; ++comp)
				{
					cfg.sigmaByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] = ivk.sigma[comp];
					cfg.lengthScaleByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] = ivk.xScale[comp];
					cfg.lateralScaleByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] = ivk.yScale[comp];
					cfg.verticalScaleByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] = ivk.zScale[comp];
				}
			}
		}
		for (int iy = 0; iy < cfg.ny; ++iy)
		{
			const int p = GridIndex(cfg, iz, iy);
			cfg.meanU[static_cast<std::size_t>(p)] = uClamped;
		}
	}

	if (IsImprovedVonKarman(in.turbModel) && !cfg.hasImprovedVkProfile)
	{
		AppendWarning(cfg.warnings, L_WARN_ImprovedVkFallback);
	}

	if (cfg.hasImprovedVkProfile)
	{
		for (int comp = 0; comp < 3; ++comp)
		{
			cfg.lateralScale[static_cast<std::size_t>(comp)] =
			    ProfileValueAtHub(cfg, cfg.lateralScaleByZ[static_cast<std::size_t>(comp)]);
			cfg.verticalScale[static_cast<std::size_t>(comp)] =
			    ProfileValueAtHub(cfg, cfg.verticalScaleByZ[static_cast<std::size_t>(comp)]);
		}
	}
}

/** @brief 估算生成过程的内存消耗和计算量(FLOPs)。
    @param cfg SimWind配置对象（将被修改）。
    @note 计算严格相干分量的Cholesky FLOPs和估计峰值内存(GiB)；
    Mann模型使用FFT估计，其他使用矩阵分解估计。
    @code
    EstimateGenerationCost(cfg);
    @endcode */
void EstimateGenerationCost(SimWindConfig &cfg)
{
	cfg.strictCoherenceComponents = 0;
	cfg.estimatedCholeskyFlops = 0.0;
	if (IsMann(cfg.input.turbModel))
	{
		const double nMann = static_cast<double>(std::max(cfg.mannFftPoints, 1)) *
		                     static_cast<double>(std::max(cfg.mannGridY, 1)) *
		                     static_cast<double>(std::max(cfg.mannGridZ, 1));
		const double mannBytes = 3.0 * nMann * sizeof(fftw_complex);
		const double fieldBytes = 3.0 * static_cast<double>(cfg.nSteps) *
		                          static_cast<double>(cfg.nPoints) * sizeof(double);
		cfg.estimatedPeakMemoryGiB = (mannBytes + fieldBytes) / (1024.0 * 1024.0 * 1024.0);
		return;
	}

	double strictMatrixBytes = 0.0;
	for (int comp = 0; comp < 3; ++comp)
	{
		if (UsesStrictCoherence(cfg, comp))
		{
			++cfg.strictCoherenceComponents;
			if (cfg.useKronecker[static_cast<std::size_t>(comp)])
			{
				const double lowFreq = static_cast<double>(std::max(cfg.kroneckerFreqLimit[static_cast<std::size_t>(comp)], 0));
				cfg.estimatedCholeskyFlops += lowFreq *
				                              (std::pow(static_cast<double>(cfg.ny), 3.0) +
				                               std::pow(static_cast<double>(cfg.nz), 3.0)) /
				                              3.0;
				strictMatrixBytes += 3.0 * static_cast<double>((cfg.ny * cfg.ny) + (cfg.nz * cfg.nz)) * sizeof(double);
			}
			else
			{
				const double nPoints = static_cast<double>(cfg.nPoints);
				const double nFreq = static_cast<double>(std::max(cfg.nFreq - 1, 0));
				cfg.estimatedCholeskyFlops += nFreq * nPoints * nPoints * nPoints / 3.0;
				strictMatrixBytes += 3.0 * nPoints * nPoints * sizeof(double);
			}
		}
	}

	const double nPoints = static_cast<double>(cfg.nPoints);
	const double spectrumBytes = static_cast<double>(cfg.nSteps) * nPoints * sizeof(fftw_complex);
	const double fieldBytes = 3.0 * static_cast<double>(cfg.nSteps) * nPoints * sizeof(double);
	cfg.estimatedPeakMemoryGiB = (strictMatrixBytes + spectrumBytes + fieldBytes) / (1024.0 * 1024.0 * 1024.0);
}

/** @brief 从 WindLInput 构建完整的 SimWindConfig。
    @param input WindLInput输入参数。
    @return 完整的SimWindConfig配置对象。
    @note 依次调用：验证、设置网格/时间参数、解析相干模型、应用IEC默认、加载频谱、
    气象闭包、构建坐标剖面、验证、覆盖参数、消耗说明、Kronecker决策、成本估算。
    @code
    SimWindConfig cfg = BuildConfig(input);
    @endcode */
SimWindConfig BuildConfig(const WindLInput &input)
{
	ValidateInput(input);

	SimWindConfig cfg;
	cfg.input = input;
	cfg.ny = input.gridPtsY;
	cfg.nz = input.gridPtsZ;
	cfg.nPoints = cfg.ny * cfg.nz;
	cfg.dt = input.timeStep;
	cfg.duration = input.analysisTime > 0.0 ? input.analysisTime : input.simTime;
	const int requestedSteps = std::max(2, static_cast<int>(std::ceil(cfg.duration / cfg.dt)));
	cfg.nSteps = EvenStepCount(cfg.duration, cfg.dt);
	cfg.duration = cfg.nSteps * cfg.dt;
	cfg.nFreq = cfg.nSteps / 2;
	cfg.df = 1.0 / cfg.duration;
	cfg.gridWidth = input.fieldDimY;
	cfg.gridHeight = input.fieldDimZ;
	cfg.dy = cfg.ny > 1 ? cfg.gridWidth / (cfg.ny - 1) : 0.0;
	cfg.dz = cfg.nz > 1 ? cfg.gridHeight / (cfg.nz - 1) : 0.0;
	cfg.hubHeight = input.hubHeight;
	cfg.uHub = input.meanWindSpeed;
	if (IsSteadyEwmWindModel(input))
		cfg.uHub = SteadyEwmHubSpeed(cfg);
	cfg.refHeight = input.refHeight > 0.0 ? input.refHeight : input.hubHeight;
	cfg.effectiveRotorDiameter = input.rotorDiameter > 0.0 ? input.rotorDiameter : input.fieldDimZ;
	cfg.zBottom = input.hubHeight + 0.5 * cfg.effectiveRotorDiameter - input.fieldDimZ;
	cfg.dx = cfg.uHub * cfg.dt;
	cfg.scaleIEC = input.scaleIEC >= 0 ? input.scaleIEC : (input.useIECSimmga ? 2 : 0);
	cfg.cohModel = ResolveCoherenceModels(input);
	cfg.hasExplicitCohDecay = {input.cohDecayU > 0.0, input.cohDecayV > 0.0, input.cohDecayW > 0.0};
	cfg.hasExplicitCohB = input.cohScaleB > 0.0;

	if (cfg.zBottom <= 0.0)
		cfg.warnings.push_back(L_WARN_GridBottomClamped);

	ApplyIecDefaults(cfg);

	cfg.mannLength = input.mannLength > 0.0 ? input.mannLength : 0.8 * std::max(cfg.lambda, kTiny);
	cfg.mannGamma = input.mannGamma > 0.0 ? input.mannGamma : 3.9;
	cfg.mannMaxL = input.mannMaxL > 0.0 ? input.mannMaxL : 8.0 * cfg.mannLength;
	cfg.mannFftPoints = input.mannNx > 0 ? input.mannNx : cfg.nSteps;
	cfg.mannGridY = input.mannNy > 0 ? std::max(input.mannNy, cfg.ny) : cfg.ny;
	cfg.mannGridZ = input.mannNz > 0 ? std::max(input.mannNz, cfg.nz) : cfg.nz;
	if (IsMann(input.turbModel))
		cfg.integralScale = {cfg.mannLength, cfg.mannLength, cfg.mannLength};
	if (input.turbModel == TurbModel::USER_SPECTRA)
	{
		cfg.userSpectra = ReadUserSpectra(input.userTurbFile);
		cfg.hasUserSpectra = !cfg.userSpectra.frequencies.empty();
		NormalizeAndValidateUserSpectra(cfg.userSpectra);
	}

	ResolveMeteorologyClosure(cfg);
	ApplyMeteorologyCoherenceDefaults(cfg);
	BuildGridAndProfiles(cfg);
	ValidateResolvedUserInputs(cfg);
	ApplyUserProfileTurbulenceOverrides(cfg);
	AppendModelConsumptionNotes(cfg);
	for (int comp = 0; comp < 3; ++comp)
	{
		if (IsMann(input.turbModel))
		{
			cfg.useKronecker[static_cast<std::size_t>(comp)] = false;
			cfg.kroneckerFreqLimit[static_cast<std::size_t>(comp)] = 0;
		}
		else
		{
			cfg.useKronecker[static_cast<std::size_t>(comp)] = ShouldUseKroneckerApproximation(cfg, comp);
			cfg.kroneckerFreqLimit[static_cast<std::size_t>(comp)] = EstimateKroneckerFreqLimit(cfg, comp);
		}
	}
	EstimateGenerationCost(cfg);
	if (IsMann(input.turbModel) && cfg.mannFftPoints < cfg.nSteps)
		cfg.warnings.push_back(BuildMannRepeatWarning(cfg));
	if (IsMann(input.turbModel) && cfg.scaleIEC < 1)
		cfg.warnings.push_back(L_WARN_MannScaleIEC0);
	for (int comp = 0; comp < 3; ++comp)
	{
		if (cfg.useKronecker[static_cast<std::size_t>(comp)])
		{
			const char axis = comp == 0 ? 'u' : (comp == 1 ? 'v' : 'w');
			std::ostringstream note;
			note << L_WARN_KroneckerCompPrefix << axis
			     << L_WARN_KroneckerCompUses
			     << cfg.kroneckerFreqLimit[static_cast<std::size_t>(comp)]
			     << L_WARN_KroneckerCompEnd;
			cfg.warnings.push_back(note.str());
		}
	}
	if (cfg.estimatedCholeskyFlops > 5.0e13 || cfg.estimatedPeakMemoryGiB > 12.0)
	{
		std::ostringstream warning;
		warning << "Large strict-coherence estimate: peak memory "
		        << FormatGiB(cfg.estimatedPeakMemoryGiB)
		        << ", Cholesky FLOPs "
		        << FormatScientific(cfg.estimatedCholeskyFlops)
		        << ".";
		cfg.warnings.push_back(warning.str());
		if (!cfg.input.allowCohApprox)
			AppendWarning(cfg.warnings, L_WARN_LargeStrictCoh);
	}

	const std::filesystem::path outputDir = input.savePath.empty() ? std::filesystem::current_path() : std::filesystem::path(input.savePath);
	std::filesystem::create_directories(outputDir);
	const std::string baseName = input.saveName.empty() ? "SimWind" : input.saveName;
	cfg.outputBase = outputDir / baseName;

	return cfg;
}

/** @brief 获取用户频谱各分量的缩放因子。
    @param data UserSpectraData数据。
    @param comp 分量索引 (0=specScale1, 1=specScale2, 其他=specScale3)。
    @return 对应分量的缩放因子。
    @code
    double scale = UserSpectraScale(data, 0); // specScale1
    @endcode */
double UserSpectraScale(const UserSpectraData &data, int comp)
{
	if (comp == 0)
		return data.specScale1;
	if (comp == 1)
		return data.specScale2;
	return data.specScale3;
}

/**
 * @brief 使用端点保持插值用户自定义PSD谱值，超出范围时锁定边界值
 * @param freqs   用户提供的频率点数组（单调递增）
 * @param values  用户提供的PSD值数组，与freqs一一对应
 * @param freq    待插值的查询频率 [Hz]
 * @return        插值后的PSD值，始终非负；若输入为空则返回0.0
 * @note         f <= freqs.front()时取values.front()，f >= freqs.back()时取values.back()；
 *               中间区域使用InterpolateProfile线性插值，结果强制截断到非负。
 * @code
 *   std::vector<double> f{0.1, 1.0, 5.0};
 *   std::vector<double> val{10.0, 2.0, 3.0};
 *   double psd = InterpolateUserPsdWithEndpointHold(f, val, 2.5); // 插值至约2.5
 * @endcode
 */
double InterpolateUserPsdWithEndpointHold(const std::vector<double> &freqs,
                                          const std::vector<double> &values,
                                          double freq)
{
	if (freqs.empty() || values.empty())
		return 0.0;
	const std::size_t n = std::min(freqs.size(), values.size());
	if (n == 1)
		return std::max(values.front(), 0.0);
	if (freq <= freqs.front())
		return std::max(values.front(), 0.0);
	if (freq >= freqs[n - 1])
		return std::max(values[n - 1], 0.0);
	return std::max(InterpolateProfile(freqs, values, freq), 0.0);
}

/**
 * @brief 根据湍流模型计算给定频率和参数下的单点风速谱值 S(f)
 * @param cfg           仿真配置，包含湍流模型、用户谱数据等
 * @param comp          速度分量索引：0=u（纵向）, 1=v（横向）, 2=w（竖向）
 * @param freq          目标频率 [Hz]，频率≤0时直接返回0
 * @param uMean         该点的平均风速 [m/s]，被钳位至≥0.1
 * @param sigma         该分量的标准差 [m/s]
 * @param integralScale 该分量的积分尺度 [m]
 * @param height        该点的高度 [m]，用于改进VonKarman模型的高度相关参数计算
 * @return              谱值 S(f) [m²/s]，若分量未启用或freq≤0则返回0
 * @note                支持四种路径：(1)USER_SPECTRA直接插值用户PSD；
 *                      (2)Improved Von Karman使用双项混合公式与高度相关的尺度修正；
 *                      (3)Von Karman/Mann使用经典VK公式S∝2σ²L/(1+71f²)⁵⁄⁶；
 *                      (4)Kaimal退化为S∝4σ²L/(1+6f)⁵⁄³。
 * @code
 *   double su = SpectrumWithParameters(cfg, 0, 0.5, cfg.uHub,
 *                                       cfg.sigma[0], cfg.integralScale[0], cfg.hubHeight);
 * @endcode
 */
double SpectrumWithParameters(const SimWindConfig &cfg,
                              int comp,
                              double freq,
                              double uMean,
                              double sigma,
                              double integralScale,
                              double height)
{
	if (freq <= 0.0 || !ComponentEnabled(cfg.input, comp))
		return 0.0;

	const double meanU = std::max(uMean, 0.1);

	if (cfg.input.turbModel == TurbModel::USER_SPECTRA && cfg.hasUserSpectra)
	{
		const auto &data = cfg.userSpectra;
		const std::vector<double> *psd = &data.uPsd;
		if (comp == 1)
			psd = &data.vPsd;
		else if (comp == 2)
			psd = &data.wPsd;
		return UserSpectraScale(data, comp) * InterpolateUserPsdWithEndpointHold(data.frequencies, *psd, freq);
	}

	if (IsImprovedVonKarman(cfg.input.turbModel))
	{
		const ImprovedVkProfilePoint ivk = ComputeImprovedVkPoint(cfg, height, meanU);
		if (!ivk.valid)
		{
			const double lOverU = std::max(integralScale, kTiny) / meanU;
			const double flu2 = std::pow(freq * lOverU, 2.0);
			const double tmp = 1.0 + 71.0 * flu2;
			const double sigmaL = 2.0 * sigma * sigma * lOverU;
			if (comp == 0)
				return 2.0 * sigmaL / std::pow(tmp, 5.0 / 6.0);
			return sigmaL * (1.0 + 189.0 * flu2) / std::pow(tmp, 11.0 / 6.0);
		}

		const double xScale = std::max(ivk.xScale[comp], kTiny);
		const double nTilde = std::max(freq * xScale / meanU, 1.0e-9);
		const double a = std::max(ivk.a, 1.0e-6);
		const double nOverA = nTilde / a;
		const double beta1 = std::clamp(2.357 * a - 0.761, 0.0, 1.0);
		const double beta2 = std::clamp(1.0 - beta1, 0.0, 1.0);

		if (comp == 0)
		{
			const double f1 = std::pow(1.0 + 0.455 * std::exp(-0.76 * nOverA), -0.8);
			const double term1 = beta1 * 2.987 * nOverA /
			                     std::pow(1.0 + std::pow(2.0 * kPi * nOverA, 2.0), 5.0 / 6.0);
			const double term2 = beta2 * 1.294 * nOverA /
			                     std::max(f1 * std::pow(1.0 + std::pow(kPi * nOverA, 2.0), 5.0 / 6.0), 1.0e-12);
			return sigma * sigma * (term1 + term2) / freq;
		}

		const double f2 = std::pow(1.0 + 2.88 * std::exp(-0.218 * nOverA), -0.9);
		const double term1 = beta1 * 2.987 * (1.0 + (8.0 / 3.0) * std::pow(4.0 * kPi * nOverA, 2.0)) * nOverA /
		                     std::pow(1.0 + std::pow(4.0 * kPi * nOverA, 2.0), 11.0 / 6.0);
		const double term2 = beta2 * 1.294 * nOverA /
		                     std::max(f2 * std::pow(1.0 + std::pow(2.0 * kPi * nOverA, 2.0), 5.0 / 6.0), 1.0e-12);
		return sigma * sigma * (term1 + term2) / freq;
	}

	if ((IsVonKarman(cfg.input.turbModel) && !IsImprovedVonKarman(cfg.input.turbModel)) || IsMann(cfg.input.turbModel))
	{
		const double lOverU = std::max(integralScale, kTiny) / meanU;
		const double flu2 = std::pow(freq * lOverU, 2.0);
		const double tmp = 1.0 + 71.0 * flu2;
		const double sigmaL = 2.0 * sigma * sigma * lOverU;
		if (comp == 0)
			return 2.0 * sigmaL / std::pow(tmp, 5.0 / 6.0);
		return sigmaL * (1.0 + 189.0 * flu2) / std::pow(tmp, 11.0 / 6.0);
	}

	const double lOverU = std::max(integralScale, kTiny) / meanU;
	const double sigmaLU = 4.0 * sigma * sigma * lOverU;
	return sigmaLU / std::pow(1.0 + 6.0 * lOverU * freq, 5.0 / 3.0);
}

/**
 * @brief SpectrumWithParameters的便捷包装：使用给定的平均风速，其余参数从cfg取默认值
 * @param cfg   仿真配置
 * @param comp  速度分量索引（0/1/2）
 * @param freq  频率 [Hz]
 * @param uMean 平均风速 [m/s]
 * @return      谱值 S(f) [m²/s]
 * @code double su = SpectrumAtMeanU(cfg, 0, 0.5, 12.0); @endcode
 */
double SpectrumAtMeanU(const SimWindConfig &cfg, int comp, double freq, double uMean)
{
	return SpectrumWithParameters(cfg,
	                              comp,
	                              freq,
	                              uMean,
	                              cfg.sigma[static_cast<std::size_t>(comp)],
	                              cfg.integralScale[static_cast<std::size_t>(comp)],
	                              cfg.hubHeight);
}

/**
 * @brief SpectrumAtMeanU的最简包装：使用轮毂高度平均风速cfg.uHub
 * @param cfg  仿真配置
 * @param comp 速度分量索引（0/1/2）
 * @param freq 频率 [Hz]
 * @return     谱值 S(f) [m²/s]
 * @code double su = SpectrumAt(cfg, 0, 1.0); @endcode
 */
double SpectrumAt(const SimWindConfig &cfg, int comp, double freq)
{
	return SpectrumAtMeanU(cfg, comp, freq, cfg.uHub);
}

/**
 * @brief 计算空间两点之间的相干函数值 Coh(dy, dz, f)
 * @param cfg   仿真配置，包含相干模型、衰减系数、指数等参数
 * @param comp  速度分量索引（0/1/2），API模型仅支持u分量
 * @param freq  频率 [Hz]
 * @param dy    两点横向距离（y方向）[m]
 * @param dz    两点竖向距离（z方向）[m]
 * @param meanU 两点平均风速 [m/s]，用于归一化距离
 * @param z1    点1高度 [m]，用于API模型的几何平均高度及SMOOTH模型的尺度计算
 * @param z2    点2高度 [m]
 * @return      相干值∈[0,1]；距离≤kTiny时返回1.0；NONE模型返回0.0
 * @note        支持三种模型：API（u分量专用，含ay/az方向性衰减）、
 *              IEC（exp(-a·√(f²·d²/U²+b²·d²))）、
 *              SMOOTH（含高度尺度归一化指数的衰减模型）。
 * @code double coh = CoherenceAtOffsets(cfg, 0, 0.5, 5.0, 3.0, 12.0, 90.0, 95.0); @endcode
 */
double CoherenceAtOffsets(const SimWindConfig &cfg,
                          int comp,
                          double freq,
                          double dy,
                          double dz,
                          double meanU,
                          double z1,
                          double z2)
{
	const CohModel model = cfg.cohModel[static_cast<std::size_t>(comp)];
	const double distance = std::sqrt(dy * dy + dz * dz);

	if (distance <= kTiny)
		return 1.0;
	if (model == CohModel::NONE)
		return 0.0;

	if (model == CohModel::API)
	{
		if (comp != 0)
			return 0.0;

		const double zGeom = std::sqrt(std::max(z1, 0.1) * std::max(z2, 0.1)) / 10.0;
		const double ay = 45.0 * std::pow(std::max(freq, 0.0), 0.92) *
		                  std::pow(std::abs(dy), 1.0) *
		                  std::pow(std::max(zGeom, 1.0e-3), -0.40);
		const double az = 13.0 * std::pow(std::max(freq, 0.0), 0.85) *
		                  std::pow(std::abs(dz), 1.25) *
		                  std::pow(std::max(zGeom, 1.0e-3), -0.50);
		return std::exp(-std::sqrt(ay * ay + az * az) / std::max(cfg.input.meanWindSpeed, 0.1));
	}

	const double decay = cfg.cohDecay[static_cast<std::size_t>(comp)];
	const double b = cfg.cohB[static_cast<std::size_t>(comp)];
	const double expValue = cfg.input.cohExp > 0.0 ? cfg.input.cohExp : 0.0;
	if (decay >= kHugeDecay * 0.5)
		return 0.0;

	const double meanSpeed = std::max(meanU, 0.1);
	const double distU = distance / meanSpeed;
	if (model == CohModel::IEC)
		return std::exp(-decay * std::sqrt(std::pow(freq * distU, 2.0) + std::pow(b * distance, 2.0)));

	const double localZ = 0.5 * (std::max(z1, 0.0) + std::max(z2, 0.0));
	const double distExp = expValue > 0.0 && localZ > 0.0 ? -std::pow(distance / localZ, expValue) : -1.0;
	return std::exp(decay * distExp * std::sqrt(std::pow(freq * distU, 2.0) + std::pow(b * distance, 2.0)));
}

/**
 * @brief 填充N×N频谱矩阵：对角线存入各点PSD，非对角线填入PSD乘积开方×相干系数
 * @param cfg        仿真配置
 * @param comp       速度分量索引
 * @param freq       当前频率 [Hz]
 * @param psdByPoint 每个空间点的PSD值（长度=nPoints）
 * @param matrix     输出的N×N对称矩阵（扁平存储，行优先），仅下三角有意义
 * @note             matrix[i*n+i]=psd[i]；matrix[i*n+j]=√(psd[i]·psd[j])·Coh(i,j)。
 *                   若cohDecay≥kHugeDecay/2则只写对角线（即无空间相干）。
 * @code
 *   std::vector<double> m(n*n);
 *   FillSpectralMatrix(cfg, 0, 0.5, psdVec, m);
 * @endcode
 */
void FillSpectralMatrix(const SimWindConfig &cfg,
                        int comp,
                        double freq,
                        const std::vector<double> &psdByPoint,
                        std::vector<double> &matrix)
{
	const int n = cfg.nPoints;
	std::fill(matrix.begin(), matrix.end(), 0.0);
	for (int i = 0; i < n; ++i)
		matrix[static_cast<std::size_t>(i) * n + i] = std::max(psdByPoint[static_cast<std::size_t>(i)], 0.0);

	if (cfg.cohDecay[static_cast<std::size_t>(comp)] >= kHugeDecay * 0.5)
		return;

	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j < i; ++j)
		{
			const double dy = cfg.y[static_cast<std::size_t>(i)] - cfg.y[static_cast<std::size_t>(j)];
			const double dz = cfg.z[static_cast<std::size_t>(i)] - cfg.z[static_cast<std::size_t>(j)];
			const double uMean = std::max(0.5 * (cfg.meanU[static_cast<std::size_t>(i)] + cfg.meanU[static_cast<std::size_t>(j)]), 0.1);
			const double coherence = CoherenceAtOffsets(cfg,
			                                           comp,
			                                           freq,
			                                           dy,
			                                           dz,
			                                           uMean,
			                                           cfg.z[static_cast<std::size_t>(i)],
			                                           cfg.z[static_cast<std::size_t>(j)]);
			const double value = std::sqrt(std::max(psdByPoint[static_cast<std::size_t>(i)], 0.0) *
			                               std::max(psdByPoint[static_cast<std::size_t>(j)], 0.0)) *
			                     coherence;
			matrix[static_cast<std::size_t>(i) * n + j] = value;
			matrix[static_cast<std::size_t>(j) * n + i] = value;
		}
	}
}

/**
 * @brief 尝试对N×N对称正定矩阵进行 Cholesky 分解 L·Lᵀ，失败返回false
 * @param matrix 输入矩阵（扁平存储，行优先，仅下三角被引用）
 * @param n      矩阵维度
 * @param lower  输出的下三角矩阵L（扁平存储），失败时内容未定义
 * @param cfg    可选配置指针，用于大矩阵（n≥200）时输出进度报告
 * @return       分解成功返回true；遇到非正对角元或非有限值返回false
 * @note         i==j对角元计算 L[i][i]=√sum；i>j时L[i][j]=sum/L[j][j]。
 *               进度估算使用行索引三次方作为完成度预测（O(n³)特性）。
 * @code TryCholesky(denseMatrix, n, lower, &cfg); @endcode
 */
bool TryCholesky(const std::vector<double> &matrix, int n, std::vector<double> &lower, const SimWindConfig *cfg = nullptr)
{
	std::fill(lower.begin(), lower.end(), 0.0);
	const bool reportRows = cfg != nullptr && n >= 200;
	const int reportEvery = std::max(1, n / 20);
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j <= i; ++j)
		{
			double sum = matrix[static_cast<std::size_t>(i) * n + j];
			for (int k = 0; k < j; ++k)
				sum -= lower[static_cast<std::size_t>(i) * n + k] * lower[static_cast<std::size_t>(j) * n + k];

			if (i == j)
			{
				if (sum <= 0.0 || !std::isfinite(sum))
					return false;
				lower[static_cast<std::size_t>(i) * n + j] = std::sqrt(sum);
			}
			else
			{
				const double diag = lower[static_cast<std::size_t>(j) * n + j];
				if (std::abs(diag) <= kTiny)
					return false;
				lower[static_cast<std::size_t>(i) * n + j] = sum / diag;
			}
		}

		if (reportRows && (i == 0 || (i + 1) % reportEvery == 0 || i + 1 == n))
		{
			const int completed = i + 1;
			const int percent = static_cast<int>(std::round(100.0 * completed / n));
			const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
			const double rowFraction = static_cast<double>(completed) / static_cast<double>(n);
			const double progress = std::max(std::pow(rowFraction, 3.0), 1.0e-9);
			const double remaining = progress > 0.0 ? elapsed * (1.0 / progress - 1.0) : 0.0;
			Report(*cfg,
			       "        Cholesky row progress " + std::to_string(percent) +
			           "%, elapsed " + FormatDuration(elapsed) +
			           ", ETA " + FormatDuration(remaining) + ".");
		}
	}
	return true;
}

/**
 * @brief 强制Cholesky分解：尝试失败时逐步添加对角线抖动(jitter)直到正定
 * @param matrix 输入矩阵（按值传递，内部可修改），扁平存储
 * @param n      矩阵维度
 * @param cfg    可选配置指针，首次尝试传递以显示进度
 * @return       分解成功的下三角矩阵L
 * @throw        8次尝试（jitter=10⁻¹⁰~10⁻³）全部失败时抛出runtime_error
 * @note         每次失败后在所有对角元上加jitter=10^(-10+attempt)，重新尝试。
 * @code auto lower = StrictCholeskyL(denseMatrix, nPoints, &cfg); @endcode
 */
std::vector<double> StrictCholeskyL(std::vector<double> matrix, int n, const SimWindConfig *cfg = nullptr)
{
	std::vector<double> lower(static_cast<std::size_t>(n) * n, 0.0);
	for (int attempt = 0; attempt < 8; ++attempt)
	{
		if (TryCholesky(matrix, n, lower, attempt == 0 ? cfg : nullptr))
			return lower;

		const double jitter = std::pow(10.0, -10 + attempt);
		for (int i = 0; i < n; ++i)
			matrix[static_cast<std::size_t>(i) * n + i] += jitter;
	}

	throw std::runtime_error(L_WIND_CholeskyNotPD);
}

/**
 * @brief 下三角矩阵乘以复数列向量：result = L·phase（仅用下三角元素）
 * @param lower  下三角矩阵L（扁平存储，行优先）
 * @param phase  复数列向量（长度n）
 * @param n      向量/矩阵维度
 * @param result 输出复向量 result[i]=Σ_{j≤i} L[i*n+j]·phase[j]
 * @code MultiplyLower(lower, randomPhase, nPoints, correlated); @endcode
 */
void MultiplyLower(const std::vector<double> &lower,
                   const std::vector<std::complex<double>> &phase,
                   int n,
                   std::vector<std::complex<double>> &result)
{
	result.assign(static_cast<std::size_t>(n), std::complex<double>(0.0, 0.0));
	for (int i = 0; i < n; ++i)
	{
		std::complex<double> sum(0.0, 0.0);
		for (int j = 0; j <= i; ++j)
			sum += lower[static_cast<std::size_t>(i) * n + j] * phase[static_cast<std::size_t>(j)];
		result[static_cast<std::size_t>(i)] = sum;
	}
}

/**
 * @brief 对2D复数据应用右乘下三角转置：data = data·Lᵀ（数据按列优先布局）
 * @param lower   下三角矩阵L（cols×cols，扁平存储）
 * @param rows    数据行数（空间点数）
 * @param cols    数据列数（=L的维度）
 * @param data    复矩阵数据，列优先（row + col*rows），原地修改
 * @param scratch 临时缓冲区
 * @note         data[row+col*rows] = Σ_{k≤col} data[row+k*rows]·L[col*cols+k]
 * @code ApplyRightLowerTranspose(lz, cfg.ny, cfg.nz, correlated, scratch); @endcode
 */
void ApplyRightLowerTranspose(const std::vector<double> &lower,
                              int rows,
                              int cols,
                              std::vector<std::complex<double>> &data,
                              std::vector<std::complex<double>> &scratch)
{
	scratch.assign(data.size(), std::complex<double>(0.0, 0.0));
	for (int col = 0; col < cols; ++col)
	{
		for (int row = 0; row < rows; ++row)
		{
			std::complex<double> sum(0.0, 0.0);
			for (int k = 0; k <= col; ++k)
				sum += data[static_cast<std::size_t>(row + k * rows)] *
				       lower[static_cast<std::size_t>(col) * cols + k];
			scratch[static_cast<std::size_t>(row + col * rows)] = sum;
		}
	}
	data.swap(scratch);
}

/**
 * @brief 对2D复数据应用左乘下三角：data = L·data（数据列优先布局）
 * @param lower   下三角矩阵L（rows×rows，扁平存储）
 * @param rows    数据行数（=L的维度）
 * @param cols    数据列数
 * @param data    复矩阵数据，列优先（row + col*rows），原地修改
 * @param scratch 临时缓冲区
 * @note         data[row+col*rows] = Σ_{k≤row} L[row*rows+k]·data[k+col*rows]
 * @code ApplyLeftLower(ly, cfg.ny, cfg.nz, correlated, scratch); @endcode
 */
void ApplyLeftLower(const std::vector<double> &lower,
                    int rows,
                    int cols,
                    std::vector<std::complex<double>> &data,
                    std::vector<std::complex<double>> &scratch)
{
	scratch.assign(data.size(), std::complex<double>(0.0, 0.0));
	for (int col = 0; col < cols; ++col)
	{
		for (int row = 0; row < rows; ++row)
		{
			std::complex<double> sum(0.0, 0.0);
			for (int k = 0; k <= row; ++k)
				sum += lower[static_cast<std::size_t>(row) * rows + k] *
				       data[static_cast<std::size_t>(k + col * rows)];
			scratch[static_cast<std::size_t>(row + col * rows)] = sum;
		}
	}
	data.swap(scratch);
}

/**
 * @brief 计算数据向量的标准差 σ = √(Σ(xᵢ-μ)²/N)
 * @param values 数据向量
 * @return       标准差；空向量返回0.0
 * @code double sigma = ComponentSigma(component); @endcode
 */
double ComponentSigma(const std::vector<double> &values)
{
	if (values.empty())
		return 0.0;
	const double mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
	double sum2 = 0.0;
	for (double value : values)
		sum2 += (value - mean) * (value - mean);
	return std::sqrt(sum2 / static_cast<double>(values.size()));
}

/**
 * @brief 将数据去均值后缩放至目标标准差（原地修改）
 * @param values      数据向量（原地修改）
 * @param targetSigma 目标标准差，≤0或数据为空则直接返回
 * @note              scale = targetSigma / 实际σ；实际σ≤kTiny则不缩放。
 * @code ScaleZeroMeanComponent(uComponent, 2.5); @endcode
 */
void ScaleZeroMeanComponent(std::vector<double> &values, double targetSigma)
{
	if (targetSigma <= 0.0 || values.empty())
		return;

	const double mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
	for (double &value : values)
		value -= mean;

	const double actual = ComponentSigma(values);
	if (actual > kTiny)
	{
		const double scale = targetSigma / actual;
		for (double &value : values)
			value *= scale;
	}
}

/**
 * @brief 在网格中搜索最接近轮毂高度(y=0, z=hubHeight)的空间点索引
 * @param cfg 仿真配置，含坐标数组和网格尺寸
 * @return    最近网格点的Flatten线性索引
 * @note      分别在y、z方向独立搜索最小|Δy|和|Δz|后通过GridIndex组合。
 * @code int hubPoint = HubPointIndex(cfg); @endcode
 */
int HubPointIndex(const SimWindConfig &cfg)
{
	int hubIy = 0;
	double bestY = std::numeric_limits<double>::max();
	for (int iy = 0; iy < cfg.ny; ++iy)
	{
		const double delta = std::abs(cfg.yCoords[static_cast<std::size_t>(iy)]);
		if (delta < bestY)
		{
			bestY = delta;
			hubIy = iy;
		}
	}

	int hubIz = 0;
	double bestZ = std::numeric_limits<double>::max();
	for (int iz = 0; iz < cfg.nz; ++iz)
	{
		const double delta = std::abs(cfg.zCoords[static_cast<std::size_t>(iz)] - cfg.hubHeight);
		if (delta < bestZ)
		{
			bestZ = delta;
			hubIz = iz;
		}
	}

	return GridIndex(cfg, hubIz, hubIy);
}

/**
 * @brief 计算轮毂高度处某分量时间序列的标准差
 * @param cfg    仿真配置
 * @param values 时间序列（第一维为时间步，第二维为空间点，扁平布局）
 * @return       轮毂点的标准差；空数据或无步数返回0.0
 * @note         先定位HubPointIndex，取该点在所有时间步的值计算σ。
 * @code double sigma = HubPointSigma(cfg, field.component[0]); @endcode
 */
double HubPointSigma(const SimWindConfig &cfg, const std::vector<double> &values)
{
	if (values.empty() || cfg.nSteps <= 0 || cfg.nPoints <= 0)
		return 0.0;

	const int point = HubPointIndex(cfg);
	double mean = 0.0;
	for (int t = 0; t < cfg.nSteps; ++t)
		mean += values[static_cast<std::size_t>(t) * cfg.nPoints + point];
	mean /= static_cast<double>(cfg.nSteps);

	double sum2 = 0.0;
	for (int t = 0; t < cfg.nSteps; ++t)
	{
		const double delta = values[static_cast<std::size_t>(t) * cfg.nPoints + point] - mean;
		sum2 += delta * delta;
	}
	return std::sqrt(sum2 / static_cast<double>(cfg.nSteps));
}

/**
 * @brief 按轮毂点目标σ缩放全时间序列（全局统一缩放因子）
 * @param cfg         仿真配置
 * @param values      时间序列（原地修改）
 * @param targetSigma 目标标准差，≤0或无数据时返回
 * @note              scale = targetσ / 轮毂点实际σ；所有空间点统一乘以scale。
 * @code ScaleComponentToHubSigma(cfg, uValues, 3.0); @endcode
 */
void ScaleComponentToHubSigma(const SimWindConfig &cfg, std::vector<double> &values, double targetSigma)
{
	if (targetSigma <= 0.0 || values.empty())
		return;

	const double actual = HubPointSigma(cfg, values);
	if (actual <= kTiny)
		return;

	const double scale = targetSigma / actual;
	for (double &value : values)
		value *= scale;
}

/**
 * @brief 按目标σ独立缩放每个空间点的时间序列（逐点缩放）
 * @param cfg         仿真配置
 * @param values      时间序列（原地修改）
 * @param targetSigma 目标标准差，≤0或无数据时返回
 * @note              每个空间点分别去均值、计算实际σ、缩放至targetSigma。
 *                   与ScaleComponentToHubSigma不同：每个点独立缩放，不共享全局因子。
 * @code ScaleComponentPerPointSigma(cfg, uValues, 3.0); @endcode
 */
void ScaleComponentPerPointSigma(const SimWindConfig &cfg, std::vector<double> &values, double targetSigma)
{
	if (targetSigma <= 0.0 || values.empty() || cfg.nSteps <= 0 || cfg.nPoints <= 0)
		return;

	for (int point = 0; point < cfg.nPoints; ++point)
	{
		double mean = 0.0;
		for (int t = 0; t < cfg.nSteps; ++t)
			mean += values[static_cast<std::size_t>(t) * cfg.nPoints + point];
		mean /= static_cast<double>(cfg.nSteps);

		double sum2 = 0.0;
		for (int t = 0; t < cfg.nSteps; ++t)
		{
			const double delta = values[static_cast<std::size_t>(t) * cfg.nPoints + point] - mean;
			sum2 += delta * delta;
		}

		const double actual = std::sqrt(sum2 / static_cast<double>(cfg.nSteps));
		if (actual <= kTiny)
			continue;

		const double scale = targetSigma / actual;
		for (int t = 0; t < cfg.nSteps; ++t)
			values[static_cast<std::size_t>(t) * cfg.nPoints + point] *= scale;
	}
}

/**
 * @brief 根据IEC缩放策略对湍流分量应用标准差缩放
 * @param cfg   仿真配置
 * @param field 风场数据（原地修改）
 * @param comp  分量索引（0/1/2）
 * @note        scaleIEC<1时不缩放；scaleIEC==1时u分量逐点缩放，其他分量轮毂点缩放；
 *              scaleIEC>1时全部使用轮毂点缩放。
 * @code ApplyScaleIecForComponent(cfg, field, 0); @endcode
 */
void ApplyScaleIecForComponent(const SimWindConfig &cfg, WindField &field, int comp)
{
	if (cfg.scaleIEC < 1)
		return;

	auto &values = field.component[static_cast<std::size_t>(comp)];
	const double targetSigma = cfg.sigma[static_cast<std::size_t>(comp)];
	if (cfg.scaleIEC == 1 || comp > 0)
		ScaleComponentToHubSigma(cfg, values, targetSigma);
	else
		ScaleComponentPerPointSigma(cfg, values, targetSigma);
}

/**
 * @brief 分配并零初始化一个WindField结构
 * @param cfg 仿真配置，提供nSteps和nPoints
 * @return    三个分量均以0.0填充的WindField
 * @note      每个分量大小=nSteps×nPoints，全部初始化为0。
 * @code WindField field = AllocateField(cfg); @endcode
 */
WindField AllocateField(const SimWindConfig &cfg)
{
	WindField field;
	field.nSteps = cfg.nSteps;
	field.nPoints = cfg.nPoints;
	const std::size_t total = static_cast<std::size_t>(cfg.nSteps) * cfg.nPoints;
	for (auto &component : field.component)
		component.assign(total, 0.0);
	return field;
}

/** @brief 用户风场空间插值权重，将源风场点通过反距离加权映射到目标网格点 */
struct UserWindSpatialWeight
{
	int index = 0;         ///< 源风场数据点索引
	double weight = 0.0;   ///< 归一化反距离平方插值权重，所有点权重之和为 1.0
};

/**
 * @brief 为用户风场目标空间点构建反距离加权插值权重（最多4个最近源点）
 * @param data    用户风速数据（含所有源点坐标）
 * @param targetY 目标点的y坐标 [m]
 * @param targetZ 目标点的z坐标 [m]
 * @return        权重向量，每个元素含源点索引和归一化权重（距离1/d²加权）
 * @note          若某源点与目标重合（d²≤1e-12）则返回单点权重1.0；
 *                最大保留4个最近邻，权重归一化使∑w=1。
 * @code auto weights = BuildUserWindSpatialWeights(data, 5.0, 90.0); @endcode
 */
std::vector<UserWindSpatialWeight> BuildUserWindSpatialWeights(const UserWindSpeedData &data, double targetY, double targetZ)
{
	std::vector<std::pair<double, int>> distances;
	distances.reserve(data.points.size());
	for (int src = 0; src < static_cast<int>(data.points.size()); ++src)
	{
		const double dy = targetY - data.points[static_cast<std::size_t>(src)].y;
		const double dz = targetZ - data.points[static_cast<std::size_t>(src)].z;
		const double d2 = dy * dy + dz * dz;
		if (d2 <= 1.0e-12)
			return {{src, 1.0}};
		distances.emplace_back(d2, src);
	}

	std::sort(distances.begin(), distances.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	const std::size_t keep = std::min<std::size_t>(4, distances.size());
	std::vector<UserWindSpatialWeight> weights;
	weights.reserve(keep);

	double weightSum = 0.0;
	for (std::size_t i = 0; i < keep; ++i)
	{
		const double w = 1.0 / std::max(distances[i].first, 1.0e-12);
		weights.push_back({distances[i].second, w});
		weightSum += w;
	}

	if (weightSum <= kTiny)
		return {};
	for (auto &item : weights)
		item.weight /= weightSum;
	return weights;
}

/**
 * @brief 用户风场时间序列线性插值，可外延到端点
 * @param time       时间点数组（已排序）
 * @param values     风速值数组
 * @param targetTime 目标时间 [s]
 * @return           插值结果；targetTime≤首点取首值，≥末点取末值
 * @note             使用std::lower_bound二分查找区间，线性比例定位。
 * @code double u = InterpolateUserWindTimeSeries(times, uSeries, 12.5); @endcode
 */
double InterpolateUserWindTimeSeries(const std::vector<double> &time,
                                     const std::vector<double> &values,
                                     double targetTime)
{
	if (time.empty() || values.empty())
		return 0.0;
	const std::size_t n = std::min(time.size(), values.size());
	if (n == 1 || targetTime <= time.front())
		return values.front();
	if (targetTime >= time[n - 1])
		return values[n - 1];

	const auto upper = std::lower_bound(time.begin(), time.begin() + static_cast<std::ptrdiff_t>(n), targetTime);
	const std::size_t i1 = static_cast<std::size_t>(std::distance(time.begin(), upper));
	if (i1 == 0)
		return values.front();
	const std::size_t i0 = i1 - 1;
	const double t0 = time[i0];
	const double t1 = time[i1];
	const double span = std::max(t1 - t0, kTiny);
	const double a = (targetTime - t0) / span;
	return values[i0] * (1.0 - a) + values[i1] * a;
}

/**
 * @brief 从用户提供的外部风速时间序列文件加载并插值生成完整风场
 * @param cfg 仿真配置（含输出网格、时间步、userTurbFile路径）
 * @return    填充完毕的WindField
 * @throw     文件为空、时间未排序或插值权重构建失败时抛出runtime_error
 * @note      先为每个输出网格点构建空间插值权重，再逐时间步、逐分量插值。
 * @code WindField field = LoadUserWindSpeedField(cfg); @endcode
 */
WindField LoadUserWindSpeedField(const SimWindConfig &cfg)
{
	const UserWindSpeedData data = ReadUserWindSpeed(cfg.input.userTurbFile);
	if (data.time.empty() || data.components.empty())
		throw std::runtime_error(L_WIND_UserWindSpeedEmpty);

	WindField field = AllocateField(cfg);
	if (!std::is_sorted(data.time.begin(), data.time.end()))
		throw std::runtime_error(L_WIND_TimeNotSorted);

	std::vector<std::vector<UserWindSpatialWeight>> weightsByPoint(static_cast<std::size_t>(cfg.nPoints));

	for (int p = 0; p < cfg.nPoints; ++p)
	{
		weightsByPoint[static_cast<std::size_t>(p)] =
		    BuildUserWindSpatialWeights(data, cfg.y[static_cast<std::size_t>(p)], cfg.z[static_cast<std::size_t>(p)]);
		if (weightsByPoint[static_cast<std::size_t>(p)].empty())
			throw std::runtime_error(L_WIND_SpatialWeightFail);
	}

	for (int p = 0; p < cfg.nPoints; ++p)
	{
		const auto &weights = weightsByPoint[static_cast<std::size_t>(p)];
		for (int t = 0; t < cfg.nSteps; ++t)
		{
			const double targetTime = t * cfg.dt;
			for (int comp = 0; comp < 3; ++comp)
			{
				double value = 0.0;
				for (const auto &item : weights)
				{
					const auto ps = static_cast<std::size_t>(item.index);
					const auto cs = static_cast<std::size_t>(comp);
					if (ps >= data.components.size() || cs >= data.components[ps].size())
						continue;
					value += item.weight * InterpolateUserWindTimeSeries(data.time, data.components[ps][cs], targetTime);
				}
				field.At(comp, t, p) = value;
			}
		}
	}

	return field;
}

/**
 * @brief 按高度填充PSD和sqrt(PSD)数组：为每个z层计算谱值
 * @param cfg       仿真配置
 * @param comp      速度分量索引
 * @param freq      当前频率 [Hz]
 * @param psdByZ    输出的PSD数组（长度nz）
 * @param sqrtPsdByZ 输出的√PSD数组（长度nz），用于后续幅度计算
 * @note            为每个高度层调用SpectrumWithParameters，传入该层的meanU、σ和积分尺度。
 * @code FillPsdByHeight(cfg, 0, 0.5, psdByZ, sqrtPsdByZ); @endcode
 */
void FillPsdByHeight(const SimWindConfig &cfg,
                     int comp,
                     double freq,
                     std::vector<double> &psdByZ,
                     std::vector<double> &sqrtPsdByZ)
{
	psdByZ.resize(static_cast<std::size_t>(cfg.nz));
	sqrtPsdByZ.resize(static_cast<std::size_t>(cfg.nz));
	for (int iz = 0; iz < cfg.nz; ++iz)
	{
		const double psd = std::max(SpectrumWithParameters(cfg,
		                                                   comp,
		                                                   freq,
		                                                   cfg.meanUByZ[static_cast<std::size_t>(iz)],
		                                                   LocalSigmaAtZ(cfg, comp, iz),
		                                                   LocalLengthScaleAtZ(cfg, comp, iz),
		                                                   cfg.zCoords[static_cast<std::size_t>(iz)]),
		                            0.0);
		psdByZ[static_cast<std::size_t>(iz)] = psd;
		sqrtPsdByZ[static_cast<std::size_t>(iz)] = std::sqrt(psd);
	}
}

/**
 * @brief 生成单个湍流分量的全频率随机相位频谱，通过IFFT得到时间序列
 * @param cfg   仿真配置，含网格、频率、相干性等参数
 * @param comp  分量索引（0=u, 1=v, 2=w）
 * @param rng   随机数生成器（Mersenne Twister 64位），用于相位生成
 * @param field 输出的风场（写入comp分量）
 * @note        流程：(1)遍历频率k=1..nFreq-1；(2)FillPsdByHeight计算各高度层PSD；
 *              (3)根据相干性策略构建频谱矩阵：Kronecker分解（y/z方向分别Cholesky）、
 *              全矩阵Cholesky、或对角（无空间相干）；
 *              (4)频谱对称共轭填充nSteps-k位置；(5)批量IFFT后缩放1/nSteps写入field。
 *              跳过分量未启用或标准差为0的情况。含频率和复制进度报告。
 * @code GenerateSpectralComponent(cfg, 0, rng, field); @endcode
 */
void GenerateSpectralComponent(const SimWindConfig &cfg, int comp, std::mt19937_64 &rng, WindField &field)
{
	if (!ComponentEnabled(cfg.input, comp) || cfg.sigma[static_cast<std::size_t>(comp)] <= 0.0)
	{
		char buf[128];
		std::snprintf(buf, sizeof(buf), L_PROG_CompSkipped, comp + 1);
		Report(cfg, buf);
		return;
	}

	const bool strictCoherence = UsesStrictCoherence(cfg, comp);
	const bool useKronecker = strictCoherence && cfg.useKronecker[static_cast<std::size_t>(comp)];
	const int kroneckerFreqLimit = useKronecker ? cfg.kroneckerFreqLimit[static_cast<std::size_t>(comp)] : 0;
	Report(cfg,
	       std::string(L_PROG_CompMatrices) + std::string(comp == 0 ? "u" : (comp == 1 ? "v" : "w")) + ") (" +
	           (strictCoherence ? (useKronecker ? L_PROG_LegacyKronecker : L_PROG_StrictCoherence)
	                            : L_PROG_UncorrelatedPhases) +
	           ").");
	if (useKronecker)
	{
		Report(cfg,
		       std::string(L_PROG_KroneckerActive) + std::to_string(kroneckerFreqLimit) + L_PROG_OfFreq +
		           std::to_string(std::max(cfg.nFreq - 1, 0)) + L_PROG_HigherDiag);
	}

	FftwBatchPlan1D spectrum(cfg.nSteps, cfg.nPoints);
	spectrum.ZeroAll();
	std::uniform_real_distribution<double> phaseDist(0.0, 2.0 * kPi);
	std::vector<std::complex<double>> randomPhase(static_cast<std::size_t>(cfg.nPoints));
	std::vector<std::complex<double>> correlated;
	std::vector<std::complex<double>> scratch;
	std::vector<double> psdByZ;
	std::vector<double> sqrtPsdByZ;
	std::vector<double> psdByPoint;
	std::vector<double> denseSpectral;
	std::vector<double> sy;
	std::vector<double> sz;
	if (strictCoherence && !useKronecker)
		denseSpectral.assign(static_cast<std::size_t>(cfg.nPoints) * cfg.nPoints, 0.0);
	if (useKronecker)
	{
		sy.assign(static_cast<std::size_t>(cfg.ny) * cfg.ny, 0.0);
		sz.assign(static_cast<std::size_t>(cfg.nz) * cfg.nz, 0.0);
	}

	const int reportEvery = std::max(1, cfg.nFreq / 10);
	const auto freqStart = std::chrono::steady_clock::now();
	Report(cfg, L_PROG_FreqUnknownETA);

	for (int k = 1; k < cfg.nFreq; ++k)
	{
		const double freq = k * cfg.df;
		FillPsdByHeight(cfg, comp, freq, psdByZ, sqrtPsdByZ);
		const bool hasEnergy = std::any_of(psdByZ.begin(), psdByZ.end(), [](double value) { return value > 0.0; });
		if (!hasEnergy)
			continue;

		for (int p = 0; p < cfg.nPoints; ++p)
		{
			const double phi = phaseDist(rng);
			randomPhase[static_cast<std::size_t>(p)] = std::complex<double>(std::cos(phi), std::sin(phi));
		}

		const double amplitude = cfg.nSteps * std::sqrt(cfg.df / 2.0);
		if (strictCoherence && useKronecker && k <= kroneckerFreqLimit)
		{
			std::fill(sy.begin(), sy.end(), 0.0);
			std::fill(sz.begin(), sz.end(), 0.0);

			for (int iz = 0; iz < cfg.nz; ++iz)
			{
				sz[static_cast<std::size_t>(iz) * cfg.nz + iz] = psdByZ[static_cast<std::size_t>(iz)];
				for (int kz = 0; kz < iz; ++kz)
				{
					const double dz = std::abs(cfg.zCoords[static_cast<std::size_t>(iz)] - cfg.zCoords[static_cast<std::size_t>(kz)]);
					const double meanU = 0.5 * (cfg.meanUByZ[static_cast<std::size_t>(iz)] + cfg.meanUByZ[static_cast<std::size_t>(kz)]);
					const double coherence = CoherenceAtOffsets(cfg,
					                                           comp,
					                                           freq,
					                                           0.0,
					                                           dz,
					                                           meanU,
					                                           cfg.zCoords[static_cast<std::size_t>(iz)],
					                                           cfg.zCoords[static_cast<std::size_t>(kz)]);
					const double value = sqrtPsdByZ[static_cast<std::size_t>(iz)] *
					                     sqrtPsdByZ[static_cast<std::size_t>(kz)] * coherence;
					sz[static_cast<std::size_t>(iz) * cfg.nz + kz] = value;
					sz[static_cast<std::size_t>(kz) * cfg.nz + iz] = value;
				}
			}

			const double representativeU = cfg.meanUByZ.empty()
			                                   ? std::max(cfg.uHub, 0.1)
			                                   : std::max(cfg.meanUByZ[static_cast<std::size_t>(cfg.nz / 2)], 0.1);
			for (int iy = 0; iy < cfg.ny; ++iy)
			{
				sy[static_cast<std::size_t>(iy) * cfg.ny + iy] = 1.0;
				for (int jy = 0; jy < iy; ++jy)
				{
					const double dy = std::abs(cfg.yCoords[static_cast<std::size_t>(iy)] - cfg.yCoords[static_cast<std::size_t>(jy)]);
					const double coherence = CoherenceAtOffsets(cfg,
					                                           comp,
					                                           freq,
					                                           dy,
					                                           0.0,
					                                           representativeU,
					                                           cfg.hubHeight,
					                                           cfg.hubHeight);
					sy[static_cast<std::size_t>(iy) * cfg.ny + jy] = coherence;
					sy[static_cast<std::size_t>(jy) * cfg.ny + iy] = coherence;
				}
			}

			correlated = randomPhase;
			const auto lz = StrictCholeskyL(sz, cfg.nz);
			const auto ly = StrictCholeskyL(sy, cfg.ny);
			ApplyRightLowerTranspose(lz, cfg.ny, cfg.nz, correlated, scratch);
			ApplyLeftLower(ly, cfg.ny, cfg.nz, correlated, scratch);
			for (int p = 0; p < cfg.nPoints; ++p)
			{
				const std::complex<double> value = amplitude * correlated[static_cast<std::size_t>(p)];
				spectrum.SetSpectrum(k, p, value);
				spectrum.SetSpectrum(cfg.nSteps - k, p, std::conj(value));
			}
		}
		else if (strictCoherence && !useKronecker)
		{
			psdByPoint.resize(static_cast<std::size_t>(cfg.nPoints));
			for (int iz = 0; iz < cfg.nz; ++iz)
			{
				for (int iy = 0; iy < cfg.ny; ++iy)
					psdByPoint[static_cast<std::size_t>(GridIndex(cfg, iz, iy))] = psdByZ[static_cast<std::size_t>(iz)];
			}
			FillSpectralMatrix(cfg, comp, freq, psdByPoint, denseSpectral);
			const auto l = StrictCholeskyL(denseSpectral, cfg.nPoints, &cfg);
			MultiplyLower(l, randomPhase, cfg.nPoints, correlated);
			for (int p = 0; p < cfg.nPoints; ++p)
			{
				const std::complex<double> value = amplitude * correlated[static_cast<std::size_t>(p)];
				spectrum.SetSpectrum(k, p, value);
				spectrum.SetSpectrum(cfg.nSteps - k, p, std::conj(value));
			}
		}
		else
		{
			for (int iz = 0; iz < cfg.nz; ++iz)
			{
				const double localAmplitude = amplitude * sqrtPsdByZ[static_cast<std::size_t>(iz)];
				for (int iy = 0; iy < cfg.ny; ++iy)
				{
					const int p = GridIndex(cfg, iz, iy);
					const std::complex<double> value = localAmplitude * randomPhase[static_cast<std::size_t>(p)];
					spectrum.SetSpectrum(k, p, value);
					spectrum.SetSpectrum(cfg.nSteps - k, p, std::conj(value));
				}
			}
		}

		if (k == 1 || k % reportEvery == 0 || k == cfg.nFreq - 1)
		{
			const int totalFreq = std::max(cfg.nFreq - 1, 1);
			const int percent = static_cast<int>(std::round(100.0 * k / totalFreq));
			const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - freqStart).count();
			const double perFreq = elapsed / static_cast<double>(k);
			const double remaining = perFreq * static_cast<double>(totalFreq - k);
			Report(cfg,
			       std::string(L_PROG_FreqProgress) + std::to_string(percent) + "%" + L_PROG_Elapsed +
			           FormatDuration(elapsed) + L_PROG_ETA + FormatDuration(remaining) + ".");
		}
	}

	Report(cfg, L_PROG_IFFTBatch);
	const auto ifftStart = std::chrono::steady_clock::now();
	spectrum.Execute();
	const double ifftElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - ifftStart).count();
	Report(cfg, std::string(L_PROG_IFFTDone) + FormatDuration(ifftElapsed) + ".");

	const double invN = 1.0 / static_cast<double>(cfg.nSteps);
	const int copyReportEvery = std::max(1, cfg.nSteps / 10);
	const auto copyStart = std::chrono::steady_clock::now();
	for (int t = 0; t < cfg.nSteps; ++t)
	{
		for (int p = 0; p < cfg.nPoints; ++p)
			field.At(comp, t, p) = invN * spectrum.OutputReal(t, p);

		const int completed = t + 1;
		if (completed == 1 || completed % copyReportEvery == 0 || completed == cfg.nSteps)
		{
			const int percent = static_cast<int>(std::round(100.0 * completed / std::max(cfg.nSteps, 1)));
			const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - copyStart).count();
			const double perStep = elapsed / static_cast<double>(completed);
			const double remaining = perStep * static_cast<double>(cfg.nSteps - completed);
			Report(cfg,
			       std::string(L_PROG_CopyProgress) + std::to_string(percent) + "%" + L_PROG_Elapsed +
			           FormatDuration(elapsed) + L_PROG_ETA + FormatDuration(remaining) + ".");
		}
	}

	ApplyScaleIecForComponent(cfg, field, comp);
	Report(cfg, L_PROG_ComponentComplete);
}

/**
 * @brief DFT波数计算：将FFT索引映射为物理波数 k = 2π·i/L（带符号）
 * @param index  FFT索引（0..count-1）
 * @param count  FFT点数
 * @param length 物理域长度 [m]，被钳位至≥kTiny
 * @return       有符号波数 [rad/m]，正频率段在index≤count/2
 * @code double k1 = DftWaveNumber(ix, nx, lx); @endcode
 */
double DftWaveNumber(int index, int count, double length)
{
	const int signedIndex = index <= count / 2 ? index : index - count;
	return 2.0 * kPi * static_cast<double>(signedIndex) / std::max(length, kTiny);
}

/**
 * @brief Mann谱张量的3×3 Cholesky分解 L·Lᵀ = Φ(k₁,k₂,k₃)
 * @param cfg   仿真配置，提供Mann参数（gamma, length）和alphaEps
 * @param k1    波数分量1（纵向）[rad/m]
 * @param k2    波数分量2（横向）[rad/m]
 * @param k3    波数分量3（竖向）[rad/m]
 * @param lower 输出的下三角矩阵（6个元素按列优先: L00,L10,L11,L20,L21,L22）
 * @return      分解成功返回true，8次jitter尝试均失败返回false
 * @note        基础公式：Φ_ij由各向同性张量+剪切修正构成，k₀=k₃+γk₁。
 *              能量E∝αε^(2/3)·k₀L⁴/(1+k₀²L²)^(17/6)，数值不稳定时递增jitter重试。
 * @code
 *   std::array<double,6> lower;
 *   if (MannLowerCholesky(cfg, k1, k2, k3, lower)) { ... }
 * @endcode
 */
bool MannLowerCholesky(const SimWindConfig &cfg,
                       double k1,
                       double k2,
                       double k3,
                       std::array<double, 6> &lower)
{
	lower.fill(0.0);

	const double gamma = cfg.mannGamma;
	const double length = std::max(cfg.mannLength, kTiny);
	const double alphaEps = cfg.input.mannAlphaEps > 0.0 ? cfg.input.mannAlphaEps : 0.05;
	const double k03 = k3 + gamma * k1;
	const double k0sq = k1 * k1 + k2 * k2 + k03 * k03;
	if (k0sq <= 1.0e-30)
		return false;

	const double k0 = std::sqrt(k0sq);
	const double k0L = k0 * length;
	const double k0L2 = k0L * k0L;
	const double energy = alphaEps * std::pow(length, 5.0 / 3.0) *
	                      k0L2 * k0L2 / std::pow(1.0 + k0L2, 17.0 / 6.0);
	const double c0 = energy / (4.0 * kPi * k0sq * k0sq);
	if (c0 <= 0.0 || !std::isfinite(c0))
		return false;

	const double phi00Iso = c0 * (k2 * k2 + k03 * k03);
	const double phi11Iso = c0 * (k1 * k1 + k03 * k03);
	const double phi22Iso = c0 * (k1 * k1 + k2 * k2);
	const double phi01Iso = -c0 * k1 * k2;
	const double phi02Iso = -c0 * k1 * k03;
	const double phi12Iso = -c0 * k2 * k03;

	double a00 = phi00Iso + 2.0 * gamma * phi02Iso + gamma * gamma * phi22Iso;
	double a01 = phi01Iso + gamma * phi12Iso;
	double a02 = phi02Iso + gamma * phi22Iso;
	double a11 = phi11Iso;
	double a12 = phi12Iso;
	double a22 = phi22Iso;

	const double baseJitter = std::max((std::abs(a00) + std::abs(a11) + std::abs(a22)) * 1.0e-12, 1.0e-30);
	for (int attempt = 0; attempt < 8; ++attempt)
	{
		const double jitter = baseJitter * std::pow(10.0, attempt);
		const double b00 = a00 + jitter;
		const double b11 = a11 + jitter;
		const double b22 = a22 + jitter;
		if (b00 <= 0.0)
			continue;

		const double l00 = std::sqrt(b00);
		const double l10 = a01 / l00;
		const double l20 = a02 / l00;
		const double d11 = b11 - l10 * l10;
		if (d11 <= 0.0)
			continue;

		const double l11 = std::sqrt(d11);
		const double l21 = (a12 - l20 * l10) / l11;
		const double d22 = b22 - l20 * l20 - l21 * l21;
		if (d22 <= 0.0)
			continue;

		lower = {l00, l10, l11, l20, l21, std::sqrt(d22)};
		return true;
	}

	return false;
}

/**
 * @brief 将输出网格的物理坐标映射到Mann FFT网格的最接近离散索引
 * @param coord    物理坐标 [m]
 * @param minCoord 网格起始坐标 [m]
 * @param width    网格宽度 [m]
 * @param count    FFT网格点数
 * @return         最近索引∈[0,count-1]
 * @note           使用lround四舍五入后clamp到有效范围。
 * @code int mix = NearestMannIndex(z, zMin, gridHeight, nz); @endcode
 */
int NearestMannIndex(double coord, double minCoord, double width, int count)
{
	if (count <= 1 || width <= kTiny)
		return 0;
	const double position = (coord - minCoord) / width * static_cast<double>(count - 1);
	return std::clamp(static_cast<int>(std::lround(position)), 0, count - 1);
}

/**
 * @brief 基于Mann 3D谱张量模型生成完整湍流风场（u/v/w三分量同时生成）
 * @param cfg 仿真配置，含Mann参数、网格和FFT尺寸
 * @return    已填充三速度分量的WindField
 * @note      流程：(1)按Mann谱张量填充3D复振幅体(uHat/vHat/wHat)；
 *            (2)对每个(k₁,k₂,k₃)调用MannLowerCholesky，用复高斯随机数乘以
 *            分解因子得到各分量振幅；(3)遵守共轭对称性只填充半个谱；
 *            (4)对三个谱体同步批量IFFT得到物理空间场；
 *            (5)通过NearestMannIndex将FFT网格采样到输出WindField网格。
 *            分量未启用时整列为零。含降采样到输出网格和ApplyScaleIecForComponent。
 * @code WindField field = GenerateMannWindField(cfg); @endcode
 */
WindField GenerateMannWindField(const SimWindConfig &cfg)
{
	const int nx = std::max(2, cfg.mannFftPoints);
	const int ny = std::max(1, cfg.mannGridY);
	const int nz = std::max(1, cfg.mannGridZ);
	const double lx = std::max(cfg.uHub * cfg.duration, cfg.uHub * cfg.dt * static_cast<double>(nx));
	const double ly = std::max(cfg.gridWidth, kTiny);
	const double lz = std::max(cfg.gridHeight, kTiny);
	const double dkVolume = (2.0 * kPi / lx) * (2.0 * kPi / ly) * (2.0 * kPi / lz);
	const double fftScale = std::sqrt(std::max(dkVolume, 0.0)) * static_cast<double>(nx) * ny * nz;

	Report(cfg,
	       std::string(L_PROG_MannCalcTensor) + std::to_string(nx) + " x " +
	           std::to_string(ny) + " x " + std::to_string(nz) + ".");

	FftwComplexVolume uHat(nx, ny, nz);
	FftwComplexVolume vHat(nx, ny, nz);
	FftwComplexVolume wHat(nx, ny, nz);
	std::mt19937_64 rng(static_cast<std::uint64_t>(cfg.input.turbSeed));
	std::normal_distribution<double> normal(0.0, 1.0);
	const double invSqrt2 = 1.0 / std::sqrt(2.0);
	const int reportEvery = std::max(1, nx / 10);
	const auto start = std::chrono::steady_clock::now();

	for (int ix = 0; ix < nx; ++ix)
	{
		const int nix = (nx - ix) % nx;
		const double k1 = DftWaveNumber(ix, nx, lx);
		for (int iy = 0; iy < ny; ++iy)
		{
			const int niy = (ny - iy) % ny;
			const double k2 = DftWaveNumber(iy, ny, ly);
			for (int iz = 0; iz < nz; ++iz)
			{
				const int niz = (nz - iz) % nz;
				const std::size_t index = uHat.Index(ix, iy, iz);
				const std::size_t mirror = uHat.Index(nix, niy, niz);
				if (index > mirror)
					continue;

				std::array<double, 6> lower{};
				const double k3 = DftWaveNumber(iz, nz, lz);
				if (!MannLowerCholesky(cfg, k1, k2, k3, lower))
					continue;

				const bool selfConjugate = index == mirror;
				const auto gaussian = [&]() -> std::complex<double>
				{
					if (selfConjugate)
						return {normal(rng), 0.0};
					return {normal(rng) * invSqrt2, normal(rng) * invSqrt2};
				};

				const auto z0 = gaussian();
				const auto z1 = gaussian();
				const auto z2 = gaussian();
				const std::complex<double> u = fftScale * lower[0] * z0;
				const std::complex<double> v = fftScale * (lower[1] * z0 + lower[2] * z1);
				const std::complex<double> w = fftScale * (lower[3] * z0 + lower[4] * z1 + lower[5] * z2);
				uHat.Set(ix, iy, iz, u);
				vHat.Set(ix, iy, iz, v);
				wHat.Set(ix, iy, iz, w);
				if (!selfConjugate)
				{
					uHat.Set(nix, niy, niz, std::conj(u));
					vHat.Set(nix, niy, niz, std::conj(v));
					wHat.Set(nix, niy, niz, std::conj(w));
				}
			}
		}

		if (ix == 0 || (ix + 1) % reportEvery == 0 || ix + 1 == nx)
		{
			const int completed = ix + 1;
			const int percent = static_cast<int>(std::round(100.0 * completed / std::max(nx, 1)));
			const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
			const double perStep = elapsed / static_cast<double>(completed);
			const double remaining = perStep * static_cast<double>(nx - completed);
			Report(cfg,
			       std::string(L_PROG_MannProgress) + std::to_string(percent) + "%" + L_PROG_Elapsed +
			           FormatDuration(elapsed) + L_PROG_ETA + FormatDuration(remaining) + ".");
		}
	}

	Report(cfg, L_PROG_Mann3DIFFT);
	uHat.ExecuteBackward();
	vHat.ExecuteBackward();
	wHat.ExecuteBackward();

	WindField field = AllocateField(cfg);
	const double invN = 1.0 / (static_cast<double>(nx) * ny * nz);
	const double yMin = -0.5 * cfg.gridWidth;
	const double zMin = cfg.zBottom;
	for (int iz = 0; iz < cfg.nz; ++iz)
	{
		const int miz = NearestMannIndex(cfg.zCoords[static_cast<std::size_t>(iz)], zMin, cfg.gridHeight, nz);
		for (int iy = 0; iy < cfg.ny; ++iy)
		{
			const int miy = NearestMannIndex(cfg.yCoords[static_cast<std::size_t>(iy)], yMin, cfg.gridWidth, ny);
			const int point = GridIndex(cfg, iz, iy);
			for (int t = 0; t < cfg.nSteps; ++t)
			{
				const int mix = t % nx;
				field.At(0, t, point) = invN * uHat.Real(mix, miy, miz);
				field.At(1, t, point) = invN * vHat.Real(mix, miy, miz);
				field.At(2, t, point) = invN * wHat.Real(mix, miy, miz);
			}
		}
	}

	for (int comp = 0; comp < 3; ++comp)
	{
		if (!ComponentEnabled(cfg.input, comp))
		{
			std::fill(field.component[static_cast<std::size_t>(comp)].begin(),
			          field.component[static_cast<std::size_t>(comp)].end(),
			          0.0);
			continue;
		}
		ApplyScaleIecForComponent(cfg, field, comp);
	}

	return field;
}

/** @brief 轮毂高度处三个湍流速度分量的零滞后二阶协方差矩阵（对称，不含 vv，因其可由 uu, ww, sigma 导出） */
struct HubCovariances
{
	double uu = 0.0;  ///< u 分量的方差 Var(u) [m²/s²]
	double uv = 0.0;  ///< u-v 协方差 Cov(u, v) [m²/s²]
	double uw = 0.0;  ///< u-w 协方差 Cov(u, w) [m²/s²]
	double vw = 0.0;  ///< v-w 协方差 Cov(v, w) [m²/s²]
	double ww = 0.0;  ///< w 分量的方差 Var(w) [m²/s²]
};

/**
 * @brief 计算轮毂高度点三个速度分量的二阶协方差（uu, uv, uw, vw, ww）
 * @param cfg   仿真配置
 * @param field 已生成的湍流风场
 * @return      HubCovariances结构体，含5个协方差分量（vv可由对称性隐含）
 * @note        定位HubPointIndex后计算该点时间序列的零滞后协方差，除以nSteps归一化。
 * @code auto cov = ComputeHubCovariances(cfg, field); @endcode
 */
HubCovariances ComputeHubCovariances(const SimWindConfig &cfg, const WindField &field)
{
	HubCovariances cov;
	if (cfg.nSteps <= 0 || cfg.nPoints <= 0)
		return cov;

	const int point = HubPointIndex(cfg);
	for (int t = 0; t < cfg.nSteps; ++t)
	{
		const double u = field.At(0, t, point);
		const double v = field.At(1, t, point);
		const double w = field.At(2, t, point);
		cov.uu += u * u;
		cov.uv += u * v;
		cov.uw += u * w;
		cov.vw += v * w;
		cov.ww += w * w;
	}

	const double inv = 1.0 / static_cast<double>(cfg.nSteps);
	cov.uu *= inv;
	cov.uv *= inv;
	cov.uw *= inv;
	cov.vw *= inv;
	cov.ww *= inv;
	return cov;
}

/**
 * @brief 对固定3×3矩阵进行带jitter的Cholesky分解 L·Lᵀ
 * @param matrix 输入对称矩阵（Matrix3 = 3×3 std::array<std::array<double,3>,3>）
 * @param n      矩阵阶数（≤3，仅前n×n子块被分解）
 * @param lower  输出的下三角矩阵L（原地初始化为零）
 * @return       分解成功返回true，8次jitter尝试失败返回false
 * @note         jitter从trace·1e-12起步，每次成10倍递增，失败时对角元+=jitter再试。
 * @code Matrix3 lower; bool ok = CholeskyLower(covMatrix, dim, lower); @endcode
 */
bool CholeskyLower(const Matrix3 &matrix, int n, Matrix3 &lower)
{
	lower = ZeroMatrix3();
	const double trace = matrix[0][0] + matrix[1][1] + matrix[2][2];
	const double baseJitter = std::max(std::abs(trace) * 1.0e-12, 1.0e-12);

	for (int attempt = 0; attempt < 8; ++attempt)
	{
		const double jitter = baseJitter * std::pow(10.0, attempt);
		bool ok = true;
		for (int i = 0; i < n && ok; ++i)
		{
			for (int j = 0; j <= i; ++j)
			{
				double sum = matrix[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
				if (i == j)
					sum += jitter;
				for (int k = 0; k < j; ++k)
					sum -= lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] *
					       lower[static_cast<std::size_t>(j)][static_cast<std::size_t>(k)];
				if (i == j)
				{
					if (sum <= 0.0)
					{
						ok = false;
						break;
					}
					lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = std::sqrt(sum);
				}
				else
				{
					const double pivot = lower[static_cast<std::size_t>(j)][static_cast<std::size_t>(j)];
					if (std::abs(pivot) <= kTiny)
					{
						ok = false;
						break;
					}
					lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = sum / pivot;
				}
			}
		}
		if (ok)
			return true;
	}

	return false;
}

/**
 * @brief 两个Matrix3矩阵相乘 C = A·B
 * @param a 左乘矩阵A
 * @param b 右乘矩阵B
 * @param n 矩阵阶数（≤3）
 * @return  乘积矩阵C
 * @code Matrix3 transform = MultiplyMatrix(targetLower, invCurrentLower, dim); @endcode
 */
Matrix3 MultiplyMatrix(const Matrix3 &a, const Matrix3 &b, int n)
{
	Matrix3 out = ZeroMatrix3();
	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j < n; ++j)
		{
			double sum = 0.0;
			for (int k = 0; k < n; ++k)
				sum += a[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] *
				       b[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)];
			out[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = sum;
		}
	}
	return out;
}

/**
 * @brief 计算下三角矩阵的逆矩阵（前代法，仍为下三角）
 * @param lower 下三角矩阵L
 * @param n     矩阵阶数（≤3）
 * @return      L⁻¹（下三角）
 * @note        对角元取倒数，非对角元通过前代求解递推。
 * @code Matrix3 invL = InvertLowerTriangular(currentLower, dim); @endcode
 */
Matrix3 InvertLowerTriangular(const Matrix3 &lower, int n)
{
	Matrix3 inv = ZeroMatrix3();
	for (int i = 0; i < n; ++i)
	{
		inv[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] =
		    1.0 / std::max(lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)], kTiny);
		for (int j = 0; j < i; ++j)
		{
			double sum = 0.0;
			for (int k = j; k < i; ++k)
				sum += lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] *
				       inv[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)];
			inv[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
			    -sum / std::max(lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)], kTiny);
		}
	}
	return inv;
}

/**
 * @brief 对非正定的协方差矩阵做二分搜索"软化"（收缩非对角元），直到可Cholesky分解
 * @param target 输入/输出的对称矩阵（原地修改）
 * @param n      矩阵阶数（≤3）
 * @return       找到可行的软化系数返回true，否则返回false
 * @note         在[0,1]区间二分搜索收缩系数scale，将非对角元乘以scale后尝试Cholesky分解。
 *               目标是不能正定分解时逐步收缩协方差，同时保留对角元素（方差）不变。
 * @code if (!CholeskyLower(target, dim, targetLower))
 *          SoftenedTargetCovariance(target, dim); @endcode
 */
bool SoftenedTargetCovariance(Matrix3 &target, int n)
{
	const Matrix3 original = target;
	double lo = 0.0;
	double hi = 1.0;
	Matrix3 best = target;
	bool found = false;
	for (int iter = 0; iter < 32; ++iter)
	{
		const double scale = 0.5 * (lo + hi);
		Matrix3 trial = original;
		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < i; ++j)
			{
				trial[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] *= scale;
				trial[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] =
				    trial[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
			}
		}
		Matrix3 lower{};
		if (CholeskyLower(trial, n, lower))
		{
			best = trial;
			lo = scale;
			found = true;
		}
		else
		{
			hi = scale;
		}
	}
	if (found)
		target = best;
	return found;
}

/**
 * @brief 对已生成的湍流风场施加雷诺应力缩放，使各点协方差矩阵匹配目标应力张量
 * @param cfg   仿真配置，含reynoldsStress目标、scaleIEC策略及各组分σ
 * @param field 风场数据（原地修改），对每个空间点独立变换
 * @note        流程：(1)计算每个点的当前协方差矩阵（仅启用分量）；
 *              (2)Cholesky分解当前矩阵得L_cur；(3)构造目标协方差矩阵（对角=目标σ²，
 *              非对角取自reynoldsStress配置项或保留当前值）；
 *              (4)Cholesky分解目标矩阵得L_tgt（失败则软化重试）；
 *              (5)变换矩阵T=L_tgt·L_cur⁻¹；(6)对每个时间步应用mapped=T·(v-mean)。
 *              跳过不可分解的点并累积警告。软化时非对角元收缩而保留对角σ。
 * @code ApplyReynoldsStressScaling(cfg, field); @endcode
 */
void ApplyReynoldsStressScaling(const SimWindConfig &cfg, WindField &field)
{
	if (!cfg.met.reynoldsStress.active)
		return;

	bool anySoftened = false;
	bool anySkipped = false;

	for (int point = 0; point < cfg.nPoints; ++point)
	{
		std::array<double, 3> mean{0.0, 0.0, 0.0};
		for (int comp = 0; comp < 3; ++comp)
		{
			if (!ComponentEnabled(cfg.input, comp))
				continue;
			for (int t = 0; t < cfg.nSteps; ++t)
				mean[static_cast<std::size_t>(comp)] += field.At(comp, t, point);
			mean[static_cast<std::size_t>(comp)] /= static_cast<double>(cfg.nSteps);
		}

		std::vector<int> active;
		active.reserve(3);
		for (int comp = 0; comp < 3; ++comp)
		{
			if (ComponentEnabled(cfg.input, comp))
				active.push_back(comp);
		}
		if (active.empty())
			continue;

		const int dim = static_cast<int>(active.size());
		Matrix3 current = ZeroMatrix3();
		for (int t = 0; t < cfg.nSteps; ++t)
		{
			std::array<double, 3> sample{0.0, 0.0, 0.0};
			for (int i = 0; i < dim; ++i)
			{
				const int comp = active[static_cast<std::size_t>(i)];
				sample[static_cast<std::size_t>(i)] = field.At(comp, t, point) - mean[static_cast<std::size_t>(comp)];
			}
			for (int i = 0; i < dim; ++i)
			{
				for (int j = 0; j <= i; ++j)
				{
					current[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] +=
					    sample[static_cast<std::size_t>(i)] * sample[static_cast<std::size_t>(j)];
				}
			}
		}
		for (int i = 0; i < dim; ++i)
		{
			for (int j = 0; j <= i; ++j)
			{
				current[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] /= static_cast<double>(cfg.nSteps);
				current[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] =
				    current[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
			}
		}

		Matrix3 currentLower{};
		if (!CholeskyLower(current, dim, currentLower))
		{
			anySkipped = true;
			continue;
		}

		const int iz = point / cfg.ny;
		Matrix3 target = ZeroMatrix3();
		for (int i = 0; i < dim; ++i)
		{
			const int compI = active[static_cast<std::size_t>(i)];
			const double sigmaTarget = cfg.scaleIEC >= 1
			                               ? LocalSigmaAtZ(cfg, compI, iz)
			                               : std::sqrt(std::max(current[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)], 0.0));
			target[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] = sigmaTarget * sigmaTarget;
			for (int j = 0; j < i; ++j)
			{
				const int compJ = active[static_cast<std::size_t>(j)];
				double offdiag = current[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
				if (HasTargetReynoldsStress(cfg.met.reynoldsStress, compI, compJ))
					offdiag = TargetReynoldsStress(cfg.met.reynoldsStress, compI, compJ);
				const double sigmaI = std::sqrt(std::max(target[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)], 0.0));
				const double sigmaJ = std::sqrt(std::max(target[static_cast<std::size_t>(j)][static_cast<std::size_t>(j)], 0.0));
				const double maxCov = 0.995 * sigmaI * sigmaJ;
				offdiag = std::clamp(offdiag, -maxCov, maxCov);
				target[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = offdiag;
				target[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] = offdiag;
			}
		}

		Matrix3 targetLower{};
		if (!CholeskyLower(target, dim, targetLower))
		{
			if (!SoftenedTargetCovariance(target, dim) || !CholeskyLower(target, dim, targetLower))
			{
				anySkipped = true;
				continue;
			}
			anySoftened = true;
		}

		const Matrix3 invCurrentLower = InvertLowerTriangular(currentLower, dim);
		const Matrix3 transform = MultiplyMatrix(targetLower, invCurrentLower, dim);

		for (int t = 0; t < cfg.nSteps; ++t)
		{
			std::array<double, 3> centered{0.0, 0.0, 0.0};
			std::array<double, 3> mapped{0.0, 0.0, 0.0};
			for (int i = 0; i < dim; ++i)
			{
				const int comp = active[static_cast<std::size_t>(i)];
				centered[static_cast<std::size_t>(i)] = field.At(comp, t, point) - mean[static_cast<std::size_t>(comp)];
			}
			for (int i = 0; i < dim; ++i)
			{
				for (int j = 0; j < dim; ++j)
				{
					mapped[static_cast<std::size_t>(i)] +=
					    transform[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] *
					    centered[static_cast<std::size_t>(j)];
				}
			}
			for (int i = 0; i < dim; ++i)
			{
				const int comp = active[static_cast<std::size_t>(i)];
				field.At(comp, t, point) = mapped[static_cast<std::size_t>(i)];
			}
		}
	}

	if (anySoftened)
	{
		AppendWarning(MutableWarnings(cfg), L_WARN_ReynoldsSoften);
	}
	if (anySkipped)
	{
		AppendWarning(MutableWarnings(cfg), L_WARN_ReynoldsSkipSingular);
	}
}

/**
 * @brief 对湍流风场叠加平均风廓线和IEC瞬态事件（EOG/EDC/ECD/EWS）
 * @param cfg   仿真配置，含windModel、角度、事件参数
 * @param field 风场数据（原地修改）：确定性模型直接赋值，湍流模型叠加到现有值
 * @note        两类处理：(1)确定性/均匀风：每个时间步用meanU重建全场速度；
 *              (2)湍流叠加：现有湍流速度+=平均风向分量分解（cosθ·cosφ, sinθ, sinφ）。
 *              事件支持EOG（极端运行阵风）、EDC（极端方向变化）、ECD（极端相干阵风+方向变化）、
 *              EWS（极端风切变）。均匀风/非确定性事件提前返回。
 * @code ApplyMeanAndEvents(cfg, field); @endcode
 */
void ApplyMeanAndEvents(const SimWindConfig &cfg, WindField &field)
{
	const double hAngleBase = cfg.input.horAngle * kRad;
	const double vAngle = cfg.input.vertAngle * kRad;
	const bool deterministicEvent = IsDeterministicEventWindModel(cfg.input.windModel);
	const bool uniformWind = IsUniformWindModel(cfg.input.windModel);

	for (int t = 0; t < cfg.nSteps; ++t)
	{
		for (int p = 0; p < cfg.nPoints; ++p)
		{
			const double meanU = cfg.meanU[static_cast<std::size_t>(p)];
			const int iz = p / cfg.ny;
			const double localHAngle = hAngleBase +
			                           (cfg.hasUserDirectionProfile &&
			                                    static_cast<std::size_t>(iz) < cfg.directionByZ.size()
			                                ? cfg.directionByZ[static_cast<std::size_t>(iz)] * kRad
			                                : 0.0);
			if (deterministicEvent || uniformWind)
			{
				field.At(0, t, p) = meanU * std::cos(localHAngle) * std::cos(vAngle);
				field.At(1, t, p) = meanU * std::sin(localHAngle);
				field.At(2, t, p) = meanU * std::sin(vAngle);
			}
			else
			{
				field.At(0, t, p) += meanU * std::cos(localHAngle) * std::cos(vAngle);
				field.At(1, t, p) += meanU * std::sin(localHAngle);
				field.At(2, t, p) += meanU * std::sin(vAngle);
			}
		}
	}

	if (uniformWind || !deterministicEvent)
		return;

	const double period = ClampPositive(cfg.input.gustPeriod, std::min(10.5, cfg.duration));
	const double start = cfg.input.eventStart > 0.0 ? cfg.input.eventStart : std::max(0.0, 0.5 * (cfg.duration - period));
	const double sign = cfg.input.eventSign == EventSign::NEGATIVE ? -1.0 : 1.0;
	const double sigma1 = ReferenceSigma1Ntm(cfg);
	const double rotorDiameter = std::max(cfg.effectiveRotorDiameter, kTiny);
	const double gustAmplitude = ExtremeOperatingGustAmplitude(cfg);
	const double thetaE = ExtremeDirectionChangeDegrees(cfg) * kRad;
	const double vCog = cfg.input.ecdVcog > 0.0 ? cfg.input.ecdVcog : 15.0;
	const double thetaCg = (cfg.uHub < 4.0 ? 180.0 : 720.0 / std::max(cfg.uHub, kTiny)) * kRad * sign;
	const double shearDenom = 1.0 + 0.1 * rotorDiameter / std::max(cfg.lambda, kTiny);

	for (int t = 0; t < cfg.nSteps; ++t)
	{
		const double time = t * cfg.dt;
		const double localTime = time - start;
		const bool inEvent = localTime >= 0.0 && localTime <= period;

		for (int p = 0; p < cfg.nPoints; ++p)
		{
			const double meanU = cfg.meanU[static_cast<std::size_t>(p)];
			const double y = cfg.y[static_cast<std::size_t>(p)];
			const double z = cfg.z[static_cast<std::size_t>(p)];

			if (cfg.input.windModel == WindModel::EOG)
			{
				double dU = 0.0;
				if (inEvent)
				{
					dU = -sign * 0.37 * gustAmplitude *
					     std::sin(3.0 * kPi * localTime / period) *
					     (1.0 - std::cos(2.0 * kPi * localTime / period));
				}
				field.At(0, t, p) = meanU + dU;
				field.At(1, t, p) = 0.0;
				field.At(2, t, p) = 0.0;
			}
			else if (cfg.input.windModel == WindModel::EDC)
			{
				double theta = 0.0;
				if (inEvent)
					theta = 0.5 * thetaE * sign * (1.0 - std::cos(kPi * localTime / period));
				else if (localTime > period)
					theta = thetaE * sign;

				field.At(0, t, p) = meanU * std::cos(theta);
				field.At(1, t, p) = meanU * std::sin(theta);
				field.At(2, t, p) = 0.0;
			}
			else if (cfg.input.windModel == WindModel::ECD)
			{
				double dU = 0.0;
				double theta = 0.0;
				if (inEvent)
				{
					dU = 0.5 * vCog * (1.0 - std::cos(kPi * localTime / period));
					theta = 0.5 * thetaCg * (1.0 - std::cos(kPi * localTime / period));
				}
				else if (localTime > period)
				{
					dU = vCog;
					theta = thetaCg;
				}

				const double totalU = meanU + dU;
				field.At(0, t, p) = totalU * std::cos(theta);
				field.At(1, t, p) = totalU * std::sin(theta);
				field.At(2, t, p) = 0.0;
			}
			else if (cfg.input.windModel == WindModel::EWS)
			{
				double dU = 0.0;
				if (inEvent)
				{
					const double envelope = 0.5 * (1.0 - std::cos(2.0 * kPi * localTime / period));
					const double vertShear = sign * 2.0 * sigma1 / shearDenom *
					                         (z - cfg.hubHeight) / rotorDiameter * envelope;
					const double latShear = sign * 2.0 * sigma1 / shearDenom *
					                        y / rotorDiameter * envelope;
					dU = vertShear + latShear;
				}
				field.At(0, t, p) = meanU + dU;
				field.At(1, t, p) = 0.0;
				field.At(2, t, p) = 0.0;
			}
		}
	}
}

/**
 * @brief 顶层风场生成调度器：根据湍流/风模型选择生成路径
 * @param cfg 仿真配置
 * @return    已填充并缩放的WindField
 * @note      路径：(1)USER_WIND_SPEED→LoadUserWindSpeedField；
 *            (2)UNIFORM/确定性事件→分配空场后直接ApplyMeanAndEvents；
 *            (3)Mann湍流→GenerateMannWindField+可选Reynolds应力缩放+ApplyMeanAndEvents；
 *            (4)其他谱模型→GenerateSpectralComponent×3+可选Reynolds应力缩放+ApplyMeanAndEvents。
 * @code WindField field = GenerateWindField(cfg); @endcode
 */
WindField GenerateWindField(const SimWindConfig &cfg)
{
	if (cfg.input.turbModel == TurbModel::USER_WIND_SPEED)
	{
		Report(cfg, L_PROG_LoadingUserWind);
		return LoadUserWindSpeedField(cfg);
	}

	if (IsUniformWindModel(cfg.input.windModel))
	{
		Report(cfg, L_PROG_UniformWindOnly);
		WindField field = AllocateField(cfg);
		Report(cfg, L_PROG_ApplyMeanProfile);
		ApplyMeanAndEvents(cfg, field);
		return field;
	}

	if (IsDeterministicEventWindModel(cfg.input.windModel))
	{
		Report(cfg, L_PROG_DeterministicEvent);
		WindField field = AllocateField(cfg);
		Report(cfg, L_PROG_ApplyMeanProfile);
		ApplyMeanAndEvents(cfg, field);
		return field;
	}

	if (IsMann(cfg.input.turbModel))
	{
		WindField field = GenerateMannWindField(cfg);
		Report(cfg, L_PROG_GenMannSeries);
		if (cfg.met.reynoldsStress.active)
		{
			Report(cfg, L_PROG_ApplyReynolds);
			ApplyReynoldsStressScaling(cfg, field);
		}
		Report(cfg, L_PROG_ApplyMeanProfile);
		ApplyMeanAndEvents(cfg, field);
		return field;
	}

	Report(cfg, L_PROG_CalcSpectral);
	WindField field = AllocateField(cfg);
	std::mt19937_64 rng(static_cast<std::uint64_t>(cfg.input.turbSeed));
	for (int comp = 0; comp < 3; ++comp)
		GenerateSpectralComponent(cfg, comp, rng, field);
	if (cfg.met.reynoldsStress.active)
	{
		Report(cfg, L_PROG_ApplyReynolds);
		ApplyReynoldsStressScaling(cfg, field);
	}
	Report(cfg, L_PROG_GenTimeSeries);
	Report(cfg, L_PROG_ApplyMeanProfile);
	ApplyMeanAndEvents(cfg, field);
	return field;
}

/**
 * @brief 计算单个分量时间序列的均值、标准差和湍流强度
 * @param values 数据向量
 * @param uHub   轮毂高度平均风速 [m/s]，用于计算TI=σ/uHub
 * @return       含mean, sigma, turbulenceIntensity的统计结构体
 * @code auto stats = ComputeStats(field.component[0], cfg.uHub); @endcode
 */
SimWindComponentStats ComputeStats(const std::vector<double> &values, double uHub)
{
	SimWindComponentStats stats;
	if (values.empty())
		return stats;

	stats.mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
	double sum2 = 0.0;
	for (double value : values)
		sum2 += (value - stats.mean) * (value - stats.mean);
	stats.sigma = std::sqrt(sum2 / static_cast<double>(values.size()));
	stats.turbulenceIntensity = uHub > 0.0 ? stats.sigma / uHub : 0.0;
	return stats;
}

/**
 * @brief 计算三个速度分量的完整场统计
 * @param cfg   仿真配置，提供uHub
 * @param field 已生成的风场
 * @return      三个SimWindComponentStats的数组 [u, v, w]
 * @code auto stats = ComputeFieldStats(cfg, field); @endcode
 */
std::array<SimWindComponentStats, 3> ComputeFieldStats(const SimWindConfig &cfg, const WindField &field)
{
	std::array<SimWindComponentStats, 3> stats{};
	for (int comp = 0; comp < 3; ++comp)
		stats[static_cast<std::size_t>(comp)] = ComputeStats(field.component[static_cast<std::size_t>(comp)], cfg.uHub);
	return stats;
}

/**
 * @brief 计算int16编码的缩放因子和偏移量（将double映射到[-32768,32767]）
 * @param values 数据向量
 * @return       pair{slope, offset}：编码值 = slope × value + offset
 * @note         斜率 = 65535/(max-min)；偏移 = -32768 - slope·min。
 *               当数据无变化或为空时返回{1.0, -value}。
 * @code auto [scl, off] = ScalingForComponent(field.component[0]); @endcode
 */
std::pair<float, float> ScalingForComponent(const std::vector<double> &values)
{
	const auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
	if (minIt == values.end() || std::abs(*maxIt - *minIt) <= kTiny)
	{
		const double value = minIt == values.end() ? 0.0 : *minIt;
		return {1.0f, static_cast<float>(-value)};
	}
	const double slope = 65535.0 / (*maxIt - *minIt);
	const double offset = -32768.0 - slope * (*minIt);
	return {static_cast<float>(slope), static_cast<float>(offset)};
}

/**
 * @brief 将double值钳位到[-32768,32767]并四舍五入为int16
 * @param value 待编码值
 * @return      int16编码结果
 * @code auto i16 = EncodeInt16(slope * field.At(0,t,p) + offset); @endcode
 */
std::int16_t EncodeInt16(double value)
{
	const long rounded = std::lround(std::clamp(value, -32768.0, 32767.0));
	return static_cast<std::int16_t>(rounded);
}

/**
 * @brief 将内部湍流模型枚举映射为Bladed .wnd文件中的模型ID
 * @param model 湍流模型枚举值
 * @return      Bladed模型ID（3=VK, 4=IVK, 5=Kaimal, 7=BKal, 8=BMann, 默认4）
 * @code int modelId = BladedModelId(cfg.input.turbModel); @endcode
 */
int BladedModelId(TurbModel model)
{
	switch (model)
	{
	case TurbModel::IEC_KAIMAL: return 5;
	case TurbModel::IEC_VKAIMAL: return 3;
	case TurbModel::B_MANN: return 8;
	case TurbModel::B_KAL: return 7;
	case TurbModel::B_VKAL: return 3;
	case TurbModel::B_IVKAL: return 4;
	case TurbModel::USRVKM: return 3;
	default: return 4;
	}
}

/**
 * @brief 返回正值尺度参数，零或负值时使用回退值
 * @param value    用户指定的尺度值
 * @param fallback 默认回退值
 * @return         value>0 ? value : max(fallback, kTiny)
 * @code float xLu = LengthScaleOrDefault(cfg.input.vkLu, cfg.integralScale[0]); @endcode
 */
double LengthScaleOrDefault(double value, double fallback)
{
	return value > 0.0 ? value : std::max(fallback, kTiny);
}

/**
 * @brief 计算Bladed格式的湍流强度百分比
 * @param stats 三分量统计
 * @param cfg   仿真配置，提供uHub
 * @param comp  分量索引（0/1/2）
 * @return      TI百分比 = 100·σ/uHub [%]
 * @code float ti = BladedTiPercent(stats, cfg, 0); @endcode
 */
float BladedTiPercent(const std::array<SimWindComponentStats, 3> &stats, const SimWindConfig &cfg, int comp)
{
	const double ti = std::max(stats[static_cast<std::size_t>(comp)].sigma / std::max(cfg.uHub, kTiny), 1.0e-6);
	return static_cast<float>(100.0 * ti);
}

/**
 * @brief 输出TurbSim .bts二进制全场风文件
 * @param cfg   仿真配置，提供网格参数和hubHeight
 * @param field 已生成的风场（三分量时间序列）
 * @param path  输出文件路径
 * @throw       无法打开文件时抛出runtime_error
 * @note        格式：int16标记(7=非循环/8=循环) → 网格和时间参数 → 每分量int16编码和偏移 →
 *              按(时间, z, y)循环写入三个int16分量（uScl*u+uOff等）。
 * @code WriteBts(cfg, field, "output.bts"); @endcode
 */
void WriteBts(const SimWindConfig &cfg, const WindField &field, const std::filesystem::path &path)
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
		throw std::runtime_error(std::string(L_WIND_CannotOpenBTS) + ": " + path.string());

	const auto [uScl, uOff] = ScalingForComponent(field.component[0]);
	const auto [vScl, vOff] = ScalingForComponent(field.component[1]);
	const auto [wScl, wOff] = ScalingForComponent(field.component[2]);
	const std::string desc = "Qahse WindL SimWind";

	WriteScalar<std::int16_t>(out, static_cast<std::int16_t>(cfg.input.cycleWind ? 8 : 7));
	WriteScalar<std::int32_t>(out, cfg.nz);
	WriteScalar<std::int32_t>(out, cfg.ny);
	WriteScalar<std::int32_t>(out, 0);
	WriteScalar<std::int32_t>(out, cfg.nSteps);
	WriteScalar<float>(out, static_cast<float>(cfg.dz));
	WriteScalar<float>(out, static_cast<float>(cfg.dy));
	WriteScalar<float>(out, static_cast<float>(cfg.dt));
	WriteScalar<float>(out, static_cast<float>(cfg.uHub));
	WriteScalar<float>(out, static_cast<float>(cfg.hubHeight));
	WriteScalar<float>(out, static_cast<float>(cfg.zBottom));
	WriteScalar<float>(out, uScl);
	WriteScalar<float>(out, uOff);
	WriteScalar<float>(out, vScl);
	WriteScalar<float>(out, vOff);
	WriteScalar<float>(out, wScl);
	WriteScalar<float>(out, wOff);
	WriteScalar<std::int32_t>(out, static_cast<std::int32_t>(desc.size()));
	out.write(desc.data(), static_cast<std::streamsize>(desc.size()));

	for (int t = 0; t < cfg.nSteps; ++t)
	{
		for (int iz = 0; iz < cfg.nz; ++iz)
		{
			for (int iy = 0; iy < cfg.ny; ++iy)
			{
				const int p = GridIndex(cfg, iz, iy);
				WriteScalar<std::int16_t>(out, EncodeInt16(uScl * field.At(0, t, p) + uOff));
				WriteScalar<std::int16_t>(out, EncodeInt16(vScl * field.At(1, t, p) + vOff));
				WriteScalar<std::int16_t>(out, EncodeInt16(wScl * field.At(2, t, p) + wOff));
			}
		}
	}
}

/**
 * @brief 输出Bladed .wnd二进制全场风文件
 * @param cfg   仿真配置，含模型ID、尺度参数、输出参数
 * @param field 已生成的风场
 * @param stats 三分量统计（用于归一化编码）
 * @param path  输出文件路径
 * @throw       无法打开文件时抛出runtime_error
 * @note        根据modelId不同写不同的头部：(4)IVK含TI百分比；(7)BKal含coh参数；(8)BMann含
 *              Mann参数和sigma比值；modelId≥7时写变长头部。数据按(时间,z,-y)循环，
 *              每个分量编码为 1000·(v-mean)/σ 的int16。
 * @code WriteBladedWnd(cfg, field, stats, "output.wnd"); @endcode
 */
void WriteBladedWnd(const SimWindConfig &cfg,
                    const WindField &field,
                    const std::array<SimWindComponentStats, 3> &stats,
                    const std::filesystem::path &path)
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
		throw std::runtime_error(std::string(L_WIND_CannotOpenBlndWND) + ": " + path.string());

	const int modelId = BladedModelId(cfg.input.turbModel);
	const int componentCount = 3;

	const float zLu = static_cast<float>(LengthScaleOrDefault(cfg.input.vzLu, cfg.verticalScale[0]));
	const float yLu = static_cast<float>(LengthScaleOrDefault(cfg.input.vyLu, cfg.lateralScale[0]));
	const float xLu = static_cast<float>(LengthScaleOrDefault(cfg.input.vkLu, cfg.integralScale[0]));
	const float zLv = static_cast<float>(LengthScaleOrDefault(cfg.input.vzLv, cfg.verticalScale[1]));
	const float yLv = static_cast<float>(LengthScaleOrDefault(cfg.input.vyLv, cfg.lateralScale[1]));
	const float xLv = static_cast<float>(LengthScaleOrDefault(cfg.input.vkLv, cfg.integralScale[1]));
	const float zLw = static_cast<float>(LengthScaleOrDefault(cfg.input.vzLw, cfg.verticalScale[2]));
	const float yLw = static_cast<float>(LengthScaleOrDefault(cfg.input.vyLw, cfg.lateralScale[2]));
	const float xLw = static_cast<float>(LengthScaleOrDefault(cfg.input.vkLw, cfg.integralScale[2]));
	const float maxFreq = static_cast<float>(0.5 / cfg.dt);
	const double cohScale = cfg.cohB[0] > 0.0 ? 1.0 / cfg.cohB[0] : cfg.lc;

	WriteScalar<std::int16_t>(out, static_cast<std::int16_t>(-99));
	WriteScalar<std::int16_t>(out, static_cast<std::int16_t>(modelId));

	std::streampos headerOffsetPos = std::streampos(-1);
	if (modelId >= 7)
	{
		headerOffsetPos = out.tellp();
		WriteScalar<std::int32_t>(out, static_cast<std::int32_t>(0));
		WriteScalar<std::int32_t>(out, static_cast<std::int32_t>(componentCount));
	}

	if (modelId == 4)
	{
		WriteScalar<std::int32_t>(out, static_cast<std::int32_t>(componentCount));
		WriteScalar<float>(out, static_cast<float>(cfg.input.latitude));
		WriteScalar<float>(out, static_cast<float>(cfg.input.roughness));
		WriteScalar<float>(out, static_cast<float>(cfg.refHeight));
		WriteScalar<float>(out, BladedTiPercent(stats, cfg, 0));
		WriteScalar<float>(out, BladedTiPercent(stats, cfg, 1));
		WriteScalar<float>(out, BladedTiPercent(stats, cfg, 2));
	}

	WriteScalar<float>(out, static_cast<float>(cfg.dz));
	WriteScalar<float>(out, static_cast<float>(cfg.dy));
	WriteScalar<float>(out, static_cast<float>(cfg.dx));
	WriteScalar<std::int32_t>(out, cfg.nSteps / 2);
	WriteScalar<float>(out, static_cast<float>(cfg.uHub));
	WriteScalar<float>(out, zLu);
	WriteScalar<float>(out, yLu);
	WriteScalar<float>(out, xLu);
	WriteScalar<float>(out, maxFreq);
	WriteScalar<std::int32_t>(out, cfg.input.turbSeed);
	WriteScalar<std::int32_t>(out, cfg.nz);
	WriteScalar<std::int32_t>(out, cfg.ny);

	WriteScalar<float>(out, zLv);
	WriteScalar<float>(out, yLv);
	WriteScalar<float>(out, xLv);
	WriteScalar<float>(out, zLw);
	WriteScalar<float>(out, yLw);
	WriteScalar<float>(out, xLw);

	if (modelId == 7)
	{
		WriteScalar<float>(out, static_cast<float>(cfg.cohDecay[0]));
		WriteScalar<float>(out, static_cast<float>(cohScale));
	}
	else if (modelId == 8)
	{
		WriteScalar<float>(out, static_cast<float>(cfg.mannGamma));
		WriteScalar<float>(out, static_cast<float>(cfg.mannLength));
		WriteScalar<float>(out, static_cast<float>(std::max(stats[1].sigma, kTiny) / std::max(stats[0].sigma, kTiny)));
		WriteScalar<float>(out, static_cast<float>(std::max(stats[2].sigma, kTiny) / std::max(stats[0].sigma, kTiny)));
		WriteScalar<float>(out, static_cast<float>(cfg.mannMaxL > 0.0 ? cfg.mannMaxL : 8.0 * std::max(cfg.mannLength, 0.0)));
		WriteScalar<float>(out, 0.0f);
		WriteScalar<float>(out, 0.0f);
		WriteScalar<std::int32_t>(out, 0);
		WriteScalar<std::int32_t>(out, cfg.nSteps);
		WriteScalar<std::int32_t>(out, 0);
		WriteScalar<std::int32_t>(out, 0);
		WriteScalar<std::int32_t>(out, 0);
		WriteScalar<float>(out, 0.0f);
		WriteScalar<float>(out, 0.0f);
	}

	if (modelId >= 7)
	{
		const std::streampos endPos = out.tellp();
		const auto headerBytes = static_cast<std::int32_t>(endPos - headerOffsetPos - static_cast<std::streamoff>(4));
		out.seekp(headerOffsetPos);
		WriteScalar<std::int32_t>(out, headerBytes);
		out.seekp(endPos);
	}

	const std::array<double, 3> means{stats[0].mean, stats[1].mean, stats[2].mean};
	const std::array<double, 3> sigmas{
	    stats[0].sigma > kTiny ? stats[0].sigma : 1.0,
	    stats[1].sigma > kTiny ? stats[1].sigma : 1.0,
	    stats[2].sigma > kTiny ? stats[2].sigma : 1.0};

	for (int t = 0; t < cfg.nSteps; ++t)
	{
		for (int iz = 0; iz < cfg.nz; ++iz)
		{
			for (int iy = cfg.ny - 1; iy >= 0; --iy)
			{
				const int p = GridIndex(cfg, iz, iy);
				for (int comp = 0; comp < componentCount; ++comp)
				{
					const double normalized = 1000.0 * (field.At(comp, t, p) - means[static_cast<std::size_t>(comp)]) /
					                          sigmas[static_cast<std::size_t>(comp)];
					WriteScalar<std::int16_t>(out, EncodeInt16(normalized));
				}
			}
		}
	}
}

/**
 * @brief 输出TurbSim兼容的Bladed格式.wnd文件
 * @param cfg   仿真配置
 * @param field 已生成的风场
 * @param stats 三分量统计
 * @param path  输出文件路径
 * @throw       无法打开文件时抛出runtime_error
 * @note        固定modelId=4（IVK）。编码方式：u分量 = 1000/(U·TIu)·u - 1000/TIu，
 *              v/w分量 = 1000/(U·TI)·v。按(时间,z,y)循环写入三个int16。
 * @code WriteTurbSimWnd(cfg, field, stats, "output.wnd"); @endcode
 */
void WriteTurbSimWnd(const SimWindConfig &cfg,
                     const WindField &field,
                     const std::array<SimWindComponentStats, 3> &stats,
                     const std::filesystem::path &path)
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
		throw std::runtime_error(std::string(L_WIND_CannotOpenTSWND) + ": " + path.string());

	double maxUDev = 0.0;
	for (double value : field.component[0])
		maxUDev = std::max(maxUDev, std::abs(value - cfg.uHub));
	const double uSig = std::max(stats[0].sigma, 0.05 * maxUDev);
	const double tiU = std::max(uSig / cfg.uHub, 1.0e-6);
	const double tiV = std::max(stats[1].sigma / cfg.uHub, 1.0e-6);
	const double tiW = std::max(stats[2].sigma / cfg.uHub, 1.0e-6);
	const double uC1 = 1000.0 / (cfg.uHub * tiU);
	const double uC2 = 1000.0 / tiU;
	const double vC = 1000.0 / (cfg.uHub * tiV);
	const double wC = 1000.0 / (cfg.uHub * tiW);

	WriteScalar<std::int16_t>(out, static_cast<std::int16_t>(-99));
	WriteScalar<std::int16_t>(out, static_cast<std::int16_t>(4));
	WriteScalar<std::int32_t>(out, static_cast<std::int32_t>(3));
	WriteScalar<float>(out, static_cast<float>(cfg.input.latitude));
	WriteScalar<float>(out, static_cast<float>(cfg.input.roughness));
	WriteScalar<float>(out, static_cast<float>(cfg.zBottom + 0.5 * cfg.gridHeight));
	WriteScalar<float>(out, static_cast<float>(100.0 * tiU));
	WriteScalar<float>(out, static_cast<float>(100.0 * tiV));
	WriteScalar<float>(out, static_cast<float>(100.0 * tiW));
	WriteScalar<float>(out, static_cast<float>(cfg.dz));
	WriteScalar<float>(out, static_cast<float>(cfg.dy));
	WriteScalar<float>(out, static_cast<float>(cfg.dx));
	WriteScalar<std::int32_t>(out, cfg.nSteps / 2);
	WriteScalar<float>(out, static_cast<float>(cfg.uHub));
	WriteScalar<float>(out, 0.0f);
	WriteScalar<float>(out, 0.0f);
	WriteScalar<float>(out, 0.0f);
	WriteScalar<std::int32_t>(out, 0);
	WriteScalar<std::int32_t>(out, cfg.input.turbSeed);
	WriteScalar<std::int32_t>(out, cfg.nz);
	WriteScalar<std::int32_t>(out, cfg.ny);
	for (int i = 0; i < 6; ++i)
		WriteScalar<std::int32_t>(out, 0);

	for (int t = 0; t < cfg.nSteps; ++t)
	{
		for (int iz = 0; iz < cfg.nz; ++iz)
		{
			for (int iy = 0; iy < cfg.ny; ++iy)
			{
				const int p = GridIndex(cfg, iz, iy);
				WriteScalar<std::int16_t>(out, EncodeInt16(uC1 * field.At(0, t, p) - uC2));
				WriteScalar<std::int16_t>(out, EncodeInt16(vC * field.At(1, t, p)));
				WriteScalar<std::int16_t>(out, EncodeInt16(wC * field.At(2, t, p)));
			}
		}
	}
}

/**
 * @brief 输出.sum统计摘要文件：写入输入参数、网格信息、IEC导出参数和分分量统计
 * @param cfg    仿真配置
 * @param stats  三分量统计
 * @param result 生成结果（含输出路径和警告）
 * @param path   输出.sum文件路径
 * @throw        无法打开文件时抛出runtime_error
 * @note         输出内容包括：输入参数、网格、预估成本、导出参数、输出文件清单、
 *               关键字状态、分分量均值/σ/TI，以及警告信息。
 * @code WriteSummary(cfg, stats, result, "output.sum"); @endcode
 */
void WriteSummary(const SimWindConfig &cfg,
                  const std::array<SimWindComponentStats, 3> &stats,
                  const SimWindResult &result,
                  const std::filesystem::path &path)
{
	std::ofstream out(path);
	if (!out)
		throw std::runtime_error(std::string(L_WIND_CannotOpenSUM) + ": " + path.string());

	out << L_SUM_Title << "\n";
	out << L_SUM_Separator << "\n\n";
	out << std::setprecision(10);
	out << L_SUM_Input << "\n";
	out << "  TurbModel: " << static_cast<int>(cfg.input.turbModel) << "\n";
	out << "  WindModel: " << static_cast<int>(cfg.input.windModel) << "\n";
	out << "  RandSeed: " << cfg.input.turbSeed << "\n";
	out << "  MeanWindSpeed: " << cfg.uHub << " m/s\n";
	out << "  HubHt: " << cfg.hubHeight << " m\n\n";

	out << L_SUM_Grid << "\n";
	out << "  NumPointY: " << cfg.ny << "\n";
	out << "  NumPointZ: " << cfg.nz << "\n";
	out << "  LenWidthY: " << cfg.gridWidth << " m\n";
	out << "  LenHeightZ: " << cfg.gridHeight << " m\n";
	out << "  Zbottom: " << cfg.zBottom << " m\n";
	out << "  TimeStep: " << cfg.dt << " s\n";
	out << "  NumSteps: " << cfg.nSteps << "\n\n";

	out << L_SUM_GenCostEstimate << "\n";
	out << "  StrictCoherenceComponents: " << cfg.strictCoherenceComponents << "\n";
	out << "  EstimatedPeakMemoryGiB: " << cfg.estimatedPeakMemoryGiB << "\n";
	out << "  EstimatedCholeskyFLOPs: " << cfg.estimatedCholeskyFlops << "\n\n";

	out << L_SUM_DerivedIEC << "\n";
	out << "  Lambda: " << cfg.lambda << " m\n";
	out << "  LC: " << cfg.lc << " m\n";
	out << "  SigmaU/SigmaV/SigmaW: " << cfg.sigma[0] << ", " << cfg.sigma[1] << ", " << cfg.sigma[2] << " m/s\n";
	out << "  IntegralScaleU/V/W: " << cfg.integralScale[0] << ", " << cfg.integralScale[1] << ", " << cfg.integralScale[2] << " m\n";
	out << "  ScaleIEC: " << cfg.scaleIEC << "\n";
	out << "  AllowCohApprox: " << (cfg.input.allowCohApprox ? "true" : "false") << "\n";
	out << "  UserDirectionProfile: " << (cfg.hasUserDirectionProfile ? "true" : "false") << "\n";
	out << "  UserSigmaProfile: " << (cfg.hasUserSigmaProfile ? "true" : "false") << "\n";
	out << "  UserLengthScaleProfile: " << (cfg.hasUserLengthScaleProfile ? "true" : "false") << "\n";
	out << "  ImprovedVKProfile: " << (cfg.hasImprovedVkProfile ? "true" : "false") << "\n";
	out << "  ZL: " << cfg.met.zL << "\n";
	out << "  L: " << cfg.met.moninLength << " m\n";
	out << "  UStar: " << cfg.met.uStar << " m/s\n";
	out << "  UStarDiab: " << cfg.met.uStarDiab << " m/s\n";
	out << "  ZI: " << cfg.met.mixingLayerDepth << " m\n";
	out << "  ReynoldsStressActive: " << (cfg.met.reynoldsStress.active ? "true" : "false") << "\n\n";

	out << L_SUM_BladedExport << "\n";
	out << "  Record2ModelID: " << BladedModelId(cfg.input.turbModel) << "\n";
	out << "  ExportLateralScaleU/V/W: " << cfg.lateralScale[0] << ", " << cfg.lateralScale[1] << ", " << cfg.lateralScale[2] << " m\n";
	out << "  ExportVerticalScaleU/V/W: " << cfg.verticalScale[0] << ", " << cfg.verticalScale[1] << ", " << cfg.verticalScale[2] << " m\n";
	if (IsMann(cfg.input.turbModel))
	{
		out << "  MannGamma: " << cfg.mannGamma << "\n";
		out << "  MannLength: " << cfg.mannLength << " m\n";
		out << "  MannMaxL: " << cfg.mannMaxL << " m\n";
	}
	out << "\n";

	out << L_SUM_OutputFiles << "\n";
	if (!result.btsPath.empty()) out << "  BTS: " << result.btsPath << "\n";
	if (!result.bladedWndPath.empty()) out << "  Bladed WND: " << result.bladedWndPath << "\n";
	if (!result.turbsimWndPath.empty()) out << "  TurbSim-compatible WND: " << result.turbsimWndPath << "\n";
	if (!result.wndPath.empty()) out << "  WND alias: " << result.wndPath << "\n";
	out << "  SUM: " << path.string() << "\n\n";

	out << L_SUM_InputKeywordStatus << "\n";
	out << "  CalWu/CalWv/CalWw: turbulence-only component switches\n";
	out << "  WrBlwnd/WrTrbts/WrTrwnd: export the same generated wind field in multiple formats\n";
	out << "  EWMReturn: removed legacy keyword; ignored if present in old input files\n";
	out << "  GenMethod: legacy keyword; does not control the current generator path\n";
	out << "  UseFFT: legacy keyword; does not override the current generator path\n";
	out << "  SumPrint: " << (cfg.input.sumPrint ? "true" : "false") << " (summary requested)\n\n";

	out << L_SUM_Statistics << "\n";
	static const char *names[3] = {"u", "v", "w"};
	for (int comp = 0; comp < 3; ++comp)
	{
		out << "  " << names[comp] << ": mean=" << stats[static_cast<std::size_t>(comp)].mean
		    << " sigma=" << stats[static_cast<std::size_t>(comp)].sigma
		    << " TI=" << 100.0 * stats[static_cast<std::size_t>(comp)].turbulenceIntensity << "%\n";
	}

	if (!result.warnings.empty())
	{
		out << "\n" << L_SUM_Warnings << "\n";
		for (const auto &warning : result.warnings)
			out << "  - " << warning << "\n";
	}
}
} // namespace

/**
 * @brief 仅验证输入文件而不生成风场的公开入口（诊断模式）
 * @param input 已解析的WindLInput配置
 * @note       直接委托给ValidateInput；无输出文件生成。
 * @code SimWind::ValidateInputOnly(input); @endcode
 */
void SimWind::ValidateInputOnly(const WindLInput &input)
{
	ValidateInput(input);
}

/**
 * @brief SimWind主生成入口：解析配置→生成湍流风场→写入输出文件→返回结果
 * @param input    已解析的WindLInput配置（湍流模型、网格、文件输出开关等）
 * @param progress 进度回调函数（接收字符串描述），可为空
 * @return         SimWindResult含统计量、输出路径、警告和预估成本
 * @note          流程：(1)BuildConfig构建配置并报告；(2)GenerateWindField分派生成路径；
 *                (3)ComputeFieldStats统计输出场；(4)按input.wrTrbts/wrBlwnd/wrTrwnd
 *                开关分别写.bts/.wnd/.ts.wnd；(5)按input.sumPrint写.sum摘要。
 *                全程含大量进度报告和ETA估算。
 * @code auto result = SimWind::Generate(input, [](const std::string& msg) { std::cout << msg << "\n"; }); @endcode
 */
SimWindResult SimWind::Generate(const WindLInput &input, SimWindProgressCallback progress)
{
	const auto startTime = std::chrono::steady_clock::now();
	if (progress)
		progress(L_PROG_ReadingInput);
	SimWindConfig cfg = BuildConfig(input);
	cfg.progress = std::move(progress);
	Report(cfg,
	       std::string(L_PROG_GridPrepared) + std::to_string(cfg.ny) + " x " + std::to_string(cfg.nz) +
	           ", steps=" + std::to_string(cfg.nSteps) +
	           ", strict-coherence components=" + std::to_string(cfg.strictCoherenceComponents) +
	           ", estimated peak memory=" + FormatGiB(cfg.estimatedPeakMemoryGiB) +
	           ", estimated Cholesky FLOPs=" + FormatScientific(cfg.estimatedCholeskyFlops) + ".");
	for (const auto &warning : cfg.warnings)
		Report(cfg, std::string(L_PROG_WarningPrefix) + ": " + warning);
	if (cfg.estimatedCholeskyFlops > 5.0e13 || cfg.estimatedPeakMemoryGiB > 12.0)
	{
		Report(cfg, L_PROG_LargeStrictCoh);
		Report(cfg, std::string(L_PROG_InitRuntimeEstimate) + ": " + InitialRuntimeEstimate(cfg.estimatedCholeskyFlops) + ".");
	}
	WindField field = GenerateWindField(cfg);

	SimWindResult result;
	result.gridPtsY = cfg.ny;
	result.gridPtsZ = cfg.nz;
	result.timeSteps = cfg.nSteps;
	result.timeStep = cfg.dt;
	result.hubHeight = cfg.hubHeight;
	result.meanWindSpeed = cfg.uHub;
	result.estimatedPeakMemoryGiB = cfg.estimatedPeakMemoryGiB;
	result.estimatedCholeskyFlops = cfg.estimatedCholeskyFlops;
	result.warnings = cfg.warnings;
	Report(cfg, L_PROG_ComputingStats);
	result.stats = ComputeFieldStats(cfg, field);

	Report(cfg, L_PROG_WritingOutput);
	if (input.wrTrbts)
	{
		auto path = cfg.outputBase;
		result.btsPath = path.replace_extension(".bts").string();
		Report(cfg, std::string(L_PROG_GenBTS) + " \"" + result.btsPath + "\".");
		WriteBts(cfg, field, result.btsPath);
	}

	if (input.wrBlwnd)
	{
		auto path = cfg.outputBase;
		result.bladedWndPath = path.replace_extension(".wnd").string();
		Report(cfg, std::string(L_PROG_GenBladed) + " \"" + result.bladedWndPath + "\".");
		WriteBladedWnd(cfg, field, result.stats, result.bladedWndPath);
		result.wndPath = result.bladedWndPath;
	}

	if (input.wrTrwnd)
	{
		auto path = cfg.outputBase;
		result.turbsimWndPath = path.replace_extension(input.wrBlwnd ? ".ts.wnd" : ".wnd").string();
		Report(cfg, std::string(L_PROG_GenTurbSim) + " \"" + result.turbsimWndPath + "\".");
		WriteTurbSimWnd(cfg, field, result.stats, result.turbsimWndPath);
		if (result.wndPath.empty())
			result.wndPath = result.turbsimWndPath;
	}

	if (input.sumPrint)
	{
		auto path = cfg.outputBase;
		result.sumPath = path.replace_extension(".sum").string();
		Report(cfg, std::string(L_PROG_WritingSum) + " \"" + result.sumPath + "\".");
		WriteSummary(cfg, result.stats, result, result.sumPath);
	}

	const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
	Report(cfg, std::string(L_PROG_ProcessingComplete) + " " + FormatSeconds(elapsed) + L_PROG_CPUSeconds);
	return result;
}

/**
 * @brief 从.qwd文件路径直接生成湍流风场的便捷入口（文件→解析→生成一站式）
 * @param qwdPath  .qwd输入文件路径（Qahse WindL定义格式）
 * @param progress 进度回调函数，可为空
 * @return         SimWindResult（同SimWind::Generate）
 * @note           内部依次调用ReadWindLInput(qwdPath)和SimWind::Generate。
 * @code auto result = SimWind::GenerateFromFile("project.qwd", progressCallback); @endcode
 */
SimWindResult SimWind::GenerateFromFile(const std::string &qwdPath, SimWindProgressCallback progress)
{
	return Generate(ReadWindLInput(qwdPath), std::move(progress));
}
