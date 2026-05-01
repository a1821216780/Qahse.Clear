#pragma once

#include <functional>
#include <string>
#include <vector>

#include "WindL/WindL_Type.hpp"
#include "WindL/SimWind.hpp"

struct WindLBatchCaseResult
{
	std::string caseName;
	std::string status;
	std::string message;
	std::string outputDir;
	std::string derivedQwdPath;
	std::string logPath;
	std::string btsPath;
	std::string bladedWndPath;
	std::string turbsimWndPath;
	std::string sumPath;
	int rowIndex = -1;
	int exitCode = 0;
	double durationSeconds = 0.0;
};

struct WindLBatchResult
{
	std::string manifestPath;
	std::string csvPath;
	std::string summaryPath;
	int totalCases = 0;
	int succeeded = 0;
	int failed = 0;
	int invalid = 0;
	int skipped = 0;
	int validated = 0;
	std::vector<WindLBatchCaseResult> cases;
};

using WindLBatchProgressCallback = std::function<void(const std::string &)>;

class WindLBatch
{
public:
	static WindLBatchResult RunFromFile(const std::string &qwdPath,
	                                    const std::string &executablePath,
	                                    WindLBatchProgressCallback progress = {});
};
