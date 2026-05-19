#pragma once

#include <Eigen/Dense>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "IO/ModuleIO.hpp"

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

enum class HydroLDiffEvalType
{
	NONE = 0,
	EXPLICIT_QTF = 1,
	NEWMAN = 2,
	MEAN_DRIFT = 3
};

struct HydroLInput
{
	std::filesystem::path inputPath;

	double waterDepth = 0.0;
	double waterDensity = 1025.0;
	bool isFloating = false;
	int advancedBuoyancy = 0;
	int waveKinEvalMorison = 0;
	int waveKinEvalPotential = 1;
	double waveKinTau = 30.0;
	std::string waveLFile;

	bool staticBuoyancy = true;
	double unitLengthWamit = 1.0;
	double diffractionOffset = 0.0;
	double deltaTIrf = 0.025;
	bool constrainedFloater = false;
	std::string potentialRadFile;
	bool useRadiation = false;
	bool useRadAddedMass = false;
	double deltaFreqRadiation = 0.0;
	double truncTimeRadiation = 0.0;

	std::string potentialExcFile;
	bool useExcitation = false;
	double deltaFreqExcitation = 0.0;
	double deltaDirExcitation = 0.0;
	double truncTimeExcitation = 0.0;

	std::string potentialDiffFile;
	HydroLDiffEvalType diffEvalType = HydroLDiffEvalType::NONE;
	std::string potentialSumFile;
	bool useSumFreqs = false;

	double subDisplacedVolume = 0.0;
	double buoyancyTuner = 1.0;
	double stiffTuner = 1.0;
	double massTuner = 1.0;
	int beamType = 1;
	std::vector<std::string> transitionMass;

	Eigen::MatrixXd jointOffset;
	Eigen::MatrixXd marineGrowth;
	Eigen::MatrixXd tpInterfacePos;
	Eigen::MatrixXd tpOrientation;
	Eigen::MatrixXd refCogPos;
	Eigen::MatrixXd refHydroPos;
	Eigen::MatrixXd subMassMatrix;
	Eigen::MatrixXd hydroQuadDampingMatrix;
	Eigen::MatrixXd hydroStiffnessMatrix;
	Eigen::MatrixXd hydroDampingMatrix;
	Eigen::MatrixXd hydroAddedMassMatrix;
	Eigen::MatrixXd hydroConstForce;

	std::vector<std::vector<std::string>> subJoints;
	std::vector<std::vector<std::string>> rigidSubElements;
	std::vector<std::vector<std::string>> rigidRectSubElements;
	std::vector<std::vector<std::string>> subElements;
	std::vector<std::vector<std::string>> hydroJointCoeff;
	std::vector<std::vector<std::string>> hydroMemberCoeff;
	std::vector<std::vector<std::string>> subConstraints;
	std::vector<std::vector<std::string>> subMembers;
	std::vector<std::vector<std::string>> moorElements;
	std::vector<std::vector<std::string>> moorMembers;
	std::vector<std::string> outputPoints;

	std::optional<HydroLWamitData> wamit;
	OutputConfig output;
};
