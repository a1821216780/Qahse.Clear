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
// 该文件实现了 WindField 结构体，提供三维湍流风场的数据存储、坐标构建、统计量计算
// 和文件导入导出功能（BTS、Bladed WND、TurbSim WND 格式）。
//
// ──────────────────────────────────────────────────────────────────────────────

#include "WindL/WindField.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <numeric>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "Math/InterpolateHelper.hpp"

namespace
{
constexpr double kTiny = 1.0e-12;
constexpr double kPi = 3.141592653589793238462643383279502884;

/**
 * @brief 从二进制输入流中读取一个标量值（POD 类型）。
 *        Read a scalar value (POD type) from a binary input stream.
 * @tparam T 要读取的类型（如 int, float, double）。Type to read (e.g., int, float, double).
 * @param in 二进制输入流引用，流须处于正常状态。Binary input stream reference; stream must be in good state.
 * @return  读取的标量值。The read scalar value.
 * @throw std::runtime_error 当读取未完整时抛出（如文件截断）。
 *                            Thrown when reading is incomplete (e.g., file truncated).
 * @code
 * int value = ReadScalar<int>(stream);
 * @endcode
 */
template <typename T>
T ReadScalar(std::ifstream &in)
{
	T value{};
	in.read(reinterpret_cast<char *>(&value), sizeof(T));
	if (!in)
		throw std::runtime_error("Unexpected end of binary wind file");
	return value;
}

/**
 * @brief 将数值钳制到 [lower, upper] 区间。
 *        Clamp a value to the interval [lower, upper].
 * @param value 待钳制的数值。Value to clamp.
 * @param lower 下界。Lower bound.
 * @param upper 上界。Upper bound.
 * @return     钳制后的数值。Clamped value.
 * @code
 * double v = Clamp(x, 0.0, 1.0);
 * @endcode
 */
double Clamp(double value, double lower, double upper)
{
	return std::max(lower, std::min(upper, value));
}

/**
 * @brief 对 Y 坐标应用镜像边界条件，实现周期性反射。
 *        Apply mirroring boundary condition to a Y coordinate for periodic reflection.
 * @param value 待反射的坐标值。Coordinate value to mirror.
 * @param lower 网格下界。Grid lower bound.
 * @param upper 网格上界。Grid upper bound.
 * @return     反射后的坐标值（落在 [lower, upper] 内）。Mirrored coordinate value (within [lower, upper]).
 * @note 使用周期性反射公式：若 value 超出 [lower, upper]，则以 2×width 为周期折叠回区间内。
 *       Uses periodic reflection: if value exceeds [lower, upper], it is folded back with period 2×width.
 * @code
 * double y = MirrorCoordinate(12.0, -5.0, 5.0);
 * @endcode
 */
double MirrorCoordinate(double value, double lower, double upper)
{
	if (upper <= lower + kTiny)
		return lower;

	const double width = upper - lower;
	const double period = 2.0 * width;
	double shifted = std::fmod(value - lower, period);
	if (shifted < 0.0)
		shifted += period;
	if (shifted > width)
		shifted = period - shifted;
	return lower + shifted;
}

/**
 * @brief 对时间坐标进行归一化处理，支持直接钳制或镜像循环两种模式。
 *        Normalize time coordinate with either clamping or mirror-cycling mode.
 * @param value  待归一化的时间值 [s]。Time value to normalize [s].
 * @param upper  时间上界（通常为总时长）[s]。Upper time bound (typically total duration) [s].
 * @param mirror 是否启用镜像循环模式。Whether to enable mirror-cycling mode.
 * @return       归一化后的时间值（落入有效区间）。Normalized time value (within valid range).
 * @throw std::runtime_error 当 mirror=false 且 value 超界时抛出。
 *                            Thrown when mirror=false and value is out of range.
 * @note mirror=true 时使用与 MirrorCoordinate 相同的镜像算法；
 *       mirror=false 时直接钳制到 [0, upper]。
 *       When mirror=true, uses the same mirroring algorithm as MirrorCoordinate;
 *       when mirror=false, clamps directly to [0, upper].
 * @code
 * double t = NormalizeTime(15.0, 10.0, true);
 * @endcode
 */
double NormalizeTime(double value, double upper, bool mirror)
{
	if (upper <= 0.0)
		return 0.0;
	if (!mirror)
	{
		if (value < 0.0 || value > upper + kTiny)
			throw std::runtime_error("Requested sample time exceeds the imported wind duration and CycleWind=false");
		return Clamp(value, 0.0, upper);
	}

	const double period = 2.0 * upper;
	double shifted = std::fmod(value, period);
	if (shifted < 0.0)
		shifted += period;
	if (shifted > upper)
		shifted = period - shifted;
	return shifted;
}

/**
 * @brief 在有序坐标序列中定位目标值的区间：二分查找括号化。
 *        Locate the bracket interval for a target value in a sorted coordinate sequence
 *        via binary search.
 * @param coords 有序坐标序列，须单调递增。Sorted coordinate sequence, must be monotonically increasing.
 * @param value  待定位的目标值。Target value to locate.
 * @return       三元组 (i0, i1, alpha)，其中 i0/i1 为左右索引，alpha 为插值权重。
 *               Tuple (i0, i1, alpha), where i0/i1 are left/right indices and alpha is the interpolation weight.
 * @throw std::runtime_error 当坐标序列为空时抛出。Thrown when coordinate sequence is empty.
 * @note 边界情况：value 在范围外时，alpha 为 0 或 1，索引指向边界区间。
 *       Edge cases: when value is out of range, alpha is 0 or 1 with indices pointing to boundary interval.
 * @code
 * auto [i0, i1, a] = Bracket(coords, 5.0);
 * @endcode
 */
std::tuple<int, int, double> Bracket(const std::vector<double> &coords, double value)
{
	if (coords.empty())
		throw std::runtime_error("Imported wind field is missing coordinates");
	if (coords.size() == 1)
		return {0, 0, 0.0};
	if (value <= coords.front())
		return {0, 1, 0.0};
	if (value >= coords.back())
		return {static_cast<int>(coords.size()) - 2, static_cast<int>(coords.size()) - 1, 1.0};

	const auto upper = std::upper_bound(coords.begin(), coords.end(), value);
	const int i1 = static_cast<int>(std::distance(coords.begin(), upper));
	const int i0 = i1 - 1;
	const double span = std::max(coords[static_cast<std::size_t>(i1)] - coords[static_cast<std::size_t>(i0)], kTiny);
	const double alpha = (value - coords[static_cast<std::size_t>(i0)]) / span;
	return {i0, i1, alpha};
}

/**
 * @brief 根据风场输入参数与回退平均风速计算三个速度分量的均值向量。
 *        Compute the mean velocity vector for three components from wind input parameters
 *        and a fallback mean wind speed.
 * @param input             风场输入参数。Wind input parameters.
 * @param fallbackMeanWind  回退平均风速（当 input.meanWindSpeed <= 0 时使用）[m/s]。
 *                          Fallback mean wind speed (used when input.meanWindSpeed <= 0) [m/s].
 * @return                  三分量均值 (u, v, w) [m/s]。Three-component mean (u, v, w) [m/s].
 * @note 根据水平入流角 horAngle、垂直入流角 vertAngle 和平均风速进行向量分解。
 *       Decomposes the mean wind speed vector using horizontal/vertical inflow angles.
 * @code
 * auto means = MeansFromInput(input, 10.0);
 * @endcode
 */
std::array<double, 3> MeansFromMetadata(const WindImportMetadata &metadata, double fallbackMeanWind)
{
	const double speed = metadata.meanWindSpeed > 0.0 ? metadata.meanWindSpeed : fallbackMeanWind;
	const double h = metadata.horAngle * kPi / 180.0;
	const double v = metadata.vertAngle * kPi / 180.0;
	const double cosV = std::cos(v);
	return {
	    speed * cosV * std::cos(h),
	    speed * cosV * std::sin(h),
	    speed * std::sin(v)};
}

/**
 * @brief 将湍流强度值归一化为小数形式（原始为百分比时除以 100）。
 *        Normalize turbulence intensity value to fractional form (divide by 100 if originally in percent).
 * @param ti 湍流强度，百分比（>1）或小数（<=1）。Turbulence intensity, in percent (>1) or fractional (<=1).
 * @return  小数形式的湍流强度；ti <= 0 返回 0。Fractional turbulence intensity; returns 0 if ti <= 0.
 * @code
 * double ti = NormalizeTiValue(12.0); // 返回 0.12
 * @endcode
 */
double NormalizeTiValue(double ti)
{
	if (ti <= 0.0)
		return 0.0;
	return ti > 1.0 ? ti / 100.0 : ti;
}

/**
 * @brief 伴随 .sum 统计摘要文件的数据结构。
 *        Data structure for companion .sum statistical summary file.
 */
struct CompanionSummary
{
	double hubHeight = 0.0;                      ///< 轮毂高度 [m]。Hub height [m].
	bool hasHubHeight = false;                   ///< 是否成功解析到轮毂高度。Whether hub height was successfully parsed.
	std::array<double, 3> mean{0.0, 0.0, 0.0};   ///< 三个分量的均值 (u, v, w) [m/s]。Three-component mean (u, v, w) [m/s].
	std::array<double, 3> sigma{0.0, 0.0, 0.0};  ///< 三个分量的标准差 (u, v, w) [m/s]。Three-component standard deviation (u, v, w) [m/s].
	bool hasStats = false;                       ///< 是否成功解析到全部统计量。Whether all statistics were successfully parsed.
};

/**
 * @brief 解析伴随 .sum 文件，提取轮毂高度和统计量信息。
 *        Parse a companion .sum file and extract hub height and statistical information.
 * @param path .sum 文件路径。Path to the .sum file.
 * @return    包含解析结果的 optional；文件不存在或无法解析时返回 nullopt。
 *            optional with parse results; returns nullopt if file does not exist or cannot be parsed.
 * @note 解析通过正则表达式匹配 "HubHt:"、"u: mean=... sigma=..." 等模式。
 *       Parsing uses regex to match patterns like "HubHt:", "u: mean=... sigma=...", etc.
 * @code
 * auto summary = ParseCompanionSummary("output.sum");
 * @endcode
 */
std::optional<CompanionSummary> ParseCompanionSummary(const std::filesystem::path &path)
{
	if (!std::filesystem::is_regular_file(path))
		return std::nullopt;

	std::ifstream in(path);
	if (!in)
		return std::nullopt;

	std::ostringstream buffer;
	buffer << in.rdbuf();
	const std::string text = buffer.str();

	CompanionSummary summary;
	std::smatch match;

	const std::regex hubRegex(R"(HubHt:\s*([-+0-9.eE]+))");
	if (std::regex_search(text, match, hubRegex))
	{
		summary.hubHeight = std::stod(match[1].str());
		summary.hasHubHeight = true;
	}

	const std::array<std::regex, 3> statRegex{
	    std::regex(R"(\bu:\s*mean=([-+0-9.eE]+)\s+sigma=([-+0-9.eE]+))"),
	    std::regex(R"(\bv:\s*mean=([-+0-9.eE]+)\s+sigma=([-+0-9.eE]+))"),
	    std::regex(R"(\bw:\s*mean=([-+0-9.eE]+)\s+sigma=([-+0-9.eE]+))")};

	bool allStats = true;
	for (int comp = 0; comp < 3; ++comp)
	{
		if (!std::regex_search(text, match, statRegex[static_cast<std::size_t>(comp)]))
		{
			allStats = false;
			break;
		}
		summary.mean[static_cast<std::size_t>(comp)] = std::stod(match[1].str());
		summary.sigma[static_cast<std::size_t>(comp)] = std::stod(match[2].str());
	}
	summary.hasStats = allStats;

	if (!summary.hasHubHeight && !summary.hasStats)
		return std::nullopt;
	return summary;
}

/**
 * @brief 确定 Bladed WND 导入时各分量的标准差。
 *        Resolve component standard deviations for Bladed WND import.
 * @param record2   Bladed WND 记录类型标识。Bladed WND record type identifier.
 * @param meanWind  平均风速 [m/s]。Mean wind speed [m/s].
 * @param tiPercent 各分量湍流强度（百分比形式）。Per-component turbulence intensity (in percent).
 * @param input     风场输入参数。Wind input parameters.
 * @param companion 伴随 .sum 文件解析结果。Companion .sum file parse result.
 * @return          三个分量的标准差 (u, v, w) [m/s]。Three-component standard deviations (u, v, w) [m/s].
 * @throw std::runtime_error 当无法确定标准差时抛出。Thrown when standard deviations cannot be determined.
 * @note 优先级：companion .sum 统计量 > record2==4 时从 TI 百分比计算 > 用户显式 TI 输入。
 *       Priority: companion .sum statistics > calculated from TI percent when record2==4 > user explicit TI input.
 * @code
 * auto sigmas = ResolveBladedSigma(4, 10.0, tiPct, input, companion);
 * @endcode
 */
std::array<double, 3> ResolveBladedSigma(
	int record2,
	double meanWind,
	const std::array<float, 3> &tiPercent,
	const WindImportMetadata &metadata,
	const std::optional<CompanionSummary> &companion)
{
	if (companion && companion->hasStats)
		return companion->sigma;

	if (record2 == 4)
	{
		return {
		    meanWind * tiPercent[0] / 100.0,
		    meanWind * tiPercent[1] / 100.0,
		    meanWind * tiPercent[2] / 100.0};
	}

	const std::array<double, 3> explicitTi{
	    NormalizeTiValue(metadata.tiU),
	    NormalizeTiValue(metadata.tiV),
	    NormalizeTiValue(metadata.tiW)};

	if (explicitTi[0] > 0.0 && explicitTi[1] > 0.0 && explicitTi[2] > 0.0)
	{
		return {
		    meanWind * explicitTi[0],
		    meanWind * explicitTi[1],
		    meanWind * explicitTi[2]};
	}

	throw std::runtime_error("Bladed WND import requires a companion .sum file or explicit component TI metadata");
}

/**
 * @brief 对 BTS 格式原始 int16 值进行解码，转换为物理风速值。
 *        Decode a raw BTS int16 value into a physical wind speed.
 * @param raw    原始 int16 编码值。Raw int16 encoded value.
 * @param slope  BTS 解码斜率（scaling factor）。BTS decoding slope (scaling factor).
 * @param offset BTS 解码偏移量。BTS decoding offset.
 * @return       解码后的物理风速值。Decoded physical wind speed value.
 * @note 解码公式：value = (raw - offset) / slope；当 slope 趋于零时直接返回 raw。
 *       Decoding formula: value = (raw - offset) / slope; returns raw directly when slope ≈ 0.
 * @code
 * double v = DecodeBtsValue(rawVal, 10.0f, 0.0f);
 * @endcode
 */
double DecodeBtsValue(std::int16_t raw, float slope, float offset)
{
	if (std::abs(slope) <= static_cast<float>(kTiny))
		return static_cast<double>(raw);
	return (static_cast<double>(raw) - static_cast<double>(offset)) / static_cast<double>(slope);
}

/**
 * @brief 在风场的单个时间片上进行 Y-Z 平面双线性插值采样。
 *        Perform bilinear interpolation sampling on the Y-Z plane at a single time slice
 *        of the wind field.
 * @param field 风场引用。Wind field reference.
 * @param comp  速度分量索引：0=u, 1=v, 2=w。Component index: 0=u, 1=v, 2=w.
 * @param step  时间步索引。Time step index.
 * @param y     目标 Y 坐标 [m]。Target Y coordinate [m].
 * @param z     目标 Z 坐标 [m]。Target Z coordinate [m].
 * @return      插值后的风速值。Interpolated wind speed value.
 * @note 使用 Bracket() 定位插值区间，先沿 Y 方向线性插值，再沿 Z 方向线性插值。
 *       Uses Bracket() to locate interpolation intervals; linearly interpolates first along Y, then along Z.
 * @code
 * double val = SamplePlaneLinear(field, 0, 10, 3.5, 90.0);
 * @endcode
 */
double SamplePlaneLinear(const WindField &field, int comp, int step, double y, double z)
{
	const auto [iy0, iy1, ay] = Bracket(field.yCoords, y);
	const auto [iz0, iz1, az] = Bracket(field.zCoords, z);

	const double v00 = field.At(comp, step, iz0, iy0);
	const double v01 = field.At(comp, step, iz0, iy1);
	const double v10 = field.At(comp, step, iz1, iy0);
	const double v11 = field.At(comp, step, iz1, iy1);
	const double low = v00 * (1.0 - ay) + v01 * ay;
	const double high = v10 * (1.0 - ay) + v11 * ay;
	return low * (1.0 - az) + high * az;
}

/**
 * @brief 在风场的单个时间片上进行 Y-Z 平面三次样条插值采样。
 *        Perform bicubic spline interpolation sampling on the Y-Z plane at a single time
 *        slice of the wind field.
 * @param field 风场引用。Wind field reference.
 * @param comp  速度分量索引：0=u, 1=v, 2=w。Component index: 0=u, 1=v, 2=w.
 * @param step  时间步索引。Time step index.
 * @param y     目标 Y 坐标 [m]。Target Y coordinate [m].
 * @param z     目标 Z 坐标 [m]。Target Z coordinate [m].
 * @return      插值后的风速值。Interpolated wind speed value.
 * @note 网格不足 4×4 时回退到双线性插值；否则先沿 Y 方向逐行三次样条插值，
 *       再沿 Z 方向三次样条插值。Uses InterpolateHelper::Interp1D 进行一维三次样条。
 *       Falls back to bilinear when grid is smaller than 4×4; otherwise performs cubic spline
 *       row-by-row along Y, then along Z. Uses InterpolateHelper::Interp1D for 1D cubic spline.
 * @code
 * double val = SamplePlaneCubic(field, 0, 10, 3.5, 90.0);
 * @endcode
 */
double SamplePlaneCubic(const WindField &field, int comp, int step, double y, double z)
{
	if (field.ny < 4 || field.nz < 4)
		return SamplePlaneLinear(field, comp, step, y, z);

	std::vector<double> byZ(static_cast<std::size_t>(field.nz), 0.0);
	std::vector<double> row(static_cast<std::size_t>(field.ny), 0.0);
	for (int iz = 0; iz < field.nz; ++iz)
	{
		for (int iy = 0; iy < field.ny; ++iy)
			row[static_cast<std::size_t>(iy)] = field.At(comp, step, iz, iy);
		byZ[static_cast<std::size_t>(iz)] =
		    InterpolateHelper::Interp1D(field.yCoords, row, y, InterpolateHelper::Interp1DType::CubicSpline);
	}
	return InterpolateHelper::Interp1D(field.zCoords, byZ, z, InterpolateHelper::Interp1DType::CubicSpline);
}

/**
 * @brief 检查导入风场与输入参数的一致性，发现不匹配时添加警告。
 *        Check consistency between imported wind field and input parameters,
 *        appending warnings on mismatch.
 * @param field 导入的风场（将向其 warnings 追加警告）。Imported wind field (warnings will be appended to it).
 * @param input 用户输入参数（含预期值）。User input parameters (with expected values).
 * @note 检查项包括：ny、nz、dt、hubHeight、meanWindSpeed。
 *       检查使用阈值容差，以避免浮点舍入误差导致的误报。
 *       Checks include: ny, nz, dt, hubHeight, meanWindSpeed.
 *       Threshold tolerances are used to avoid false positives from floating-point rounding.
 */
void WarnOnMismatch(WindField &field, const WindImportMetadata &metadata)
{
	if (metadata.expectedNy > 0 && metadata.expectedNy != field.ny)
		field.warnings.push_back("Imported file ny does not match NumPointY; using file header value.");
	if (metadata.expectedNz > 0 && metadata.expectedNz != field.nz)
		field.warnings.push_back("Imported file nz does not match NumPointZ; using file header value.");
	if (metadata.expectedDt > 0.0 && std::abs(metadata.expectedDt - field.dt) > 1.0e-9)
		field.warnings.push_back("Imported file dt does not match TimeStep; using file header value.");
	if (metadata.hubHeight > 0.0 && field.hubHeight > 0.0 && std::abs(metadata.hubHeight - field.hubHeight) > 1.0e-6)
		field.warnings.push_back("Imported file hub height does not match HubHt; using imported metadata.");
	if (metadata.meanWindSpeed > 0.0 && field.meanWindSpeed > 0.0 && std::abs(metadata.meanWindSpeed - field.meanWindSpeed) > 1.0e-6)
		field.warnings.push_back("Imported file mean wind does not match MeanWindSpeed; using imported metadata.");
}
} // namespace

