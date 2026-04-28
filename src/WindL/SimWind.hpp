#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "WindL/WindL_Type.hpp"

struct SimWindComponentStats
{
	double mean = 0.0;
	double sigma = 0.0;
	double turbulenceIntensity = 0.0;
};

struct SimWindResult
{
	std::string btsPath;
	std::string bladedWndPath;
	std::string turbsimWndPath;
	std::string wndPath;
	std::string sumPath;
	int gridPtsY = 0;
	int gridPtsZ = 0;
	int timeSteps = 0;
	double timeStep = 0.0;
	double hubHeight = 0.0;
	double meanWindSpeed = 0.0;
	double estimatedPeakMemoryGiB = 0.0;
	double estimatedCholeskyFlops = 0.0;
	std::array<SimWindComponentStats, 3> stats{};
	std::vector<std::string> warnings;
};

using SimWindProgressCallback = std::function<void(const std::string &)>;

class SimWind
{
public:
	static SimWindResult Generate(const WindLInput &input, SimWindProgressCallback progress = {});
	static SimWindResult GenerateFromFile(const std::string &qwdPath, SimWindProgressCallback progress = {});
};
