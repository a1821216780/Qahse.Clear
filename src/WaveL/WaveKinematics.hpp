#pragma once

#include <string>
#include <vector>

#include "WaveL/WaveL_Type.hpp"

namespace wavel_detail
{
	/**
	 * @brief 波浪运动学场缓存容器，以四维规则网格存储自由表面高程、速度、加速度和动水压力。
	 *        Wave kinematics field cache container, storing free surface elevation, velocity,
	 *        acceleration, and dynamic pressure on a 4D regular grid.
	 *
	 * 网格维度为 (nx × ny × nz × nt)，分别对应空间三维 (x, y, z) 和时间一维 (t)。
	 * 内存布局：Index(ix, iy, iz, it) = ((it × nz + iz) × ny + iy) × nx + ix，
	 * 即最内层维度为 x，向外依次为 y、z、t。
	 *
	 * Grid dimensions are (nx × ny × nz × nt) for space (x, y, z) and time (t).
	 * Memory layout: Index(ix, iy, iz, it) = ((it × nz + iz) × ny + iy) × nx + ix,
	 * i.e., innermost dimension is x, then y, z, t moving outward.
	 */
	struct WaveFieldCache
	{
		int nx = 0;                  ///< X 方向（顺浪向）网格点数。X-direction grid point count.
		int ny = 0;                  ///< Y 方向（横向）网格点数。Y-direction grid point count.
		int nz = 0;                  ///< Z 方向（垂向）网格点数。Z-direction grid point count.
		int nt = 0;                  ///< 时间步数。Number of time steps.
		double dx = 0.0;             ///< X 方向网格间距 [m]。X-direction grid spacing [m].
		double dy = 0.0;             ///< Y 方向网格间距 [m]。Y-direction grid spacing [m].
		double dz = 0.0;             ///< Z 方向网格间距 [m]。Z-direction grid spacing [m].
		double dt = 0.0;             ///< 时间步长 [s]。Time step size [s].
		double zBottom = 0.0;        ///< 网格底部高程 [m]（通常为 -waterDepth）。Grid bottom elevation [m] (typically -waterDepth).
		std::vector<double> eta;     ///< 自由表面高程 [m]，维度 nx×ny×nz×nt。Free surface elevation [m], size nx×ny×nz×nt.
		std::vector<double> u;       ///< X 方向速度 [m/s]。Velocity in X direction [m/s].
		std::vector<double> v;       ///< Y 方向速度 [m/s]。Velocity in Y direction [m/s].
		std::vector<double> w;       ///< Z 方向速度 [m/s]。Velocity in Z direction [m/s].
		std::vector<double> ax;      ///< X 方向加速度 [m/s²]。Acceleration in X direction [m/s²].
		std::vector<double> ay;      ///< Y 方向加速度 [m/s²]。Acceleration in Y direction [m/s²].
		std::vector<double> az;      ///< Z 方向加速度 [m/s²]。Acceleration in Z direction [m/s²].
		std::vector<double> dynP;    ///< 动水压力 [Pa]。Dynamic pressure [Pa].

		/**
		 * @brief 分配 (nx × ny × nz × nt) 个元素的空间，并初始化为零。
		 *        Allocate storage of (nx × ny × nz × nt) elements and initialize to zero.
		 * @param xCount X 方向点数。X point count.
		 * @param yCount Y 方向点数。Y point count.
		 * @param zCount Z 方向点数。Z point count.
		 * @param tCount 时间步数。Time step count.
		 */
		void Resize(int xCount, int yCount, int zCount, int tCount);

		/**
		 * @brief 将四维索引 (ix, iy, iz, it) 映射为一维线性索引。
		 *        Map 4D index (ix, iy, iz, it) to a 1D linear index.
		 * @param ix X 方向索引，范围 [0, nx-1]。X index, range [0, nx-1].
		 * @param iy Y 方向索引，范围 [0, ny-1]。Y index, range [0, ny-1].
		 * @param iz Z 方向索引，范围 [0, nz-1]。Z index, range [0, nz-1].
		 * @param it 时间索引，范围 [0, nt-1]。Time index, range [0, nt-1].
		 * @return   一维线性索引，范围 [0, nx×ny×nz×nt-1]。
		 *          1D linear index, range [0, nx×ny×nz×nt-1].
		 * @note 内存布局为行优先：(it × nz + iz) × ny + iy) × nx + ix。
		 *       Memory layout is row-major: ((it × nz + iz) × ny + iy) × nx + ix.
		 * @code
		 * size_t idx = cache.Index(3, 2, 5, 10);
		 * @endcode
		 */
		std::size_t Index(int ix, int iy, int iz, int it) const;
	};