void WindField::Resize(int steps, int nyPoints, int nzPoints)
{
	nSteps = steps;
	ny = nyPoints;
	nz = nzPoints;
	nPoints = ny * nz;
	for (auto &values : component)
		values.assign(static_cast<std::size_t>(nSteps) * static_cast<std::size_t>(nPoints), 0.0);
}

void WindField::BuildCoordinates()
{
	fieldDimY = ny > 1 ? dy * static_cast<double>(ny - 1) : 0.0;
	fieldDimZ = nz > 1 ? dz * static_cast<double>(nz - 1) : 0.0;

	yCoords.resize(static_cast<std::size_t>(ny));
	zCoords.resize(static_cast<std::size_t>(nz));
	timeCoords.resize(static_cast<std::size_t>(nSteps));

	for (int iy = 0; iy < ny; ++iy)
		yCoords[static_cast<std::size_t>(iy)] = ny > 1 ? (-0.5 * fieldDimY + dy * static_cast<double>(iy)) : 0.0;
	for (int iz = 0; iz < nz; ++iz)
	{
		if (nz > 1)
			zCoords[static_cast<std::size_t>(iz)] = zBottom + dz * static_cast<double>(iz);
		else if (hubHeight > 0.0)
			zCoords[static_cast<std::size_t>(iz)] = hubHeight;
		else
			zCoords[static_cast<std::size_t>(iz)] = zBottom;
	}
	for (int step = 0; step < nSteps; ++step)
		timeCoords[static_cast<std::size_t>(step)] = dt * static_cast<double>(step);
}

