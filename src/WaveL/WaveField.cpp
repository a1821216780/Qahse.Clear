#include "WaveL/WaveField.hpp"

#include <algorithm>
#include <utility>

/**
 * @brief 从已有 WaveSpectrum 构造波浪场。
 *        Construct a wave field from an existing WaveSpectrum.
 *
 * 内部使用 std::move 转移频谱所有权，避免深拷贝波浪成分数据。构造后的 WaveField
 * 可直接用于运动学采样和环境查询。
 *
 * Uses std::move to transfer spectrum ownership, avoiding deep copy of wave component data.
 * The constructed WaveField is immediately ready for kinematics sampling and environment queries.
 *
 * @param spectrum 已生成的波浪频谱（将被移动）。Generated wave spectrum (will be moved).
 */
WaveField::WaveField(WaveSpectrum spectrum)
	: spectrum_(std::move(spectrum))
{
}

/**
 * @brief 获取指定位置的波浪运动学量，委托给内部 WaveSpectrum。
 *        Get wave kinematics at a specified location, delegates to internal WaveSpectrum.
 *
 * 内部根据 cacheBuilt_/importedExternalKinematics_ 标志自动选择求解路径：
 * - 缓存可用时：使用 EvaluateFromCache 进行四维线性插值
 * - 无缓存时：使用 Evaluate 直接在波浪成分上叠加计算
 *
 * Internally selects the solving path based on cacheBuilt_/importedExternalKinematics_ flags:
 * - With cache: uses EvaluateFromCache for 4D linear interpolation
 * - Without cache: uses Evaluate to superpose wave components directly
 */
WaveKinematics WaveField::GetKinematics(double x, double y, double z, double time) const
{
	return spectrum_.GetKinematics(x, y, z, time);
}

/**
 * @brief 获取自由表面高度，等价于 GetKinematics(x, y, 0, time).eta。
 *        Get free surface height, equivalent to GetKinematics(x, y, 0, time).eta.
 *
 * z=0 对应于静水面（SWL），eta 为瞬时波面相对于静水面的位移。
 *
 * z=0 corresponds to still water level (SWL); eta is the instantaneous surface displacement
 * relative to SWL.
 */
double WaveField::GetFreeSurfaceZ(double x, double y, double time) const
{
	return spectrum_.GetFreeSurfaceZ(x, y, time);
}

/**
 * @brief 判断空间点是否浸没于波浪中。
 *        Check whether a spatial point is submerged.
 *
 * 判断条件：z <= 自由表面高度 + tolerance。tolerance 用于考虑波浪飞溅区的湿润效应，
 * 默认 0 表示严格按静水面标准判断。
 *
 * Condition: z <= free surface height + tolerance. tolerance accounts for splash zone effects;
 * default 0 uses strict still-water criterion.
 */
bool WaveField::IsSubmerged(double x, double y, double z, double time, double tolerance) const
{
	return spectrum_.IsSubmerged(x, y, z, time, tolerance);
}

/**
 * @brief 判断空间点是否被波浪湿润。
 *        Check whether a spatial point is wetted.
 *
 * 当前实现直接委托给 IsSubmerged，两者语义等价。该接口为 API 兼容性保留，
 * 方便与风机载荷计算中的 IsWetted 概念对接。
 *
 * Currently delegates directly to IsSubmerged; both are semantically equivalent.
 * This interface is kept for API compatibility with wind turbine load calculation conventions.
 */
bool WaveField::IsWetted(double x, double y, double z, double time, double tolerance) const
{
	return IsSubmerged(x, y, z, time, tolerance);
}

/**
 * @brief 对单个空间点进行完整的波浪环境采样。
 *        Perform a complete wave environment sample at a single spatial point.
 *
 * 计算步骤：
 * 1. 调用 GetKinematics 获取运动学量（eta, vel, acc, dynP）
 * 2. 调用 GetFreeSurfaceZ 获取自由表面高度
 * 3. 计算浸没深度 = max(0, freeSurfaceZ + tolerance - z)
 * 4. 判断浸没状态：z <= freeSurfaceZ + tolerance
 *
 * Steps:
 * 1. Call GetKinematics for kinematics (eta, vel, acc, dynP)
 * 2. Call GetFreeSurfaceZ for free surface height
 * 3. Compute immersion depth = max(0, freeSurfaceZ + tolerance - z)
 * 4. Determine submersion: z <= freeSurfaceZ + tolerance
 */
