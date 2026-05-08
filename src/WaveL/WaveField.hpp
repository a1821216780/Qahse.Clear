#pragma once

#include <string>
#include <vector>

#include "WaveL/WaveSpectrum.hpp"

/**
 * @brief 波浪场顶层封装类，提供波浪环境的生成、导入和时空采样接口。
 *        Top-level wave field wrapper class, providing wave environment generation, import,
 *        and spatio-temporal sampling interface.
 *
 * WaveField 是波浪模块的最外层 API，封装了 WaveSpectrum 的频谱生成和 WaveKinematicsEngine
 * 的运动学求解。用户通过此类创建波浪场、查询基本属性、并在任意位置和时间采样波浪环境数据
 * （波面高程、流体质点速度/加速度、动水压力、浸没状态）。
 *
 * WaveField is the outermost API of the wave module, encapsulating WaveSpectrum's spectrum
 * generation and WaveKinematicsEngine's kinematics solving. Users create wave fields, query
 * basic properties, and sample wave environment data (surface elevation, fluid particle
 * velocity/acceleration, dynamic pressure, submersion state) at arbitrary locations and times.
 */
class WaveField
{
public:
	/**
	 * @brief 默认构造，创建一个空波浪场。
	 *        Default constructor, creates an empty wave field.
	 */
	WaveField() = default;

	/**
	 * @brief 从已有 WaveSpectrum 构造波浪场（用于内部工厂方法）。
	 *        Construct a wave field from an existing WaveSpectrum (used by internal factory methods).
	 * @param spectrum 已生成的波浪频谱。Generated wave spectrum.
	 */
	explicit WaveField(WaveSpectrum spectrum);

	const WaveLInput &GetInput() const { return spectrum_.GetInput(); }            ///< 获取输入参数。Get input parameters.
	const WaveSpectrumResult &GetResult() const { return spectrum_.GetResult(); }   ///< 获取频谱结果。Get spectrum result.
	const std::vector<WaveTrain> &GetWaveTrains() const { return spectrum_.GetWaveTrains(); }  ///< 获取波浪成分列表。Get wave component list.
	double GetHs() const { return spectrum_.GetHs(); }        ///< 获取有效波高 Hs [m]。Get significant wave height Hs [m].
	double GetTp() const { return spectrum_.GetTp(); }        ///< 获取谱峰周期 Tp [s]。Get peak spectral period Tp [s].
	double GetFp() const { return spectrum_.GetFp(); }        ///< 获取谱峰频率 fp [Hz]。Get peak spectral frequency fp [Hz].
	double GetDepth() const { return spectrum_.GetDepth(); }  ///< 获取水深 [m]。Get water depth [m].

	/**
	 * @brief 在指定空间位置和时间获取波浪运动学量（高程、速度、加速度、动水压力）。
	 *        Get wave kinematics (elevation, velocity, acceleration, dynamic pressure) at a
	 *        specified spatial location and time.
	 * @param x    X 坐标（顺浪向）[m]。X coordinate (down-wave) [m].
	 * @param y    Y 坐标（横向）[m]。Y coordinate (lateral) [m].
	 * @param z    Z 坐标（垂向，向上为正）[m]。Z coordinate (vertical, positive upward) [m].
	 * @param time 模拟时间 [s]。Simulation time [s].
	 * @return     WaveKinematics 结构，含 eta、vel、acc、dynP。WaveKinematics struct with eta, vel, acc, dynP.
	 * @note 优先使用缓存插值（若已构建）；否则通过规则波叠加直接计算。
	 *       Prefers cache interpolation if built; otherwise computes via regular wave superposition.
	 */
	WaveKinematics GetKinematics(double x, double y, double z, double time) const;

	/**
	 * @brief 获取指定水平位置和时刻的自由表面高度。
	 *        Get free surface height at a specified horizontal location and time.
	 * @param x    X 坐标 [m]。X coordinate [m].
	 * @param y    Y 坐标 [m]。Y coordinate [m].
	 * @param time 模拟时间 [s]。Simulation time [s].
	 * @return     自由表面高度 [m]。Free surface height [m].
	 * @note 等价于 GetKinematics(x, y, 0, time).eta。
	 *       Equivalent to GetKinematics(x, y, 0, time).eta.
	 */
	double GetFreeSurfaceZ(double x, double y, double time) const;

	/**
	 * @brief 判断指定空间位置在指定时刻是否浸没于波浪中。
	 *        Check whether a specified spatial location is submerged at a given time.
	 * @param x         X 坐标 [m]。X coordinate [m].
	 * @param y         Y 坐标 [m]。Y coordinate [m].
	 * @param z         Z 坐标 [m]。Z coordinate [m].
	 * @param time      模拟时间 [s]。Simulation time [s].
	 * @param tolerance 浸没容差 [m]（默认 0，用于考虑飞溅区）。Submersion tolerance [m] (default 0, for splash zone).
	 * @return          true 表示 z <= freeSurfaceZ + tolerance。true if z <= freeSurfaceZ + tolerance.
	 */
	bool IsSubmerged(double x, double y, double z, double time, double tolerance = 0.0) const;