void WindField::ComputeStats()
{
	for (int comp = 0; comp < 3; ++comp)
	{
		const auto &values = component[static_cast<std::size_t>(comp)];
		if (values.empty())
			continue;

		const double sum = std::accumulate(values.begin(), values.end(), 0.0);
		const double avg = sum / static_cast<double>(values.size());
		double variance = 0.0;
		for (double value : values)
		{
			const double delta = value - avg;
			variance += delta * delta;
		}
		variance /= static_cast<double>(values.size());
		mean[static_cast<std::size_t>(comp)] = avg;
		sigma[static_cast<std::size_t>(comp)] = std::sqrt(std::max(variance, 0.0));
		turbulenceIntensity[static_cast<std::size_t>(comp)] =
		    meanWindSpeed > kTiny ? sigma[static_cast<std::size_t>(comp)] / meanWindSpeed : 0.0;
	}
}

int WindField::GridIndex(int iz, int iy) const
{
	return iz * ny + iy;
}

double &WindField::At(int comp, int step, int point)
{
	return component[static_cast<std::size_t>(comp)][static_cast<std::size_t>(step) * static_cast<std::size_t>(nPoints) + static_cast<std::size_t>(point)];
}

double WindField::At(int comp, int step, int point) const
{
	return component[static_cast<std::size_t>(comp)][static_cast<std::size_t>(step) * static_cast<std::size_t>(nPoints) + static_cast<std::size_t>(point)];
}

