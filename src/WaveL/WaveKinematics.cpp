#include "WaveL/WaveKinematics.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "../IO/vtu11-cpp17.hpp"
#include "../Params.h"
#include "WaveL/IO/LocaleString_WaveL.hpp"

namespace
{
	/**
	 * @brief 深水判别阈值，当 kd = wavenumber × depth > 20 时视为深水条件。
	 *        Deep-water threshold: water is considered deep when kd = wavenumber × depth > 20.
	 *
	 * 深水条件下波浪不受海底影响，波速仅取决于波长；浅水条件下需考虑海底摩擦效应。
	 *
	 * Under deep-water conditions, waves are unaffected by seabed; wave speed depends only on
	 * wavelength. Under shallow-water conditions, seabed friction effects must be considered.
	 */
	constexpr double kDeepWaterThreshold = 20.0;

	/** @brief VTK 顶点单元类型代码（单点单元）。VTK vertex cell type code (single-point cell). */
	constexpr int kVtkVertex = 1;

	/**
	 * @brief 根据网格索引计算 X 坐标（顺浪向）。
	 *        Compute the X coordinate (down-wave) from grid index.
	 * @param cache 运动学缓存（取 nx 和 dx）。Kinematics cache (uses nx and dx).
	 * @param ix    X 方向网格索引。X-direction grid index.
	 * @return      X 坐标值：单点时返回 0.0，多点时关于原点对称分布。
	 *              X coordinate: returns 0.0 for single point, symmetric about origin for multiple points.
	 * @note 公式：x = (ix - 0.5×(nx-1)) × dx，使网格中心位于 x=0。
	 *       Formula: x = (ix - 0.5×(nx-1)) × dx, placing grid center at x=0.
	 */
	double CacheX(const wavel_detail::WaveFieldCache &cache, int ix)
	{
		return (cache.nx == 1) ? 0.0 : (static_cast<double>(ix) - 0.5 * static_cast<double>(cache.nx - 1)) * cache.dx;
	}

	/**
	 * @brief 根据网格索引计算 Y 坐标（横向）。
	 *        Compute the Y coordinate (lateral) from grid index.
	 * @param cache 运动学缓存（取 ny 和 dy）。Kinematics cache (uses ny and dy).
	 * @param iy    Y 方向网格索引。Y-direction grid index.
	 * @return      Y 坐标值：单点时返回 0.0，多点时关于原点对称分布。
	 *              Y coordinate: returns 0.0 for single point, symmetric about origin for multiple points.
	 */
	double CacheY(const wavel_detail::WaveFieldCache &cache, int iy)
	{
		return (cache.ny == 1) ? 0.0 : (static_cast<double>(iy) - 0.5 * static_cast<double>(cache.ny - 1)) * cache.dy;
	}

	/**
	 * @brief 根据网格索引计算 Z 坐标（垂向，从网格底部向上递增）。
	 *        Compute the Z coordinate (vertical, increasing upward from grid bottom) from grid index.
	 * @param cache 运动学缓存（取 zBottom 和 dz）。Kinematics cache (uses zBottom and dz).
	 * @param iz    Z 方向网格索引。Z-direction grid index.
	 * @return      Z 坐标：zBottom + iz × dz。Z coordinate: zBottom + iz × dz.
	 */
	double CacheZ(const wavel_detail::WaveFieldCache &cache, int iz)
	{
		return cache.zBottom + static_cast<double>(iz) * cache.dz;
	}

	/**
	 * @brief 将时间折叠到 [0, duration] 区间，实现周期性时间循环。
	 *        Fold time into [0, duration] for periodic time cycling.
	 * @param time     原始时间 [s]。Raw time [s].
	 * @param duration 周期长度 [s]。Period length [s].
	 * @return         折叠到 [0, duration] 内的时间。Time folded into [0, duration].
	 * @note 使用 fmod 实现，负值修正为正。duration <= 0 时直接返回原始时间。
	 *       Uses fmod; negative values corrected to positive. Returns raw time if duration <= 0.
	 * @code
	 * double lt = WrapTime(15.0, 10.0); // 返回 5.0
	 * @endcode
	 */
	double WrapTime(double time, double duration)
	{
		if (duration <= 0.0)
			return time;
		double wrapped = std::fmod(time, duration);
		if (wrapped < 0.0)
			wrapped += duration;
		return wrapped;
	}

