#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "AeroL/AeroL_Type.hpp"
#include "ControL/ControL_Type.hpp"
#include "HydroL/HydroL_Type.hpp"
#include "IO/ModuleIO.hpp"
#include "StrL/StrL_Type.hpp"
#include "WaveL/WaveL_Type.hpp"
#include "WindL/WindL_Type.hpp"

struct VtkOutputConfig
{
	bool enabled = false;
	double dt = 0.0;
	int type = 0;
	int sideNum = 0;
};

struct SimLInput
{
	std::filesystem::path inputPath;

	double tMax = 0.0;
	double dt = 0.0;
	bool afShowLog = false;
	int dtPut = 0;
	int simulateType = 0;
	double rampUpTime = 0.0;

	int wtType = 0;
	int solver = 0;
	bool linearization = false;

	double gravity = 9.81;
	double airDensity = 1.225;
	double waterDensity = 1025.0;
	double kinVisc = 1.464e-5;
	double speedOfSound = 335.0;

	std::string strFile;
	std::string windFile;
	std::string aeroFile;
	std::string controlFile;
	std::string hydroLFile;
	std::string subFEMLFile;
	std::string mlinLFile;

	VtkOutputConfig vtk;
	OutputConfig output;
};

struct SimLModuleInputs
{
	std::vector<std::string> warnings;

	AeroLInput aeroL;
	StrLInput strL;
	ControLInput controL;
	WindLInput windL;

	std::optional<HydroLInput> hydroL;
	std::optional<WaveLInput> waveL;

	BladeAeroStructInput bladeAeroStruct;
	std::optional<TowerStructInput> towerStruct;
};

struct SimLResolvedInput
{
	SimLInput simL;
	SimLModuleInputs modules;
};
