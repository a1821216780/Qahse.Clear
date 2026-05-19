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
// 该文件定义了 WindField 结构体，作为三维湍流风场容器，管理风场数据的存储、坐标构建、
// 统计计算与文件导入，支持多种导入格式（BTS、Bladed WND、TurbSim WND）和时空插值采样。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "Math/Vec3.h"
#include "WindL/WindL_Type.hpp"

/**
 * @brief 三维湍流风场容器，管理风场数据的存储、坐标构建、统计计算与文件导入。
 *        3D turbulent wind field container for data storage, coordinate construction,
 *        statistical computation, and file import.
 *
 * 内存布局采用交错存储（时间步优先，空间点连续），支持多种导入格式（BTS、Bladed WND、TurbSim WND）
 * 以及时空插值采样，并提供风场统计量（均值、标准差、湍流强度）的自动计算。
 *
 * Memory layout uses interleaved storage (time-step major, spatial-point contiguous).
 * Supports multiple import formats (BTS, Bladed WND, TurbSim WND), spatio-temporal
 * interpolation sampling, and automatic computation of field statistics (mean, standard
 * deviation, turbulence intensity).
 */
struct WindField
{
	int nSteps = 0;                     ///< 时间步数。Number of time steps.
	int ny = 0;                         ///< Y 方向（水平横向）网格点数。Number of grid points in Y (horizontal lateral) direction.
	int nz = 0;                         ///< Z 方向（垂直）网格点数。Number of grid points in Z (vertical) direction.
	int nPoints = 0;                    ///< 空间点总数 (ny × nz)。Total number of spatial points (ny × nz).

	double dy = 0.0;                    ///< Y 方向网格间距 [m]。Grid spacing in Y direction [m].
	double dz = 0.0;                    ///< Z 方向网格间距 [m]。Grid spacing in Z direction [m].
	double dt = 0.0;                    ///< 时间步长 [s]。Time step size [s].
	double hubHeight = 0.0;             ///< 轮毂高度 [m]。Hub height [m].
	double zBottom = 0.0;               ///< 网格底部高程 [m]。Grid bottom elevation [m].
	double fieldDimY = 0.0;             ///< Y 方向风场总宽度 [m]。Total field width in Y direction [m].
	double fieldDimZ = 0.0;             ///< Z 方向风场总高度 [m]。Total field height in Z direction [m].
	double meanWindSpeed = 0.0;         ///< 轮毂平均风速 [m/s]。Mean wind speed at hub height [m/s].

	WndFormat wndFormat = WndFormat::TURBSIM_BTS;  ///< 风场文件的格式类型。Wind field file format type.
	std::filesystem::path sourcePath;              ///< 导入文件的源路径。Source path of the imported file.
	std::string summaryPath;                       ///< 关联的统计摘要 (.sum) 文件路径。Path to the associated summary (.sum) file.
	bool usedCompanionSummary = false;             ///< 是否使用了伴随 .sum 文件的统计信息。Whether companion .sum statistics were used.

	std::array<std::vector<double>, 3> component;  ///< 三个速度分量向量 (u, v, w)，按 [step × nPoints + point] 交错存储。Three velocity component vectors (u, v, w), interleaved as [step × nPoints + point].
	std::vector<double> yCoords;                   ///< Y 坐标向量（水平横向）[m]。Y-coordinate vector (horizontal lateral) [m].
	std::vector<double> zCoords;                   ///< Z 坐标向量（垂直）[m]。Z-coordinate vector (vertical) [m].
	std::vector<double> timeCoords;                ///< 时间坐标向量 [s]。Time coordinate vector [s].
	std::array<double, 3> mean{0.0, 0.0, 0.0};    ///< 三个速度分量的均值 (u, v, w) [m/s]。Mean of three velocity components (u, v, w) [m/s].
	std::array<double, 3> sigma{0.0, 0.0, 0.0};   ///< 三个速度分量的标准差 (u, v, w) [m/s]。Standard deviation of three velocity components (u, v, w) [m/s].
	std::array<double, 3> turbulenceIntensity{0.0, 0.0, 0.0};  ///< 三个分量的湍流强度 (fraction)。Turbulence intensity of three components (fraction).
	std::vector<std::string> warnings;             ///< 导入过程中的警告信息列表。List of warning messages from the import process.

	/**
	 * @brief 分配存储空间，初始化风场维度。
	 *        Allocate storage and initialize field dimensions.
	 * @param steps    时间步数。Number of time steps.
	 * @param nyPoints Y 方向网格点数。Number of grid points in Y direction.
	 * @param nzPoints Z 方向网格点数。Number of grid points in Z direction.
	 * @note 同时设置 nPoints = ny × nz，并将各分量向量清零。
	 *       Also sets nPoints = ny × nz and zero-initializes all component vectors.
	 */
	void Resize(int steps, int nyPoints, int nzPoints);

