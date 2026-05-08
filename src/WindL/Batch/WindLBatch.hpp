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
// 该文件声明了 WindLBatch 批量执行结构体（WindLBatchCaseResult）和 WindLBatchRunner 类，
// 支持多案例、多线程批量风场生成。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "WindL/WindL_Type.hpp"
#include "WindL/SimWind.hpp"

/**
 * @brief 单个批量案例的执行结果。
 */
struct WindLBatchCaseResult
{
	std::string caseName;          ///< 案例名称
	std::string status;            ///< 执行状态 (success / failed / invalid / skipped / validated)
	std::string message;           ///< 状态消息
	std::string outputDir;         ///< 输出目录
	std::string derivedQwdPath;    ///< 派生 .qwd 文件路径
	std::string logPath;           ///< 日志文件路径
	std::string btsPath;           ///< .bts 输出路径
	std::string bladedWndPath;     ///< Bladed .wnd 输出路径
	std::string turbsimWndPath;    ///< TurbSim 兼容 .wnd 输出路径
	std::string sumPath;           ///< .sum 统计摘要路径
	int rowIndex = -1;             ///< 在 CSV/Excel 中的行索引
	int exitCode = 0;              ///< 进程退出码
	double durationSeconds = 0.0;  ///< 执行耗时 (s)
};

/**
 * @brief 批量风场生成的聚合结果。
 */
struct WindLBatchResult
{
	std::string manifestPath;                   ///< manifest 文件路径
	std::string csvPath;                        ///< 汇总 CSV 路径
	std::string summaryPath;                    ///< 摘要文件路径
	int totalCases = 0;                         ///< 总案例数
	int succeeded = 0;                          ///< 成功案例数
	int failed = 0;                             ///< 失败案例数
	int invalid = 0;                            ///< 无效案例数
	int skipped = 0;                            ///< 跳过案例数
	int validated = 0;                          ///< 校验通过案例数
	std::vector<WindLBatchCaseResult> cases;    ///< 各案例结果列表
};

/// @brief WindLBatch 进度回调函数类型。
using WindLBatchProgressCallback = std::function<void(const std::string &)>;

/**
 * @brief 批量风场运行器，支持从参数表批量生成风场。
 */
class WindLBatch
{
public:
	/**
	 * @brief 从参数文件运行批量风场生成。
	 * @param qwdPath .qwd 批量参数文件路径
	 * @param executablePath WindL 可执行文件路径
	 * @param progress 进度回调（可选）
	 * @return 批量执行结果
	 */
	static WindLBatchResult RunFromFile(const std::string &qwdPath,
	                                    const std::string &executablePath,
	                                    WindLBatchProgressCallback progress = {});
};