	/**
	 * @brief 计算 MacCamy-Fuchs 大直径修正因子。
	 *        Compute the MacCamy-Fuchs large-diameter correction factor.
	 *
	 * MacCamy-Fuchs 理论修正了大直径圆柱体对波浪的衍射效应，减小了作用于结构上的
	 * 有效加速度。该因子用于波浪运动学中加速度项的缩放。
	 *
	 * MacCamy-Fuchs theory corrects for wave diffraction around large-diameter cylinders,
	 * reducing the effective acceleration on the structure. This factor scales the acceleration
	 * term in wave kinematics.
	 *
	 * @param waveNumber 波数 [rad/m]。Wavenumber [rad/m].
	 * @param depth      水深 [m]。Water depth [m].
	 * @param diameter   构件直径 [m]。Member diameter [m].
	 * @return           修正因子（0 到 1 之间），diameter<=0 或 waveNumber<=0 时返回 1.0。
	 *                   Correction factor (0 to 1); returns 1.0 if diameter<=0 or waveNumber<=0.
	 * @note 公式：factor = 1.05 × tanh(kd) / ((|0.5×d×k - 0.2|^2.2 + 1)^0.85)，钳制在 [0, 1]。
	 *       Formula: factor = 1.05 × tanh(kd) / ((|0.5×d×k - 0.2|^2.2 + 1)^0.85), clamped to [0, 1].
	 */
	double MacCamyFuchsFactor(double waveNumber, double depth, double diameter)
	{
		if (diameter <= 0.0 || waveNumber <= 0.0)
			return 1.0;

		const double kd = waveNumber * depth;
		const double x = 0.5 * diameter * waveNumber;
		const double factor = 1.05 * std::tanh(kd) /
			std::pow(std::pow(std::fabs(x - 0.2), 2.2) + 1.0, 0.85);
		return std::min(1.0, factor);
	}

	/**
	 * @brief 计算 MacCamy-Fuchs 相位偏移角。
	 *        Compute the MacCamy-Fuchs phase shift angle.
	 *
	 * 基于预计算的查找表通过线性插值获取相位偏移。查找表以 ka（波数×半径）的有理数为索引，
	 * 覆盖了从极小到极大的 ka 范围。
	 *
	 * Uses a precomputed lookup table with linear interpolation to obtain the phase shift.
	 * The table is indexed by ka (wavenumber × radius) ratio, covering a wide range of ka values.
	 *
	 * @param ratio ka 比值的倒数 = 1/(0.5×diameter×wavenumber)。Inverse of ka ratio = 1/(0.5×diameter×wavenumber).
	 * @return      相位偏移 [rad]。Phase shift [rad].
	 * @note 查找表包含 83 个 (ratio, phase_deg) 对，通过线性插值转换为弧度。
	 *       The lookup table contains 83 (ratio, phase_deg) pairs, linearly interpolated and converted to radians.
	 */
	double MacCamyFuchsPhaseShift(double ratio)
	{
		static const double ratios[] = {
			0.314159,0.320571,0.327249,0.334212,0.341477,0.349066,0.356999,0.365301,0.373999,0.383121,0.392699,0.402768,
			0.413367,0.42454,0.436332,0.448799,0.461999,0.475999,0.490874,0.506708,0.523599,0.541654,0.560999,0.581776,
			0.604152,0.628319,0.654498,0.682955,0.713998,0.747998,0.785398,0.826735,0.872665,0.923998,0.981748,1.047198,
			1.121997,1.208305,1.308997,1.427997,1.570796,1.745329,1.963495,2.243995,2.617994,3.141593,3.205707,3.272492,
			3.34212,3.414775,3.490659,3.569992,3.653015,3.739991,3.831211,3.926991,4.027683,4.133675,4.245395,4.363323,
			4.48799,4.619989,4.759989,4.908739,5.067085,5.235988,5.416539,5.609987,5.817764,6.041524,6.283185,6.544985,
			6.829549,7.139983,7.479983,7.853982,8.267349,8.726646,9.239978,9.817477,10.47198,11.21997,12.08305,13.08997,
			14.27997,15.70796,17.45329,19.63495,22.43995,26.17994,31.41593,39.26991,52.35988,78.53982,157.0796
		};
		static const double phases[] = {
			-443.14,-431.77,-420.39,-409.03,-397.66,-386.31,-374.96,-363.61,-352.27,-340.94,-329.62,-318.3,
			-306.99,-295.7,-284.21,-273.13,-261.87,-250.62,-239.38,-228.16,-216.97,-205.78,-194.62,-183.48,-173.03,
			-161.95,-150.91,-139.91,-128.95,-118.05,-107.20,-96.43,-85.74,-75.17,-64.67,-54.34,-44.10,-34.26,-24.62,
			-15.33,-6.53,1.61,8.86,14.83,18.97,20.54,20.52,20.47,20.39,20.26,20.10,19.91,19.68,19.41,19.11,18.77,
			18.39,17.98,17.54,17.07,16.56,16.03,15.47,14.48,14.27,13.64,13.00,12.34,11.67,11.00,10.32,9.64,8.96,8.29,
			7.63,6.98,6.35,5.74,5.15,4.59,4.05,3.54,3.06,2.61,2.20,1.82,1.47,1.16,0.89,0.65,0.45,0.29,0.16,0.07,0.02
		};

		const std::size_t count = sizeof(ratios) / sizeof(ratios[0]);
		if (ratio <= ratios[0])
			return phases[0] * M_PI / 180.0;
		if (ratio >= ratios[count - 1])
			return phases[count - 1] * M_PI / 180.0;

		// 二分查找后线性插值
		// Binary search then linear interpolation
		for (std::size_t i = 0; i + 1 < count; ++i)
		{
			if (ratio >= ratios[i] && ratio <= ratios[i + 1])
			{
				const double w = (ratio - ratios[i]) / (ratios[i + 1] - ratios[i]);
				return (phases[i] + (phases[i + 1] - phases[i]) * w) * M_PI / 180.0;
			}
		}
		return 0.0;
	}