	/**
	 * @brief 根据网格参数构建 Y、Z 和时间坐标向量。
	 *        Build Y, Z, and time coordinate vectors from grid parameters.
	 * @note Y 坐标关于零点对称；Z 坐标从 zBottom 开始向上递增；
	 *       时间坐标从 0 开始以 dt 步长递增。单点时按特殊规则处理。
	 *       Y coords are symmetric about zero; Z coords increase upward from zBottom;
	 *       time coords start at 0 and increase by dt. Single-point cases are handled specially.
	 */
	void BuildCoordinates();

	/**
	 * @brief 计算三个速度分量的统计量：均值、标准差和湍流强度。
	 *        Compute statistics for the three velocity components: mean, standard deviation,
	 *        and turbulence intensity.
	 * @note 结果存储在 mean、sigma 和 turbulenceIntensity 成员中。
	 *       Results are stored in the mean, sigma, and turbulenceIntensity members.
	 */
	void ComputeStats();

	/**
	 * @brief 将二维网格索引 (iz, iy) 转换为一维索引。
	 *        Convert 2D grid index (iz, iy) to 1D linear index.
	 * @param iz Z 方向（垂直）网格索引，范围 [0, nz-1]。Z-direction (vertical) grid index, range [0, nz-1].
	 * @param iy Y 方向（水平）网格索引，范围 [0, ny-1]。Y-direction (horizontal) grid index, range [0, ny-1].
	 * @return  一维索引值 iz × ny + iy。1D index value iz × ny + iy.
	 * @note    按行主序（iy 连续）排列。Row-major ordering (iy contiguous).
	 * @code
	 * int idx = field.GridIndex(2, 3);
	 * @endcode
	 */
	int GridIndex(int iz, int iy) const;

	/**
	 * @brief 获取指定分量在指定时间步和空间点处的风速值（可写引用）。
	 *        Get wind speed value at a given component, time step, and spatial point (writable reference).
	 * @param comp  速度分量索引：0=u（纵向），1=v（横向），2=w（竖向）。Component index: 0=u (longitudinal), 1=v (lateral), 2=w (vertical).
	 * @param step  时间步索引，范围 [0, nSteps-1]。Time step index, range [0, nSteps-1].
	 * @param point 空间点一维索引，范围 [0, nPoints-1]。Spatial point 1D index, range [0, nPoints-1].
	 * @return      对应位置的可写引用，允许直接赋值修改风场。Writable reference to the value, allowing direct modification.
	 * @note        内存布局为交错存储：component[comp][step * nPoints + point]。
	 *              Memory layout is interleaved: component[comp][step * nPoints + point].
	 */
	double &At(int comp, int step, int point);

	/**
	 * @brief 获取指定分量在指定时间步和空间点处的风速值（只读）。
	 *        Get wind speed value at a given component, time step, and spatial point (read-only).
	 * @param comp  速度分量索引：0=u（纵向），1=v（横向），2=w（竖向）。Component index: 0=u (longitudinal), 1=v (lateral), 2=w (vertical).
	 * @param step  时间步索引，范围 [0, nSteps-1]。Time step index, range [0, nSteps-1].
	 * @param point 空间点一维索引，范围 [0, nPoints-1]。Spatial point 1D index, range [0, nPoints-1].
	 * @return      对应位置的风速值（const 副本）。Wind speed value at the position (const copy).
	 * @note        与可写版本共享相同的内存布局。Shares the same memory layout as the writable overload.
	 */
	double At(int comp, int step, int point) const;

	/**
	 * @brief 获取指定分量在指定时间步和二维网格位置 (iz, iy) 处的风速值（可写引用）。
	 *        Get wind speed value at a given component, time step, and 2D grid position (iz, iy) (writable reference).
	 * @param comp 速度分量索引：0=u, 1=v, 2=w。Component index: 0=u, 1=v, 2=w.
	 * @param step 时间步索引，范围 [0, nSteps-1]。Time step index, range [0, nSteps-1].
	 * @param iz   Z 方向网格索引，范围 [0, nz-1]。Z-direction grid index, range [0, nz-1].
	 * @param iy   Y 方向网格索引，范围 [0, ny-1]。Y-direction grid index, range [0, ny-1].
	 * @return     对应位置的可写引用。Writable reference to the value.
	 * @note       等价于 At(comp, step, GridIndex(iz, iy))。Equivalent to At(comp, step, GridIndex(iz, iy)).
	 */
	double &At(int comp, int step, int iz, int iy);

	/**
	 * @brief 获取指定分量在指定时间步和二维网格位置 (iz, iy) 处的风速值（只读）。
	 *        Get wind speed value at a given component, time step, and 2D grid position (iz, iy) (read-only).
	 * @param comp 速度分量索引：0=u, 1=v, 2=w。Component index: 0=u, 1=v, 2=w.
	 * @param step 时间步索引，范围 [0, nSteps-1]。Time step index, range [0, nSteps-1].
	 * @param iz   Z 方向网格索引，范围 [0, nz-1]。Z-direction grid index, range [0, nz-1].
	 * @param iy   Y 方向网格索引，范围 [0, ny-1]。Y-direction grid index, range [0, ny-1].
	 * @return     对应位置的风速值（const 副本）。Wind speed value at the position (const copy).
	 * @note       等价于 At(comp, step, GridIndex(iz, iy))。Equivalent to At(comp, step, GridIndex(iz, iy)).
	 */
	double At(int comp, int step, int iz, int iy) const;

