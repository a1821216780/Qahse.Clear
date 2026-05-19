#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "HydroL/HydroL_Type.hpp"

enum class WamitFileType
{
	UNKNOWN = 0,
	RADIATION = 1,
	EXCITATION = 3,
	DIFFERENCE_QTF = 12,
	SUM_QTF = 13
};

struct WamitRadiationEntry
{
	double period = 0.0;
	int row = 0;
	int column = 0;
	double addedMass = 0.0;
	double damping = 0.0;
	bool hasDamping = false;
};

struct WamitExcitationEntry
{
	double period = 0.0;
	double headingDeg = 0.0;
	int dof = 0;
	double magnitude = 0.0;
	double phaseDeg = 0.0;
	double real = 0.0;
	double imaginary = 0.0;
};

struct WamitQtfEntry
{
	double period1 = 0.0;
	double period2 = 0.0;
	double heading1Deg = 0.0;
	double heading2Deg = 0.0;
	int dof = 0;
	double magnitude = 0.0;
	double phaseDeg = 0.0;
	double real = 0.0;
	double imaginary = 0.0;
};

struct WamitData
{
	std::filesystem::path inputPath;
	WamitFileType type = WamitFileType::UNKNOWN;
	std::vector<WamitRadiationEntry> radiation;
	std::vector<WamitExcitationEntry> excitation;
	std::vector<WamitQtfEntry> qtf;
};

struct HydroLWamitData
{
	WamitData radiation;
	WamitData excitation;
	WamitData difference;
	WamitData sum;
};

WamitFileType InferWamitFileType(const std::string &path);
WamitData ReadWamitFile(const std::string &path, WamitFileType type = WamitFileType::UNKNOWN);
HydroLWamitData ReadHydroLWamitFiles(const HydroLInput &input);