	/**
	 * @brief 根据拉伸方法对 Z 坐标进行波浪拉伸变换。
	 *        Apply wave stretching transformation to Z coordinate based on stretching method.
	 *
	 * 四种拉伸模式：
	 * - NONE：返回原始 z（自由表面以上无运动学量）
	 * - VERTICAL：z>0 时返回 0（自由表面以上速度设为零）
	 * - EXTRAPOLATION：返回原始 z（后续在 Evaluate 中单独处理外推）
	 * - WHEELER：应用 Wheeler 变换 -> depth×(z - eta)/(eta + depth)
	 *
	 * Four stretching modes:
	 * - NONE: returns raw z (no kinematics above free surface)
	 * - VERTICAL: returns 0 when z>0 (zero velocity above free surface)
	 * - EXTRAPOLATION: returns raw z (extrapolation handled separately in Evaluate)
	 * - WHEELER: applies Wheeler transform -> depth×(z - eta)/(eta + depth)
	 *
	 * @param z          原始 Z 坐标 [m]。Raw Z coordinate [m].
	 * @param eta        瞬时自由表面高程 [m]。Instantaneous free surface elevation [m].
	 * @param depth      水深 [m]。Water depth [m].
	 * @param stretching 拉伸方法枚举。Stretching method enum.
	 * @return           变换后的有效 Z 坐标 [m]。Transformed effective Z coordinate [m].
	 */
	double ApplyStretching(double z, double eta, double depth, WaveStretching stretching)
	{
		switch (stretching)
		{
		case WaveStretching::NONE:
			return z;
		case WaveStretching::VERTICAL:
			return (z > 0.0) ? 0.0 : z;
		case WaveStretching::EXTRAPOLATION:
			return z;
		case WaveStretching::WHEELER:
			return depth * (z - eta) / (eta + depth);
		default:
			return z;
		}
	}

	/**
	 * @brief VTU 点网格辅助结构，用于构建 VTK UnstructuredGrid 格式的点集拓扑。
	 *        VTU point-mesh helper struct, used to build point-set topology in VTK UnstructuredGrid format.
	 *
	 * 实现 vtu11 库所需的 points()、connectivity()、offsets()、types() 接口，
	 * 用于输出波浪运动学的空间场快照。
	 *
	 * Implements the points(), connectivity(), offsets(), types() interface required by the
	 * vtu11 library, used for outputting spatial field snapshots of wave kinematics.
	 */
	struct PointMesh
	{
		std::vector<double> points_;                       ///< 节点坐标 (x, y, z 交错)。Point coordinates (x, y, z interleaved).
		std::vector<vtu11::VtkIndexType> connectivity_;    ///< 单元连接性（每个顶点独立）。Cell connectivity (each vertex independent).
		std::vector<vtu11::VtkIndexType> offsets_;         ///< 单元偏移量数组。Cell offset array.
		std::vector<vtu11::VtkCellType> types_;            ///< 单元类型数组（均为 kVtkVertex）。Cell type array (all kVtkVertex).

		const std::vector<double> &points() { return points_; }
		const std::vector<vtu11::VtkIndexType> &connectivity() { return connectivity_; }
		const std::vector<vtu11::VtkIndexType> &offsets() { return offsets_; }
		const std::vector<vtu11::VtkCellType> &types() { return types_; }
		std::size_t numberOfPoints() { return points_.size() / 3; }
		std::size_t numberOfCells() { return types_.size(); }
	};
}

/**
 * @brief 分配缓存存储空间，设置网格维度参数并将所有场量初始化为零。
 *        Allocate cache storage, set grid dimension parameters, and initialize all field quantities to zero.
 *
 * 总元素数 = nx × ny × nz × nt，每个场量（eta, u, v, w, ax, ay, az, dynP）均分配等量空间。
 * 通过 assign(total, 0.0) 确保初始值为零。
 *
 * Total elements = nx × ny × nz × nt; each field (eta, u, v, w, ax, ay, az, dynP) receives
 * equal storage. Uses assign(total, 0.0) to ensure zero initialization.
 */