double &WindField::At(int comp, int step, int iz, int iy)
{
	return At(comp, step, GridIndex(iz, iy));
}

double WindField::At(int comp, int step, int iz, int iy) const
{
	return At(comp, step, GridIndex(iz, iy));
}

std::array<double, 3> WindField::Sample(double y, double z, double t, InterpMethod method, bool cycleWind) const
{
	if (nSteps <= 0 || ny <= 0 || nz <= 0)
		throw std::runtime_error("Imported wind field is empty");

	const double sampleY = MirrorCoordinate(y, yCoords.front(), yCoords.back());
	const double sampleZ = Clamp(z, zCoords.front(), zCoords.back());
	const double maxTime = timeCoords.empty() ? 0.0 : timeCoords.back();
	const double sampleT = NormalizeTime(t, maxTime, cycleWind);
	const auto [t0, t1, at] = Bracket(timeCoords, sampleT);

	std::array<double, 3> result{};
	for (int comp = 0; comp < 3; ++comp)
	{
		const double v0 = method == InterpMethod::CUBIC ? SamplePlaneCubic(*this, comp, t0, sampleY, sampleZ)
		                                                : SamplePlaneLinear(*this, comp, t0, sampleY, sampleZ);
		if (t0 == t1)
		{
			result[static_cast<std::size_t>(comp)] = v0;
			continue;
		}
		const double v1 = method == InterpMethod::CUBIC ? SamplePlaneCubic(*this, comp, t1, sampleY, sampleZ)
		                                                : SamplePlaneLinear(*this, comp, t1, sampleY, sampleZ);
		result[static_cast<std::size_t>(comp)] = v0 * (1.0 - at) + v1 * at;
	}
	return result;
}

