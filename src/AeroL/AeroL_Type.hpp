#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "IO/ModuleIO.hpp"

struct AirfoilFileSet
{
	int count = 0;
	int interpolationOrder = 1;
	std::vector<std::string> files;
};

struct AeroLInput
{
	std::filesystem::path inputPath;

	int apOfMb = 0;
	std::string rotorType = "HAWT";
	double hubRadius = 0.0;
	int bladeNum = 3;
	double cutInWindSpeed = 0.0;
	double cutOutWindSpeed = 0.0;
	double ratedPower = 0.0;
	double ratedRotorSpeed = 0.0;

	double airDensity = 1.225;
	double kinVisc = 1.464e-5;
	double speedOfSound = 335.0;

	AirfoilFileSet airfoils;
	std::string bladeAeroStructFile;

	bool unsteadyAero = false;
	bool twoPointLiftDrag = false;
	bool himmelskamp = false;
	bool towerShadow = false;
	double towerDrag = 0.0;

	int dynamicStallType = 0;
	double tfOye = 0.0;
	double amGb = 0.0;
	double tfAte = 0.0;
	double tpAte = 0.0;
	std::vector<double> iagParams;

	int wakeType = 1;
	int wakeIntType = 0;
	bool wakeRollup = true;
	bool trailingVort = true;
	bool shedVort = true;
	int convectionType = 0;
	double wakeRelaxation = 1.0;
	double firstWakeRow = 1.0;
	int maxWakeSize = 0;
	double maxWakeDist = 0.0;
	double wakeReduction = 0.0;
	int wakeLengthType = 0;
	double conversionLength = 0.0;
	double nearWakeLength = 0.0;
	double zone1Length = 0.0;
	double zone2Length = 0.0;
	double zone3Length = 0.0;
	int zone1Factor = 1;
	int zone2Factor = 1;
	int zone3Factor = 1;
	int zone1FactorS = 1;
	int zone2FactorS = 1;
	int zone3FactorS = 1;

	double boundCoreRadius = 0.0;
	double wakeCoreRadius = 0.0;
	double vortexViscosity = 0.0;
	bool vortexStrain = false;
	double maxStrain = 0.0;

	double gammaEpsilon = 0.0;
	int gammaIterations = 0;

	int polarDisc = 0;
	bool bemTipLoss = true;
	double bemSpeedUp = 0.0;

	double minLambda = 0.0;
	double maxLambda = 0.0;
	double lambdaStep = 0.0;
	double minPitch = 0.0;
	double maxPitch = 0.0;
	double pitchStep = 0.0;
	std::string cpResultFilePath;

	double minWindSpeed = 0.0;
	double maxWindSpeed = 0.0;
	double windSpeedStep = 0.0;
	double origPitch = 0.0;
	double omegaMin = 0.0;
	double genEfficiency = 0.0;
	double pitchUp = 0.0;
	double pitchDown = 0.0;
	bool ifPitch = true;
	double fixedPitch = 0.0;
	double fixedRotationalSpeed = 0.0;
	std::string powerCurveResultFilePath;

	OutputConfig output;
};
