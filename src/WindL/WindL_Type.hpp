#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "SiMwind/SimWind_Type.hpp"

enum class WindLWindType
{
	STEADY = 1,
	USER_DEFINED = 2,
	TURBSIM_WND = 3,
	BLADED_WND = 4,
	TURBSIM_BTS = 5
};

struct WindLTimeSpeed
{
	double time = 0.0;
	double speed = 0.0;
};

struct WindVelocityOptions
{
	bool cycleWind = true;
	bool mirrorTime = false;
	bool autoFieldShift = true;
	double shiftTime = 0.0;
	InterpMethod interpMethod = InterpMethod::TRILINEAR;
};

struct WindImportMetadata
{
	std::string filePath;
	WndFormat format = WndFormat::TURBSIM_BTS;

	int expectedNy = 0;
	int expectedNz = 0;
	double expectedDt = 0.0;
	double hubHeight = 0.0;
	double refHeight = 0.0;
	double meanWindSpeed = 0.0;
	double horAngle = 0.0;
	double vertAngle = 0.0;
	double tiU = 0.0;
	double tiV = 0.0;
	double tiW = 0.0;

	bool sumPrint = false;
	std::string savePath;
	std::string saveName;
};

struct WindLInput
{
	std::filesystem::path inputPath;
	WindLWindType windType = WindLWindType::STEADY;
	bool cycleWind = true;

	double hWindSpeed = 0.0;
	double refHeight = 0.0;
	double plExp = 0.0;

	double gridYMin = 0.0;
	double gridYMax = 0.0;
	double gridYStep = 0.0;
	double gridZMin = 0.0;
	double gridZMax = 0.0;
	double gridZStep = 0.0;
	std::vector<WindLTimeSpeed> windSpeedList;

	std::string turWindFilePath;
	std::string bldWindFilePath;
	std::string iecWindFilePath;

	std::vector<std::string> warnings;
};