WaveEnvironmentSample WaveField::SampleEnvironment(double x, double y, double z, double time, double tolerance) const
{
	WaveEnvironmentSample sample;
	sample.kinematics = GetKinematics(x, y, z, time);
	sample.freeSurfaceZ = GetFreeSurfaceZ(x, y, time);
	sample.immersionDepth = std::max(0.0, sample.freeSurfaceZ + tolerance - z);
	sample.submerged = z <= sample.freeSurfaceZ + tolerance;
	return sample;
}

/**
 * @brief 批量采样多个空间点的波浪环境。
 *        Batch sample wave environment at multiple spatial points.
 *
 * 对每个采样点依次调用单点采样，结果按相同顺序返回。使用 reserve 预分配内存
 * 以提升性能。
 *
 * Calls single-point sampling for each point sequentially; results are returned in the
 * same order. Uses reserve for memory pre-allocation to improve performance.
 */
std::vector<WaveEnvironmentSample> WaveField::SampleEnvironment(const std::vector<WaveSamplePoint> &points, double time, double tolerance) const
{
	std::vector<WaveEnvironmentSample> samples;
	samples.reserve(points.size());
	for (const auto &point : points)
		samples.push_back(SampleEnvironment(point.x, point.y, point.z, time, tolerance));
	return samples;
}

/**
 * @brief 仅校验输入参数的合法性，不执行后续计算。
 *        Validate input parameters only, without performing subsequent computations.
 *
 * 委托给 WaveSpectrum::ValidateInputOnly，该方法内部构造临时 WaveSpectrum 并调用
 * ValidateInput() 进行全面的参数检查。适用于 GUI 预检或批处理前的参数诊断。
 *
 * Delegates to WaveSpectrum::ValidateInputOnly, which internally constructs a temporary
 * WaveSpectrum and calls ValidateInput() for comprehensive parameter checking.
 * Suitable for GUI pre-check or parameter diagnostics before batch processing.
 */
void WaveField::ValidateInputOnly(const WaveLInput &input)
{
	WaveSpectrum::ValidateInputOnly(input);
}

/**
 * @brief 根据输入参数生成波浪场（工厂方法）。
 *        Generate a wave field from input parameters (factory method).
 *
 * 内部构造 WaveSpectrum、调用 Generate() 执行频谱合成、运动学缓存构建和输出文件写入，
 * 然后将生成的 WaveSpectrum 移动构造为 WaveField 返回。
 *
 * Internally constructs WaveSpectrum, calls Generate() for spectrum synthesis, kinematics cache
 * building, and output file writing, then move-constructs the generated WaveSpectrum into WaveField.
 */
WaveField WaveField::Generate(const WaveLInput &input, WaveProgressCallback progress)
{
	return WaveField(WaveSpectrum::Generate(input, progress));
}

/**
 * @brief 导入已有波浪数据（工厂方法）。
 *        Import existing wave data (factory method).
 *
 * 内部构造 WaveSpectrum 并调用 Import() 从缓存文件 (.wfc) 或波浪成分文件 (.wvc)
 * 加载已有数据。优先使用缓存导入（含运动学网格可直接插值）；若无缓存则从成分文件
 * 重建波浪成分列表。
 *
 * Internally constructs WaveSpectrum and calls Import() to load data from cache file (.wfc)
 * or wave component file (.wvc). Prefers cache import (with kinematics grid for direct
 * interpolation); falls back to rebuilding wave component list from component file.
 */
WaveField WaveField::Import(const WaveLInput &input, WaveProgressCallback progress)
{
	return WaveField(WaveSpectrum::Import(input, progress));
}

/**
 * @brief 从 .qoe 输入文件生成波浪场。
 *        Generate a wave field from a .qoe input file.
 *
 * 读取 .qoe 文件中的 WaveLInput 配置，检查模式为 GENERATE 后执行生成流程。
 *
 * Reads WaveLInput configuration from .qoe file, checks mode is GENERATE, then executes
 * the generation pipeline.
 */
WaveField WaveField::GenerateFromFile(const std::string &qoePath, WaveProgressCallback progress)
{
	return WaveField(WaveSpectrum::GenerateFromFile(qoePath, progress));
}

/**
 * @brief 从 .qoe 输入文件导入波浪场。
 *        Import a wave field from a .qoe input file.
 *
 * 读取 .qoe 文件中的 WaveLInput 配置，检查模式为 IMPORT 后执行导入流程。
 *
 * Reads WaveLInput configuration from .qoe file, checks mode is IMPORT, then executes
 * the import pipeline.
 */
WaveField WaveField::ImportFromFile(const std::string &qoePath, WaveProgressCallback progress)
{
	return WaveField(WaveSpectrum::ImportFromFile(qoePath, progress));
}