	/**
	 * @brief 在风场中进行时空采样，返回指定位置和时刻的三分量风速。
	 *        Perform spatio-temporal sampling in the wind field, returning three-component
	 *        wind speeds at the specified location and time.
	 * @param y         采样点 Y 坐标 [m]。Sample point Y coordinate [m].
	 * @param z         采样点 Z 坐标（高度）[m]。Sample point Z coordinate (height) [m].
	 * @param t         采样时刻 [s]。Sample time [s].
	 * @param method    插值方法（线性或三次样条）。Interpolation method (linear or cubic spline).
	 * @param cycleWind 是否循环采样时间（超出时长时绕回）。Whether to cycle sample time (wrap around when exceeding duration).
	 * @return          三个速度分量 (u, v, w) 的采样结果 [m/s]。Three-component (u, v, w) sampling result [m/s].
	 * @throw std::runtime_error 当风场为空或采样时间超限（cycleWind=false）时抛出。
	 *                            Thrown when wind field is empty or sample time exceeds bounds (cycleWind=false).
	 * @note Y 坐标使用镜像边界条件，Z 坐标使用钳制边界条件。
	 *       Y-coordinate uses mirroring boundary condition; Z-coordinate uses clamping boundary condition.
	 * @code
	 * auto vel = field.Sample(5.0, 90.0, 10.0, InterpMethod::TRILINEAR, true);
	 * @endcode
	 */
	std::array<double, 3> Sample(double y, double z, double t, InterpMethod method, bool cycleWind) const;
	std::array<double, 3> SampleAt(double x, double y, double z, double t, const WindVelocityOptions &options = {}) const;
	std::array<double, 3> SampleLinearFast(double y, double z, double t, bool cycleWind) const;
	Vec3 getWindspeed(Vec3 vec,
	                  double time,
	                  bool mirror = false,
	                  bool isAutoFielShift = true,
	                  double shiftTime = 0.0) const;

	/**
	 * @brief 从 .bts 二进制格式文件导入风场。
	 *        Import wind field from a .bts binary format file.
	 * @param path .bts 文件路径。Path to the .bts file.
	 * @return 导入的 WindField 实例。Imported WindField instance.
	 * @throw std::runtime_error 当文件无法打开或格式不支持时抛出。
	 *                            Thrown when file cannot be opened or format is unsupported.
	 */
	static WindField ReadBts(const std::string &path);

	/**
	 * @brief 从 TurbSim .wnd 二进制格式文件导入风场。
	 *        Import wind field from a TurbSim .wnd binary format file.
	 * @param path TurbSim .wnd 文件路径。Path to the TurbSim .wnd file.
	 * @return 导入的 WindField 实例。Imported WindField instance.
	 * @throw std::runtime_error 当文件无法打开或格式不支持时抛出。
	 *                            Thrown when file cannot be opened or format is unsupported.
	 */
	static WindField ReadTurbSimWnd(const std::string &path);

	/**
	 * @brief 从 Bladed .wnd 二进制格式文件导入风场。
	 *        Import wind field from a Bladed .wnd binary format file.
	 * @param path  Bladed .wnd 文件路径。Path to the Bladed .wnd file.
	 * @param input 风场输入参数（用于计算统计量）。Wind input parameters (used for computing statistics).
	 * @return 导入的 WindField 实例。Imported WindField instance.
	 * @throw std::runtime_error 当文件无法打开或格式不支持时抛出。
	 *                            Thrown when file cannot be opened or format is unsupported.
	 * @note 优先从伴随 .sum 文件获取统计量，其次使用 input 参数计算。
	 *       Statistics are preferentially obtained from companion .sum file, then computed from input parameters.
	 */
	static WindField ReadBladedWnd(const std::string &path, const WindImportMetadata &metadata);

	/**
	 * @brief 根据文件扩展名和指定格式自动选择导入方法。
	 *        Automatically select import method based on file extension and specified format.
	 * @param path   风场文件路径。Path to the wind field file.
	 * @param format 期望的风场格式类型。Expected wind field format type.
	 * @param input  风场输入参数（仅 Bladed WND 需要）。Wind input parameters (required only for Bladed WND).
	 * @return 导入的 WindField 实例。Imported WindField instance.
	 * @throw std::runtime_error 当格式与扩展名组合不支持时抛出。
	 *                            Thrown when the format/extension combination is unsupported.
	 * @note .bts 扩展名使用 ReadBts；.wnd 扩展名根据 WndFormat 分派到 ReadBladedWnd 或 ReadTurbSimWnd。
	 *       .bts extension uses ReadBts; .wnd extension dispatches to ReadBladedWnd or ReadTurbSimWnd based on WndFormat.
	 */
	static WindField ReadAny(const std::string &path, WndFormat format, const WindImportMetadata &metadata = {});
};