std::array<double, 3> WindField::SampleAt(double x, double y, double z, double t, const WindVelocityOptions &options) const
{
	if (nSteps <= 0 || ny <= 0 || nz <= 0)
		throw std::runtime_error("Imported wind field is empty");

	double sampleT = t;
	if (options.autoFieldShift && meanWindSpeed > kTiny)
		sampleT += (0.5 * fieldDimY - x) / meanWindSpeed;
	sampleT += options.shiftTime;
	if (sampleT < 0.0)
		sampleT = 0.0;

	const double maxTime = timeCoords.empty() ? 0.0 : timeCoords.back();
	if (options.mirrorTime)
	{
		sampleT = NormalizeTime(sampleT, maxTime, true);
	}
	else if (options.cycleWind)
	{
		if (maxTime > 0.0)
		{
			sampleT = std::fmod(sampleT, maxTime);
			if (sampleT < 0.0)
				sampleT += maxTime;
		}
		else
		{
			sampleT = 0.0;
		}
	}
	else
	{
		sampleT = NormalizeTime(sampleT, maxTime, false);
	}

	return Sample(y, z, sampleT, options.interpMethod, false);
}

WindField WindField::ReadBts(const std::string &path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		throw std::runtime_error("Cannot open BTS import file: " + path);

	WindField field;
	field.wndFormat = WndFormat::TURBSIM_BTS;
	field.sourcePath = std::filesystem::path(path);

	const std::int16_t fileId = ReadScalar<std::int16_t>(in);
	if (fileId != 7 && fileId != 8)
		throw std::runtime_error("Unsupported BTS file identifier");

	field.nz = ReadScalar<std::int32_t>(in);
	field.ny = ReadScalar<std::int32_t>(in);
	(void)ReadScalar<std::int32_t>(in);
	field.nSteps = ReadScalar<std::int32_t>(in);
	field.dz = ReadScalar<float>(in);
	field.dy = ReadScalar<float>(in);
	field.dt = ReadScalar<float>(in);
	field.meanWindSpeed = ReadScalar<float>(in);
	field.hubHeight = ReadScalar<float>(in);
	field.zBottom = ReadScalar<float>(in);

	std::array<float, 3> slope{};
	std::array<float, 3> offset{};
	for (int comp = 0; comp < 3; ++comp)
	{
		slope[static_cast<std::size_t>(comp)] = ReadScalar<float>(in);
		offset[static_cast<std::size_t>(comp)] = ReadScalar<float>(in);
	}

	const auto descLength = ReadScalar<std::int32_t>(in);
	if (descLength > 0)
		in.ignore(descLength);

	field.Resize(field.nSteps, field.ny, field.nz);
	field.BuildCoordinates();

	for (int step = 0; step < field.nSteps; ++step)
	{
		for (int iz = 0; iz < field.nz; ++iz)
		{
			for (int iy = 0; iy < field.ny; ++iy)
			{
				for (int comp = 0; comp < 3; ++comp)
				{
					const auto raw = ReadScalar<std::int16_t>(in);
					field.At(comp, step, iz, iy) = DecodeBtsValue(raw, slope[static_cast<std::size_t>(comp)], offset[static_cast<std::size_t>(comp)]);
				}
			}
		}
	}

	field.ComputeStats();
	return field;
}