	/**
	 * @brief 判断指定空间位置在指定时刻是否被波浪湿润（当前等同于 IsSubmerged）。
	 *        Check whether a specified spatial location is wetted at a given time (currently equals IsSubmerged).
	 * @note 为 API 兼容性保留的别名，实现与 IsSubmerged 相同。
	 *       Alias kept for API compatibility; implementation is identical to IsSubmerged.
	 */
	bool IsWetted(double x, double y, double z, double time, double tolerance = 0.0) const;

	/**
	 * @brief 对单个空间点和时刻进行完整的波浪环境采样。
	 *        Perform a complete wave environment sample at a single spatial point and time.
	 * @param x         X 坐标 [m]。X coordinate [m].
	 * @param y         Y 坐标 [m]。Y coordinate [m].
	 * @param z         Z 坐标 [m]。Z coordinate [m].
	 * @param time      模拟时间 [s]。Simulation time [s].
	 * @param tolerance 浸没容差 [m]。Submersion tolerance [m].
	 * @return          WaveEnvironmentSample 结构，含运动学、自由表面高度、浸没深度和浸没状态。
	 *                 WaveEnvironmentSample struct with kinematics, free surface height, immersion depth, and submersion state.
	 */
	WaveEnvironmentSample SampleEnvironment(double x, double y, double z, double time, double tolerance = 0.0) const;

	/**
	 * @brief 对多个空间点在指定时刻批量进行波浪环境采样。
	 *        Batch sample wave environment at multiple spatial points for a given time.
	 * @param points    采样点列表。List of sample points.
	 * @param time      模拟时间 [s]。Simulation time [s].
	 * @param tolerance 浸没容差 [m]。Submersion tolerance [m].
	 * @return          各采样点的 WaveEnvironmentSample 结果向量。Vector of WaveEnvironmentSample results for each point.
	 */
	std::vector<WaveEnvironmentSample> SampleEnvironment(const std::vector<WaveSamplePoint> &points, double time, double tolerance = 0.0) const;

	/**
	 * @brief 仅校验输入参数，不执行波浪场生成（用于参数预检）。
	 *        Validate input parameters only, without generating a wave field (for parameter pre-check).
	 * @param input 波浪输入参数。Wave input parameters.
	 * @throw std::runtime_error 当参数非法时抛出。Thrown when parameters are invalid.
	 */
	static void ValidateInputOnly(const WaveLInput &input);

	/**
	 * @brief 根据输入参数生成波浪场（工厂方法，自动选择频谱模型和时间序列）。
	 *        Generate a wave field from input parameters (factory method, auto-selects spectrum model and time series).
	 * @param input    波浪输入参数。Wave input parameters.
	 * @param progress 进度回调（可选）。Progress callback (optional).
	 * @return         生成的 WaveField 实例。Generated WaveField instance.
	 */
	static WaveField Generate(const WaveLInput &input, WaveProgressCallback progress = {});

	/**
	 * @brief 导入已有波浪数据（工厂方法，支持从缓存或成分文件导入）。
	 *        Import existing wave data (factory method, supports import from cache or component file).
	 * @param input    波浪输入参数（含导入路径）。Wave input parameters (with import paths).
	 * @param progress 进度回调（可选）。Progress callback (optional).
	 * @return         导入的 WaveField 实例。Imported WaveField instance.
	 */
	static WaveField Import(const WaveLInput &input, WaveProgressCallback progress = {});

	/**
	 * @brief 从 .qoe 文件读取输入参数并生成波浪场。
	 *        Read input parameters from a .qoe file and generate a wave field.
	 * @param qoePath  .qoe 输入文件路径。Path to .qoe input file.
	 * @param progress 进度回调（可选）。Progress callback (optional).
	 * @return         生成的 WaveField 实例。Generated WaveField instance.
	 * @throw std::runtime_error 当模式非 GENERATE 时抛出。Thrown when mode is not GENERATE.
	 */
	static WaveField GenerateFromFile(const std::string &qoePath, WaveProgressCallback progress = {});

	/**
	 * @brief 从 .qoe 文件读取输入参数并导入波浪场。
	 *        Read input parameters from a .qoe file and import a wave field.
	 * @param qoePath  .qoe 输入文件路径。Path to .qoe input file.
	 * @param progress 进度回调（可选）。Progress callback (optional).
	 * @return         导入的 WaveField 实例。Imported WaveField instance.
	 * @throw std::runtime_error 当模式非 IMPORT 时抛出。Thrown when mode is not IMPORT.
	 */
	static WaveField ImportFromFile(const std::string &qoePath, WaveProgressCallback progress = {});

private:
	WaveSpectrum spectrum_;  ///< 内部波浪频谱实例。Internal wave spectrum instance.
};
