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