void wavel_detail::WaveFieldCache::Resize(int xCount, int yCount, int zCount, int tCount)
{
	nx = xCount;
	ny = yCount;
	nz = zCount;
	nt = tCount;
	const std::size_t total = static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny) * static_cast<std::size_t>(nz) * static_cast<std::size_t>(nt);
	eta.assign(total, 0.0);
	u.assign(total, 0.0);
	v.assign(total, 0.0);
	w.assign(total, 0.0);
	ax.assign(total, 0.0);
	ay.assign(total, 0.0);
	az.assign(total, 0.0);
	dynP.assign(total, 0.0);
}

/**
 * @brief 四维索引到一维线性索引的行优先映射。
 *        Row-major mapping from 4D index to 1D linear index.
 *
 * 排列顺序（从外到内）：时间步 it -> Z 层 iz -> Y 行 iy -> X 列 ix，
 * 即最内层维度为 x，向外依次为 y、z、t。这种布局使连续的内存访问沿 x 方向。
 *
 * Ordering (outer to inner): time step it -> Z layer iz -> Y row iy -> X column ix,
 * i.e., innermost dimension is x, then y, z, t outward. This layout gives contiguous
 * memory access along x direction.
 */
std::size_t wavel_detail::WaveFieldCache::Index(int ix, int iy, int iz, int it) const
{
	return static_cast<std::size_t>(((it * nz + iz) * ny + iy) * nx + ix);
}

/**
 * @brief 基于线性波浪理论的规则波叠加运动学求解器。
 *        Regular wave superposition kinematics solver based on linear wave theory.
 *
 * 该函数是波浪运动学计算的核心，实现了以下物理模型：
 *
 * This function is the core of wave kinematics computation, implementing the following physics:
 *
 * @par 自由表面高程 (Free Surface Elevation)
 * eta(x, y, t) = Σ amplitudeᵢ × sin(kᵢ(Xᵢ) - ωᵢt + φᵢ)
 * 其中 Xᵢ = x×cosDirᵢ + y×sinDirᵢ 为波浪传播方向上的空间坐标。
 * Where Xᵢ = x×cosDirᵢ + y×sinDirᵢ is the spatial coordinate along the wave propagation direction.
 *
 * @par 深水条件 (Deep Water Condition)
 * 当 kd > 20 时使用深水公式。深水条件下，速度势随深度指数衰减：
 * depthVarXY = depthVarZ = exp(k×evalZ)
 *
 * When kd > 20, deep-water formulas are used. Under deep-water conditions, the velocity
 * potential decays exponentially with depth: depthVarXY = depthVarZ = exp(k×evalZ).
 *
 * @par 有限水深条件 (Finite Depth Condition)
 * 当 kd ≤ 20 时使用有限水深双曲函数：
 * depthVarXY = cosh(k(evalZ+d)) / sinh(kd)    (水平速度衰减因子)
 * depthVarZ  = sinh(k(evalZ+d)) / sinh(kd)     (垂向速度衰减因子)
 *
 * When kd ≤ 20, finite-depth hyperbolic functions are used:
 * depthVarXY = cosh(k(evalZ+d)) / sinh(kd)    (horizontal velocity decay)
 * depthVarZ  = sinh(k(evalZ+d)) / sinh(kd)     (vertical velocity decay)
 *
 * @par 外推拉伸 (Extrapolation Stretching)
 * 当 z > 0 (自由表面以上) 且使用 EXTRAPOLATION 拉伸时，对衰减因子在 z=0 处进行线性外推：
 * depthVarXY(z) = cosh(kd)/sinh(kd) + z×k
 * depthVarZ(z)  = 1 + z×k×(cosh(kd)/sinh(kd))
 *
 * When z > 0 (above free surface) and EXTRAPOLATION stretching is used, depth decay
 * factors are linearly extrapolated from z=0:
 * depthVarXY(z) = cosh(kd)/sinh(kd) + z×k
 * depthVarZ(z)  = 1 + z×k×(cosh(kd)/sinh(kd))
 *
 * @par 速度叠加 (Velocity Superposition)
 * u = Σ A_omegaᵢ × cosDirᵢ × depthVarXY × sin(phase)
 * v = Σ A_omegaᵢ × sinDirᵢ × depthVarXY × sin(phase)
 * w = Σ -A_omegaᵢ × depthVarZ × cos(phase)
 *
 * @par 加速度叠加 (Acceleration Superposition)
 * ax = Σ -A_omega2ᵢ × cosDirᵢ × depthVarXY × cos(phase) × accFactor
 * ay = Σ -A_omega2ᵢ × sinDirᵢ × depthVarXY × cos(phase) × accFactor
 * az = Σ -A_omega2ᵢ × depthVarZ × sin(phase) × accFactor
 *
 * 其中 accFactor 为 MacCamy-Fuchs 大直径修正因子。
 * Where accFactor is the MacCamy-Fuchs large-diameter correction factor.
 *
 * @par 动水压力 (Dynamic Pressure)
 * p_dyn = ρ×g × Σ amplitudeᵢ × pressureFactor × sin(phase)
 * pressureFactor = exp(k×evalZ) （深水）或 cosh(k(evalZ+d))/cosh(kd)（有限水深）
 *
 * p_dyn = ρ×g × Σ amplitudeᵢ × pressureFactor × sin(phase)
 * pressureFactor = exp(k×evalZ) (deep) or cosh(k(evalZ+d))/cosh(kd) (finite depth)
 */
