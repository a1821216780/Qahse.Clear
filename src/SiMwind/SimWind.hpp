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
// 该文件声明了 SimWind 风场生成模块的结果结构体（SimWindComponentStats、SimWindResult）
// 和主函数 SimWindGenerate，基于 Mann 模型和 FFT 合成生成三维湍流风场。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "SiMwind/SimWind_Type.hpp"

/**
 * @brief 单个风分量的统计信息。
 */
struct SimWindComponentStats
{
	double mean = 0.0;                  ///< 平均风速 (m/s)
	double sigma = 0.0;                 ///< 标准差 (m/s)
	double turbulenceIntensity = 0.0;   ///< 湍流强度 (fraction)
};

/**
 * @brief 风场生成的完整结果。
 */
struct SimWindResult
{
	std::string btsPath;                                    ///< .bts 输出路径
	std::string bladedWndPath;                              ///< Bladed .wnd 输出路径
	std::string turbsimWndPath;                             ///< TurbSim 兼容 .wnd 输出路径
	std::string wndPath;                                    ///< 主要 .wnd 路径的别名
	std::string sumPath;                                    ///< .sum 统计摘要路径
	int gridPtsY = 0;                                       ///< Y 方向网格点数
	int gridPtsZ = 0;                                       ///< Z 方向网格点数
	int timeSteps = 0;                                      ///< 时间步数
	double timeStep = 0.0;                                  ///< 时间步长 (s)
	double hubHeight = 0.0;                                 ///< 轮毂高度 (m)
	double meanWindSpeed = 0.0;                             ///< 轮毂平均风速 (m/s)
	double estimatedPeakMemoryGiB = 0.0;                    ///< 估计峰值内存 (GiB)
	double estimatedCholeskyFlops = 0.0;                    ///< 估计 Cholesky 运算量 (FLOPs)
	std::array<SimWindComponentStats, 3> stats{};           ///< 各分量统计 (u, v, w)
	std::vector<std::string> warnings;                      ///< 生成警告列表
};

/// @brief SimWind 进度回调函数类型。
using SimWindProgressCallback = std::function<void(const std::string &)>;

/**
 * @brief 风场模拟器，提供风场生成与输入校验功能。
 */
class SimWind
{
public:
	/**
	 * @brief 仅校验输入参数，不执行风场生成。
	 * @param input 风场输入参数
	 */
	static void ValidateInputOnly(const SimWindInput &input);

	/**
	 * @brief 根据输入参数生成风场。
	 * @param input 风场输入参数
	 * @param progress 进度回调（可选）
	 * @return 风场生成结果
	 */
	static SimWindResult Generate(const SimWindInput &input, SimWindProgressCallback progress = {});

	/**
	 * @brief 从 .qwd 文件读取输入并生成风场。
	 * @param qwdPath .qwd 输入文件路径
	 * @param progress 进度回调（可选）
	 * @return 风场生成结果
	 */
	static SimWindResult GenerateFromFile(const std::string &qwdPath, SimWindProgressCallback progress = {});

};
