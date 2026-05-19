#pragma once

#include <Eigen/Dense>

#include <filesystem>
#include <string>
#include <vector>

#include "IO/ModuleIO.hpp"

struct BladeSection
{
	std::vector<std::string> raw;
	double radialPos = 0.0;
	double chord = 0.0;
	double twist = 0.0;
	double offsetY = 0.0;
	double offsetX = 0.0;
	double pitchAxisY = 0.0;
	double pitchAxisX = 0.0;
	int polarFileId = 0;
	std::string polarFileToken;
	double relThickness = 0.0;
};

struct BladeAeroStructInput
{
	std::filesystem::path inputPath;
	double rayleighDamp = 0.0;
	double stiffTuner = 1.0;
	double massTuner = 1.0;
	int beamType = 1;
	int discCount = 0;
	std::vector<BladeSection> sections;
	std::vector<std::vector<std::string>> sectionRows;
};

struct TowerStructInput
{
	std::filesystem::path inputPath;
	std::vector<std::vector<std::string>> sectionRows;
};

struct StrLInput
{
	std::filesystem::path inputPath;

	int numBld = 3;
	double rotorOverhang = 0.0;
	double shaftTilt = 0.0;
	double preCone = 0.0;
	double azimuth = 0.0;

	bool drivetrainDof = false;
	double dtTorSpr = 0.0;
	double dtTorDmp = 0.0;

	int bladeNum = 3;
	std::string bladeAeroStructFile;
	std::vector<std::string> bladeFiles;

	double towerHeight = 0.0;
	std::string towerFile;
	std::string subFile;

	OutputConfig output;
};