WaveKinematics wavel_detail::WaveKinematicsEngine::Evaluate(const std::vector<WaveTrain> &waveTrains,
	const WaveLInput &input,
	double x,
	double y,
	double z,
	double time)
{
	WaveKinematics result;
	if (waveTrains.empty())
		return result;

	// 时间折叠：将任意时间映射到 [0, simDuration] 的周期窗口
	// Time folding: map arbitrary time to periodic window [0, simDuration]
	const double localTime = WrapTime(time + input.timeOffset, input.simDuration);

	// 第一步：叠加自由表面高程
	// Step 1: Superpose free surface elevation
	result.eta = 0.0;
	for (const auto &train : waveTrains)
	{
		const double X = x * train.cosDir + y * train.sinDir;
		result.eta += train.amplitude * std::sin(train.wavenumber * X - train.omega * localTime + train.phase);
	}

	const double eta = result.eta;
	double evalZ = z;

	// 第二步：根据拉伸方法确定有效计算深度
	// Step 2: Determine effective computation depth based on stretching method
	if (input.stretching == WaveStretching::NONE)
	{
		// 无拉伸：自由表面以上无运动学量，直接返回仅含 eta 的结果
		// No stretching: no kinematics above free surface, return result with eta only
		if (z > 0.0)
			return result;
	}
	else if (z > eta)
	{
		// 点位于瞬时波浪表面之上且非 NONE 拉伸模式→无运动学量
		// Point above instantaneous wave surface with non-NONE stretching → no kinematics
		return result;
	}

	evalZ = ApplyStretching(z, eta, input.waterDepth, input.stretching);
	// 拉伸后坐标超出海底时返回
	// Return if stretched coordinate exceeds seabed
	if (evalZ + input.waterDepth < 0.0)
		return result;

	// 第三步：逐波浪成分叠加速度和加速度
	// Step 3: Superpose velocity and acceleration per wave component
	for (const auto &train : waveTrains)
	{
		const double X = x * train.cosDir + y * train.sinDir;
		const double kd = train.wavenumber * input.waterDepth;
		const bool deepWater = kd > kDeepWaterThreshold || input.waterDepth > 100.0;

		// 计算深度衰减因子（根据水深条件选择深水/有限水深/外推公式）
		// Compute depth decay factors (choose deep/finite/extrapolation formula based on depth condition)
		double depthVarXY = 0.0;
		double depthVarZ = 0.0;
		if (deepWater)
		{
			// 深水：指数衰减，exp(kz)
			// Deep water: exponential decay, exp(kz)
			depthVarXY = std::exp(train.wavenumber * evalZ);
			depthVarZ = depthVarXY;
			// 外推拉伸：对自由表面以上 (z>0) 区域，从 z=0 处的值线性外推
			// Extrapolation stretching: for region above free surface (z>0), linearly extrapolate from z=0
			if (input.stretching == WaveStretching::EXTRAPOLATION && z > 0.0)
			{
				depthVarXY = 1.0 + train.wavenumber * z;
				depthVarZ = depthVarXY;
			}
		}
		else
		{
			// 有限水深：双曲函数形式
			// Finite depth: hyperbolic function form
			const double sinhDen = std::sinh(kd);
			depthVarXY = std::cosh(train.wavenumber * (evalZ + input.waterDepth)) / sinhDen;
			depthVarZ = std::sinh(train.wavenumber * (evalZ + input.waterDepth)) / sinhDen;
			// 外推拉伸：从 z=0 处的外推值
			// Extrapolation stretching: extrapolated from z=0
			if (input.stretching == WaveStretching::EXTRAPOLATION && z > 0.0)
			{
				const double z0xy = std::cosh(kd) / sinhDen;
				const double z0z = 1.0;
				const double dxy = train.wavenumber;
				const double dz = train.wavenumber * z0xy;
				depthVarXY = z0xy + z * dxy;
				depthVarZ = z0z + z * dz;
			}
		}

		double phase = train.wavenumber * X - train.omega * localTime + train.phase;
		double cosPhase = std::cos(phase);
		double sinPhase = std::sin(phase);

		// MacCamy-Fuchs 大直径修正：调整加速度因子和相位偏移
		// MacCamy-Fuchs large-diameter correction: adjust acceleration factor and phase shift
		double accFactor = 1.0;
		if (input.mcfDiameter > 0.0)
		{
			accFactor = MacCamyFuchsFactor(train.wavenumber, input.waterDepth, input.mcfDiameter);
			const double mcfPhaseShift = MacCamyFuchsPhaseShift(1.0 / (0.5 * input.mcfDiameter * train.wavenumber));
			phase += mcfPhaseShift;
			cosPhase = std::cos(phase);
			sinPhase = std::sin(phase);
		}

		// 速度叠加（线性波浪理论的水平与垂向分量）
		// Velocity superposition (horizontal and vertical components of linear wave theory)
		result.vel[0] += train.A_omega * train.cosDir * depthVarXY * sinPhase;
		result.vel[1] += train.A_omega * train.sinDir * depthVarXY * sinPhase;
		result.vel[2] += -train.A_omega * depthVarZ * cosPhase;

		// 加速度叠加（速度对时间的导数，含 MCF 修正因子）
		// Acceleration superposition (time derivative of velocity, with MCF correction factor)
		result.acc[0] += -train.A_omega2 * train.cosDir * depthVarXY * cosPhase * accFactor;
		result.acc[1] += -train.A_omega2 * train.sinDir * depthVarXY * cosPhase * accFactor;
		result.acc[2] += -train.A_omega2 * depthVarZ * sinPhase * accFactor;

		// 动水压力叠加（基于线性的非定常伯努利方程）
		// Dynamic pressure superposition (based on linearized unsteady Bernoulli equation)
		const double pressureFactor = deepWater
			? std::exp(train.wavenumber * evalZ)
			: std::cosh(train.wavenumber * (evalZ + input.waterDepth)) / std::cosh(kd);
		result.dynP += DENSITYWATER * GRAVITY * train.amplitude * pressureFactor * sinPhase;
	}

	return result;
}