	/**
	 * @brief 波浪运动学计算引擎，实现基于线性波浪理论的规则波叠加、缓存构建和插值采样。
	 *        Wave kinematics computation engine, implementing regular wave superposition based
	 *        on linear wave theory, cache building, and interpolation sampling.
	 *
	 * 支持三种水深条件下的速度势函数计算：
	 * - 深水 (kd > 20)：使用指数衰减形式 exp(kz)
	 * - 有限水深：使用双曲函数形式 cosh(k(z+d))/sinh(kd)
	 * - 外推拉伸模式：对自由表面以上区域线性外推
	 *
	 * Supports velocity potential computation under three depth conditions:
	 * - Deep water (kd > 20): uses exponential decay exp(kz)
	 * - Finite depth: uses hyperbolic form cosh(k(z+d))/sinh(kd)
	 * - Extrapolation stretching: linear extrapolation above free surface
	 */
	class WaveKinematicsEngine
	{
	public:
		/**
		 * @brief 直接叠加所有波浪成分，计算指定位置和时刻的运动学量。
		 *        Directly superpose all wave components to compute kinematics at a given location and time.
		 * @param waveTrains 波浪成分列表。List of wave components.
		 * @param input      波浪输入参数（水深、拉伸方式、MCF 直径、模拟时长等）。Wave input parameters (depth, stretching, MCF diameter, duration, etc.).
		 * @param x          X 坐标 [m]。X coordinate [m].
		 * @param y          Y 坐标 [m]。Y coordinate [m].
		 * @param z          Z 坐标 [m]。Z coordinate [m].
		 * @param time       模拟时间 [s]。Simulation time [s].
		 * @return           WaveKinematics 运动学结果。Wave kinetics result.
		 * @note 算法流程：
		 *       1. 用 WrapTime 将时间折叠到 [0, simDuration] 区间
		 *       2. 叠加所有成分的自由表面高程 eta = Σ amplitude × sin(kX - ωt + φ)
		 *       3. 对每个成分判断深浅水条件，计算深度衰减因子
		 *       4. 若启用 MacCamy-Fuchs 大直径修正，调整加速度因子和相位
		 *       5. 叠加速度和加速度（含方向余弦投影）
		 *       6. 叠加动水压力
		 *       Algorithm flow:
		 *       1. Fold time into [0, simDuration] with WrapTime
		 *       2. Superpose free surface: eta = Σ amplitude × sin(kX - ωt + φ)
		 *       3. For each component, determine deep/shallow water, compute depth decay
		 *       4. If MacCamy-Fuchs correction is enabled, adjust acceleration factor and phase
		 *       5. Superpose velocity and acceleration (with direction cosine projection)
		 *       6. Superpose dynamic pressure
		 */
		static WaveKinematics Evaluate(const std::vector<WaveTrain> &waveTrains,
			const WaveLInput &input,
			double x,
			double y,
			double z,
			double time);

		/**
		 * @brief 从预计算的运动学缓存中通过四维线性插值获取运动学量。
		 *        Obtain kinematics from precomputed kinematics cache via 4D linear interpolation.
		 * @param cache 运动学缓存数据。Kinematics cache data.
		 * @param x     X 坐标 [m]。X coordinate [m].
		 * @param y     Y 坐标 [m]。Y coordinate [m].
		 * @param z     Z 坐标 [m]。Z coordinate [m].
		 * @param time  模拟时间 [s]。Simulation time [s].
		 * @return      WaveKinematics 运动学结果。Wave kinetics result.
		 * @note 四维线性插值：先对每个缓存变量在 (x, y, z) 上进行三线性插值，
		 *       再在时间维上线性插值。网格外值钳制到边界。
		 *       4D linear interpolation: trilinear in (x, y, z) then linear in time.
		 *       Out-of-grid values are clamped to boundaries.
		 */
		static WaveKinematics EvaluateFromCache(const WaveFieldCache &cache,
			double x,
			double y,
			double z,
			double time);

		/**
		 * @brief 在所有网格点和时间步上预计算运动学量，填充缓存。
		 *        Precompute kinematics at all grid points and time steps, filling the cache.
		 * @param waveTrains 波浪成分列表。List of wave components.
		 * @param input      波浪输入参数（含网格配置）。Wave input parameters (with grid configuration).
		 * @param cache      待填充的缓存（将被 Resize 并写入）。Cache to be filled (will be resized and written).
		 * @throw std::runtime_error 当网格参数非法时抛出。Thrown when grid parameters are invalid.
		 * @note 对缓存中的每个 (ix, iy, iz, it) 调用 Evaluate() 逐一计算并存储结果。
		 *       For each (ix, iy, iz, it) in cache, calls Evaluate() to compute and store results.
		 */
		static void BuildCache(const std::vector<WaveTrain> &waveTrains,
			const WaveLInput &input,
			WaveFieldCache &cache);

		/**
		 * @brief 将每个时间步的运动学场输出为 VTU (XML Unstructured Grid) 格式快照。
		 *        Output the kinematics field at each time step as VTU (XML Unstructured Grid) snapshot files.
		 * @param waveTrains 波浪成分列表（当 outputKinematicsGrid=false 时直接返回）。Wave components (returns immediately if outputKinematicsGrid=false).
		 * @param input      波浪输入参数（含输出开关和目录配置）。Wave input parameters (with output toggle and directory config).
		 * @param directory  输出目录路径。Output directory path.
		 * @note 每个 VTU 文件包含 velocity（3 分量）、acceleration（3 分量）、dynamic_pressure（1 分量）
		 *       三个 PointData 数据集，文件名格式为 wave_<it>.vtu。
		 *       Each VTU file contains velocity (3 comp), acceleration (3 comp), dynamic_pressure (1 comp)
		 *       as PointData datasets; files named wave_<it>.vtu.
		 */
		static void WriteKinematicsSnapshots(const std::vector<WaveTrain> &waveTrains,
			const WaveLInput &input,
			const std::string &directory);
	};
}