WindField WindField::ReadTurbSimWnd(const std::string &path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		throw std::runtime_error("Cannot open TurbSim WND import file: " + path);

	WindField field;
	field.wndFormat = WndFormat::TURBSIM_WND;
	field.sourcePath = std::filesystem::path(path);

	const std::int16_t record1 = ReadScalar<std::int16_t>(in);
	const std::int16_t record2 = ReadScalar<std::int16_t>(in);
	if (record1 != -99 || record2 != 4)
		throw std::runtime_error("Unsupported TurbSim WND header");

	const std::int32_t nComp = ReadScalar<std::int32_t>(in);
	if (nComp != 3)
		throw std::runtime_error("Only 3-component TurbSim WND files are supported");

	(void)ReadScalar<float>(in);
	(void)ReadScalar<float>(in);
	field.hubHeight = ReadScalar<float>(in);
	const std::array<float, 3> tiPercent{
	    ReadScalar<float>(in),
	    ReadScalar<float>(in),
	    ReadScalar<float>(in)};

	field.dz = ReadScalar<float>(in);
	field.dy = ReadScalar<float>(in);
	const double dx = ReadScalar<float>(in);
	const std::int32_t halfNt = ReadScalar<std::int32_t>(in);
	field.meanWindSpeed = ReadScalar<float>(in);
	field.dt = field.meanWindSpeed > kTiny ? dx / field.meanWindSpeed : 0.0;
	field.nz = 0;
	field.ny = 0;

	for (int i = 0; i < 3; ++i)
		(void)ReadScalar<float>(in);
	(void)ReadScalar<std::int32_t>(in);
	(void)ReadScalar<std::int32_t>(in);
	field.nz = ReadScalar<std::int32_t>(in);
	field.ny = ReadScalar<std::int32_t>(in);
	for (int i = 0; i < 6; ++i)
		(void)ReadScalar<std::int32_t>(in);

	field.nSteps = halfNt * 2;
	field.zBottom = field.hubHeight - 0.5 * field.dz * static_cast<double>(std::max(field.nz - 1, 0));
	field.Resize(field.nSteps, field.ny, field.nz);
	field.BuildCoordinates();

	const double tiU = std::max(static_cast<double>(tiPercent[0]) / 100.0, 1.0e-6);
	const double tiV = std::max(static_cast<double>(tiPercent[1]) / 100.0, 1.0e-6);
	const double tiW = std::max(static_cast<double>(tiPercent[2]) / 100.0, 1.0e-6);

	for (int step = 0; step < field.nSteps; ++step)
	{
		for (int iz = 0; iz < field.nz; ++iz)
		{
			for (int iy = 0; iy < field.ny; ++iy)
			{
				const auto rawU = ReadScalar<std::int16_t>(in);
				const auto rawV = ReadScalar<std::int16_t>(in);
				const auto rawW = ReadScalar<std::int16_t>(in);
				field.At(0, step, iz, iy) = (static_cast<double>(rawU) + 1000.0 / tiU) * (field.meanWindSpeed * tiU) / 1000.0;
				field.At(1, step, iz, iy) = static_cast<double>(rawV) * (field.meanWindSpeed * tiV) / 1000.0;
				field.At(2, step, iz, iy) = static_cast<double>(rawW) * (field.meanWindSpeed * tiW) / 1000.0;
			}
		}
	}

	field.ComputeStats();
	return field;
}