/**
 * @brief 从运动学缓存中通过四维线性插值获取结果。
 *        Obtain kinematics results from cache via 4D linear interpolation.
 *
 * 插值策略：
 * 1. 对时间进行折叠（WrapTime），使其落入缓存时间范围
 * 2. 对 (x, y, z, t) 四个维度分别调用 indexWeight() 获取左右索引和权重
 * 3. 使用 lambda sample 对每个场量执行 4D 线性插值（先在空间上三线性插值，再在时间上线性插值）
 *
 * Interpolation strategy:
 * 1. Fold time (WrapTime) to fit within cache time range
 * 2. Call indexWeight() on each of (x, y, z, t) to get left/right indices and weights
 * 3. Use lambda sample to perform 4D linear interpolation on each field quantity
 *    (trilinear in space, then linear in time)
 *
 * @note 坐标超出网格范围时自动钳制到边界。缓存不可用时返回全零结果。
 *       Out-of-grid coordinates are automatically clamped to boundaries. Returns all-zero
 *       result when cache is unavailable.
 */
WaveKinematics wavel_detail::WaveKinematicsEngine::EvaluateFromCache(const WaveFieldCache &cache,
	double x,
	double y,
	double z,
	double time)
{
	WaveKinematics result;
	if (cache.nx <= 0 || cache.ny <= 0 || cache.nz <= 0 || cache.nt <= 0)
		return result;

	const double duration = cache.dt * static_cast<double>(std::max(1, cache.nt - 1));
	const double localTime = WrapTime(time, duration);

	// 辅助 lambda：计算给定维度上的插值索引和权重
	// Helper lambda: compute interpolation indices and weights for a given dimension
	const auto indexWeight = [](double value, double origin0, double spacing, int count) {
		if (count <= 1 || spacing <= 0.0)
			return std::tuple<int, int, double>(0, 0, 0.0);
		double coord = (value - origin0) / spacing;
		coord = std::max(0.0, std::min(coord, static_cast<double>(count - 1)));
		const int i0 = static_cast<int>(std::floor(coord));
		const int i1 = std::min(count - 1, i0 + 1);
		return std::tuple<int, int, double>(i0, i1, coord - static_cast<double>(i0));
	};

	const auto [ix0, ix1, wx] = indexWeight(x, CacheX(cache, 0), cache.dx, cache.nx);
	const auto [iy0, iy1, wy] = indexWeight(y, CacheY(cache, 0), cache.dy, cache.ny);
	const auto [iz0, iz1, wz] = indexWeight(z, CacheZ(cache, 0), cache.dz, cache.nz);
	const auto [it0, it1, wt] = indexWeight(localTime, 0.0, cache.dt, cache.nt);

	// 辅助 lambda：对缓存中的任意场量执行四维线性插值
	// Helper lambda: perform 4D linear interpolation on an arbitrary field in cache
	auto sample = [&](const std::vector<double> &values) {
		const auto lerp = [](double a, double b, double w) { return a + (b - a) * w; };
		// 单时间步内的三线性插值
		// Trilinear interpolation within a single time step
		auto cell = [&](int it) {
			const double c000 = values[cache.Index(ix0, iy0, iz0, it)];
			const double c100 = values[cache.Index(ix1, iy0, iz0, it)];
			const double c010 = values[cache.Index(ix0, iy1, iz0, it)];
			const double c110 = values[cache.Index(ix1, iy1, iz0, it)];
			const double c001 = values[cache.Index(ix0, iy0, iz1, it)];
			const double c101 = values[cache.Index(ix1, iy0, iz1, it)];
			const double c011 = values[cache.Index(ix0, iy1, iz1, it)];
			const double c111 = values[cache.Index(ix1, iy1, iz1, it)];
			const double c00 = lerp(c000, c100, wx);
			const double c10 = lerp(c010, c110, wx);
			const double c01 = lerp(c001, c101, wx);
			const double c11 = lerp(c011, c111, wx);
			const double c0 = lerp(c00, c10, wy);
			const double c1 = lerp(c01, c11, wy);
			return lerp(c0, c1, wz);
		};
		// 在两个时间步之间线性插值
		// Linear interpolation between two time steps
		const double v0 = cell(it0);
		const double v1 = cell(it1);
		return lerp(v0, v1, wt);
	};

	// 对每个场量应用相同的四维插值
	// Apply the same 4D interpolation to each field
	result.eta = sample(cache.eta);
	result.vel[0] = sample(cache.u);
	result.vel[1] = sample(cache.v);
	result.vel[2] = sample(cache.w);
	result.acc[0] = sample(cache.ax);
	result.acc[1] = sample(cache.ay);
	result.acc[2] = sample(cache.az);
	result.dynP = sample(cache.dynP);
	return result;
}

/**
 * @brief 构建运动学缓存：对四维规则网格上的每个点遍历时间步并调用 Evaluate()。
 *        Build kinematics cache: iterate over every point on the 4D regular grid and call Evaluate().
 *
 * 计算流程：
 * 1. 校验网格参数（nx, ny, nz 必须 > 0）
 * 2. 计算时间步数 nt = simDuration / timeStep + 1
 * 3. 设置缓存间距（dx, dy, dz 为 0 时使用默认值）
 * 4. Resize 缓存并逐点遍历 (it, iz, iy, ix)，调用 Evaluate 计算运动学量
 *
 * Computation flow:
 * 1. Validate grid parameters (nx, ny, nz must be > 0)
 * 2. Compute time step count: nt = simDuration / timeStep + 1
 * 3. Set cache spacing (defaults when dx, dy, dz are 0)
 * 4. Resize cache and iterate over (it, iz, iy, ix), calling Evaluate for kinematics
 *
 * @throw std::runtime_error 当网格参数非法时抛出。Thrown when grid parameters are invalid.
 *
 * @note 默认 dz：当 gridDZ=0 且 nz>1 时，dz = waterDepth / (nz-1)，使网格均匀覆盖整个水深。
 *       Default dz: when gridDZ=0 and nz>1, dz = waterDepth / (nz-1), uniformly covering water depth.
 */
void wavel_detail::WaveKinematicsEngine::BuildCache(const std::vector<WaveTrain> &waveTrains,
	const WaveLInput &input,
	WaveFieldCache &cache)
{
	if (input.gridNX <= 0 || input.gridNY <= 0 || input.gridNZ <= 0)
		throw std::runtime_error(L_WAVEL_GridInvalid);

	const int nt = std::max(1, static_cast<int>(std::llround(input.simDuration / input.timeStep)) + 1);
	cache.dx = input.gridDX > 0.0 ? input.gridDX : 1.0;
	cache.dy = input.gridDY > 0.0 ? input.gridDY : 1.0;
	cache.dz = input.gridDZ > 0.0 ? input.gridDZ : ((input.gridNZ > 1) ? input.waterDepth / static_cast<double>(input.gridNZ - 1) : 1.0);
	cache.dt = input.timeStep;
	cache.zBottom = -input.waterDepth;
	cache.Resize(input.gridNX, input.gridNY, input.gridNZ, nt);

	// 四重循环：时间 -> Z -> Y -> X，逐点调用 Evaluate 并存储结果
	// Quadruple loop: time -> Z -> Y -> X, calling Evaluate per point and storing results
	for (int it = 0; it < nt; ++it)
	{
		const double time = static_cast<double>(it) * input.timeStep;
		for (int iz = 0; iz < input.gridNZ; ++iz)
		{
			const double z = cache.zBottom + static_cast<double>(iz) * cache.dz;
			for (int iy = 0; iy < input.gridNY; ++iy)
			{
				const double y = (input.gridNY == 1) ? 0.0 : (static_cast<double>(iy) - 0.5 * static_cast<double>(input.gridNY - 1)) * cache.dy;
				for (int ix = 0; ix < input.gridNX; ++ix)
				{
					const double x = CacheX(cache, ix);
					const auto kin = Evaluate(waveTrains, input, x, y, z, time);
					const std::size_t idx = cache.Index(ix, iy, iz, it);
					cache.eta[idx] = kin.eta;
					cache.u[idx] = kin.vel[0];
					cache.v[idx] = kin.vel[1];
					cache.w[idx] = kin.vel[2];
					cache.ax[idx] = kin.acc[0];
					cache.ay[idx] = kin.acc[1];
					cache.az[idx] = kin.acc[2];
					cache.dynP[idx] = kin.dynP;
				}
			}
		}
	}
}