WindField WindField::ReadBladedWnd(const std::string &path, const WindImportMetadata &metadata)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		throw std::runtime_error("Cannot open Bladed WND import file: " + path);

	WindField field;
	field.wndFormat = WndFormat::BLADED_WND;
	field.sourcePath = std::filesystem::path(path);

	const std::int16_t record1 = ReadScalar<std::int16_t>(in);
	const std::int16_t record2 = ReadScalar<std::int16_t>(in);
	if (record1 != -99)
		throw std::runtime_error("Unsupported Bladed WND header");

	std::int32_t headerBytes = 0;
	std::int32_t nComp = 3;
	std::array<float, 3> tiPercent{0.0f, 0.0f, 0.0f};

	if (record2 >= 7)
	{
		headerBytes = ReadScalar<std::int32_t>(in);
		nComp = ReadScalar<std::int32_t>(in);
	}
	else if (record2 == 4)
	{
		nComp = ReadScalar<std::int32_t>(in);
		(void)ReadScalar<float>(in);
		(void)ReadScalar<float>(in);
		(void)ReadScalar<float>(in);
		tiPercent[0] = ReadScalar<float>(in);
		tiPercent[1] = ReadScalar<float>(in);
		tiPercent[2] = ReadScalar<float>(in);
	}

	if (nComp != 3)
		throw std::runtime_error("Only 3-component Bladed WND files are supported");

	field.dz = ReadScalar<float>(in);
	field.dy = ReadScalar<float>(in);
	const double dx = ReadScalar<float>(in);
	const std::int32_t halfNt = ReadScalar<std::int32_t>(in);
	field.meanWindSpeed = ReadScalar<float>(in);
	for (int i = 0; i < 3; ++i)
		(void)ReadScalar<float>(in);
	(void)ReadScalar<float>(in);
	(void)ReadScalar<std::int32_t>(in);
	field.nz = ReadScalar<std::int32_t>(in);
	field.ny = ReadScalar<std::int32_t>(in);

	if (nComp == 3)
	{
		for (int i = 0; i < 6; ++i)
			(void)ReadScalar<float>(in);
	}

	if (record2 == 7)
	{
		(void)ReadScalar<float>(in);
		(void)ReadScalar<float>(in);
	}
	else if (record2 == 8)
	{
		for (int i = 0; i < 7; ++i)
			(void)ReadScalar<float>(in);
		for (int i = 0; i < 5; ++i)
			(void)ReadScalar<std::int32_t>(in);
		(void)ReadScalar<float>(in);
		(void)ReadScalar<float>(in);
	}

	if (record2 >= 7)
	{
		const auto headerEnd = static_cast<std::streamoff>(8 + headerBytes);
		if (in.tellg() < headerEnd)
			in.seekg(headerEnd, std::ios::beg);
	}

	const auto companion = ParseCompanionSummary(std::filesystem::path(path).replace_extension(".sum"));
	if (companion && companion->hasStats)
		field.usedCompanionSummary = true;

	field.hubHeight = companion && companion->hasHubHeight ? companion->hubHeight
	                                                       : (metadata.hubHeight > 0.0 ? metadata.hubHeight : metadata.refHeight);
	if (field.hubHeight <= 0.0)
		field.hubHeight = 0.5 * field.dz * static_cast<double>(std::max(field.nz - 1, 0));

	field.nSteps = halfNt * 2;
	field.dt = field.meanWindSpeed > kTiny ? dx / field.meanWindSpeed : 0.0;
	field.zBottom = field.hubHeight - 0.5 * field.dz * static_cast<double>(std::max(field.nz - 1, 0));
	field.Resize(field.nSteps, field.ny, field.nz);
	field.BuildCoordinates();

	const double reconstructionMeanWind = metadata.meanWindSpeed > 0.0 ? metadata.meanWindSpeed : field.meanWindSpeed;
	const std::array<double, 3> means = companion && companion->hasStats ? companion->mean : MeansFromMetadata(metadata, field.meanWindSpeed);
	const std::array<double, 3> sigmas = ResolveBladedSigma(record2, reconstructionMeanWind, tiPercent, metadata, companion);

	for (int step = 0; step < field.nSteps; ++step)
	{
		for (int iz = 0; iz < field.nz; ++iz)
		{
			for (int iy = field.ny - 1; iy >= 0; --iy)
			{
				for (int comp = 0; comp < 3; ++comp)
				{
					const auto raw = ReadScalar<std::int16_t>(in);
					field.At(comp, step, iz, iy) =
					    means[static_cast<std::size_t>(comp)] +
					    static_cast<double>(raw) * sigmas[static_cast<std::size_t>(comp)] / 1000.0;
				}
			}
		}
	}

	if (!field.usedCompanionSummary)
		field.warnings.push_back("Bladed WND import reconstructed physical values without a companion .sum file.");

	field.ComputeStats();
	return field;
}

WindField WindField::ReadAny(const std::string &path, WndFormat format, const WindImportMetadata &metadata)
{
	const std::filesystem::path source(path);
	auto ext = source.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

	WindField field;
	if (ext == ".bts")
		field = ReadBts(path);
	else if (ext == ".wnd")
	{
		if (format == WndFormat::BLADED_WND)
			field = ReadBladedWnd(path, metadata);
		else if (format == WndFormat::TURBSIM_WND)
			field = ReadTurbSimWnd(path);
		else
			throw std::runtime_error("WndFormat=TURBSIM_BTS is invalid for a .wnd import file");
	}
	else
	{
		throw std::runtime_error("Unsupported import wind-file extension");
	}

	WarnOnMismatch(field, metadata);
	return field;
}