/**
 * @brief 将每个时间步的运动学场写入 VTU 文件，用于 ParaView 等工具的可视化诊断。
 *        Write kinematics field for each time step as VTU files for visualization diagnostics
 *        with tools like ParaView.
 *
 * 输出内容：
 * - 每个时间步生成一个 wave_<it>.vtu 文件
 * - 网格为规则点集（nx × ny × nz 个顶点）
 * - 每个文件包含三个 PointData 数组：
 *   - velocity (3 分量)：u, v, w
 *   - acceleration (3 分量)：ax, ay, az
 *   - dynamic_pressure (1 分量)：dynP
 *
 * Output contents:
 * - One wave_<it>.vtu file per time step
 * - Grid is a regular point set (nx × ny × nz vertices)
 * - Each file contains three PointData arrays:
 *   - velocity (3 comp): u, v, w
 *   - acceleration (3 comp): ax, ay, az
 *   - dynamic_pressure (1 comp): dynP
 *
 * @note 当 outputKinematicsGrid=false 时函数直接返回，不执行任何操作。
 *       When outputKinematicsGrid=false, the function returns immediately without any action.
 */
void wavel_detail::WaveKinematicsEngine::WriteKinematicsSnapshots(const std::vector<WaveTrain> &waveTrains,
	const WaveLInput &input,
	const std::string &directory)
{
	if (!input.outputKinematicsGrid)
		return;

	std::filesystem::create_directories(directory);
	WaveFieldCache cache;
	BuildCache(waveTrains, input, cache);

	// 构建 VTU 点网格拓扑（每点一个顶点单元）
	// Build VTU point mesh topology (one vertex cell per point)
	PointMesh mesh;
	mesh.points_.reserve(static_cast<std::size_t>(cache.nx) * cache.ny * cache.nz * 3);
	mesh.connectivity_.reserve(static_cast<std::size_t>(cache.nx) * cache.ny * cache.nz);
	mesh.offsets_.reserve(static_cast<std::size_t>(cache.nx) * cache.ny * cache.nz);
	mesh.types_.reserve(static_cast<std::size_t>(cache.nx) * cache.ny * cache.nz);

	vtu11::VtkIndexType offset = 0;
	for (int iz = 0; iz < cache.nz; ++iz)
	{
		const double z = cache.zBottom + static_cast<double>(iz) * cache.dz;
		for (int iy = 0; iy < cache.ny; ++iy)
		{
			const double y = (cache.ny == 1) ? 0.0 : (static_cast<double>(iy) - 0.5 * static_cast<double>(cache.ny - 1)) * cache.dy;
				for (int ix = 0; ix < cache.nx; ++ix)
				{
					const double x = CacheX(cache, ix);
					mesh.points_.push_back(x);
				mesh.points_.push_back(y);
				mesh.points_.push_back(z);
				mesh.connectivity_.push_back(offset);
				++offset;
				mesh.offsets_.push_back(offset);
				mesh.types_.push_back(kVtkVertex);
			}
		}
	}

	const std::size_t pointCount = static_cast<std::size_t>(cache.nx) * cache.ny * cache.nz;
	// 对每个时间步提取运动学数据并写入 VTU 文件
	// Extract kinematics data for each time step and write VTU file
	for (int it = 0; it < cache.nt; ++it)
	{
		std::vector<double> velocity;
		std::vector<double> acceleration;
		std::vector<double> pressure;
		velocity.reserve(pointCount * 3);
		acceleration.reserve(pointCount * 3);
		pressure.reserve(pointCount);

		for (int iz = 0; iz < cache.nz; ++iz)
		{
			for (int iy = 0; iy < cache.ny; ++iy)
			{
				for (int ix = 0; ix < cache.nx; ++ix)
				{
					const std::size_t idx = cache.Index(ix, iy, iz, it);
					velocity.push_back(cache.u[idx]);
					velocity.push_back(cache.v[idx]);
					velocity.push_back(cache.w[idx]);
					acceleration.push_back(cache.ax[idx]);
					acceleration.push_back(cache.ay[idx]);
					acceleration.push_back(cache.az[idx]);
					pressure.push_back(cache.dynP[idx]);
				}
			}
		}

		const std::vector<vtu11::DataSetInfo> infos = {
			{"velocity", vtu11::DataSetType::PointData, 3},
			{"acceleration", vtu11::DataSetType::PointData, 3},
			{"dynamic_pressure", vtu11::DataSetType::PointData, 1}
		};
		const std::vector<vtu11::DataSetData> data = {velocity, acceleration, pressure};
		const std::filesystem::path filePath = std::filesystem::path(directory) / ("wave_" + std::to_string(it) + ".vtu");
		vtu11::writeVtu(filePath.string(), mesh, infos, data, "Ascii");
	}
}
