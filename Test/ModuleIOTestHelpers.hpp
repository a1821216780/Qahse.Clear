#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "AeroL/AeroL_Type.hpp"
#include "ControL/ControL_Type.hpp"
#include "HydroL/HydroL_Type.hpp"
#include "IO/ModuleIO.hpp"
#include "SimL/SimL_Type.hpp"
#include "StrL/StrL_Type.hpp"

namespace module_io_test
{
inline bool HasRepoMarker(const std::filesystem::path &root)
{
	std::error_code ec;
	return std::filesystem::is_regular_file(root / "vsstudio" / "UnitTests.vcxproj", ec);
}

inline std::filesystem::path FindRepoRoot(std::filesystem::path dir)
{
	std::error_code ec;
	dir = std::filesystem::absolute(dir, ec);
	while (!ec && !dir.empty())
	{
		if (HasRepoMarker(dir))
			return dir;
		const auto parent = dir.parent_path();
		if (parent == dir)
			break;
		dir = parent;
	}
	return std::filesystem::current_path();
}

inline std::filesystem::path RepoRoot()
{
	return FindRepoRoot(std::filesystem::current_path());
}

inline std::filesystem::path TestOutputDir()
{
	auto dir = RepoRoot() / "build" / "test" / "module_io";
	std::filesystem::create_directories(dir);
	return dir;
}

inline std::filesystem::path SemisubRoot()
{
	return RepoRoot() / "demo" / "SimL" / "NREL_5MW_OC4_Semisub";
}

inline std::filesystem::path SemisubMainFile()
{
	return SemisubRoot() / "Qahse_SimL_NREL_5MW_OC4_Semisub.hst";
}

inline std::vector<std::filesystem::path> SimLCases()
{
	std::vector<std::filesystem::path> cases;
	const auto root = RepoRoot() / "demo" / "SimL";
	for (const auto &entry : std::filesystem::directory_iterator(root))
	{
		if (!entry.is_directory() || entry.path().filename() == "save")
			continue;
		for (const auto &file : std::filesystem::directory_iterator(entry.path()))
		{
			if (file.path().extension() == ".hst")
				cases.push_back(file.path());
		}
	}
	return cases;
}

inline std::string QuotePath(const std::filesystem::path &path)
{
	return "\"" + path.string() + "\"";
}

inline void WriteLines(const std::filesystem::path &path, const std::vector<std::string> &lines)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream out(path);
	ASSERT_TRUE(out.is_open()) << path.string();
	for (const auto &line : lines)
		out << line << '\n';
}

inline void TouchFile(const std::filesystem::path &path)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream out(path);
	ASSERT_TRUE(out.is_open()) << path.string();
	out << "test\n";
}

inline std::string NormalizedPath(std::string path)
{
	return std::filesystem::path(path).lexically_normal().string();
}

inline void ExpectOutputEqual(const OutputConfig &actual, const OutputConfig &expected)
{
	EXPECT_EQ(actual.sumPrint, expected.sumPrint);
	EXPECT_EQ(actual.afSpanput, expected.afSpanput);
	EXPECT_EQ(actual.bldOutSig, expected.bldOutSig);
	EXPECT_EQ(actual.twrOutSig, expected.twrOutSig);
	EXPECT_EQ(actual.blade.count, expected.blade.count);
	EXPECT_EQ(actual.blade.nodes, expected.blade.nodes);
	EXPECT_EQ(actual.tower.count, expected.tower.count);
	EXPECT_EQ(actual.tower.nodes, expected.tower.nodes);
	EXPECT_EQ(actual.outList, expected.outList);
	EXPECT_DOUBLE_EQ(actual.dtOut, expected.dtOut);
	EXPECT_DOUBLE_EQ(actual.tStart, expected.tStart);
	EXPECT_EQ(actual.outType, expected.outType);
	EXPECT_EQ(NormalizedPath(actual.sumPath), NormalizedPath(expected.sumPath));
}

inline void ExpectMatrixNear(const Eigen::MatrixXd &actual, const Eigen::MatrixXd &expected)
{
	ASSERT_EQ(actual.rows(), expected.rows());
	ASSERT_EQ(actual.cols(), expected.cols());
	for (Eigen::Index r = 0; r < actual.rows(); ++r)
		for (Eigen::Index c = 0; c < actual.cols(); ++c)
			EXPECT_NEAR(actual(r, c), expected(r, c), 1.0e-12) << "matrix(" << r << "," << c << ")";
}

inline void ExpectAeroEqual(const AeroLInput &actual, const AeroLInput &expected)
{
	EXPECT_EQ(actual.apOfMb, expected.apOfMb);
	EXPECT_EQ(actual.rotorType, expected.rotorType);
	EXPECT_DOUBLE_EQ(actual.hubRadius, expected.hubRadius);
	EXPECT_EQ(actual.bladeNum, expected.bladeNum);
	EXPECT_DOUBLE_EQ(actual.ratedPower, expected.ratedPower);
	EXPECT_DOUBLE_EQ(actual.airDensity, expected.airDensity);
	EXPECT_EQ(actual.airfoils.count, expected.airfoils.count);
	EXPECT_EQ(actual.airfoils.interpolationOrder, expected.airfoils.interpolationOrder);
	EXPECT_EQ(actual.airfoils.files, expected.airfoils.files);
	EXPECT_EQ(NormalizedPath(actual.bladeAeroStructFile), NormalizedPath(expected.bladeAeroStructFile));
	EXPECT_EQ(actual.dynamicStallType, expected.dynamicStallType);
	EXPECT_EQ(actual.iagParams, expected.iagParams);
	EXPECT_EQ(actual.wakeType, expected.wakeType);
	EXPECT_EQ(actual.ifPitch, expected.ifPitch);
	ExpectOutputEqual(actual.output, expected.output);
}

inline void ExpectControlEqual(const ControLInput &actual, const ControLInput &expected)
{
	EXPECT_EQ(actual.pcMode, expected.pcMode);
	EXPECT_EQ(NormalizedPath(actual.dllFileName), NormalizedPath(expected.dllFileName));
	EXPECT_EQ(NormalizedPath(actual.dllInFile), NormalizedPath(expected.dllInFile));
	EXPECT_EQ(actual.dllProcName, expected.dllProcName);
	ExpectOutputEqual(actual.output, expected.output);
}

inline void ExpectHydroEqual(const HydroLInput &actual, const HydroLInput &expected)
{
	EXPECT_DOUBLE_EQ(actual.waterDepth, expected.waterDepth);
	EXPECT_DOUBLE_EQ(actual.waterDensity, expected.waterDensity);
	EXPECT_EQ(actual.isFloating, expected.isFloating);
	EXPECT_EQ(actual.advancedBuoyancy, expected.advancedBuoyancy);
	EXPECT_EQ(NormalizedPath(actual.waveLFile), NormalizedPath(expected.waveLFile));
	EXPECT_EQ(actual.staticBuoyancy, expected.staticBuoyancy);
	EXPECT_DOUBLE_EQ(actual.unitLengthWamit, expected.unitLengthWamit);
	EXPECT_DOUBLE_EQ(actual.diffractionOffset, expected.diffractionOffset);
	EXPECT_DOUBLE_EQ(actual.deltaTIrf, expected.deltaTIrf);
	EXPECT_EQ(actual.constrainedFloater, expected.constrainedFloater);
	EXPECT_EQ(NormalizedPath(actual.potentialRadFile), NormalizedPath(expected.potentialRadFile));
	EXPECT_EQ(actual.useRadiation, expected.useRadiation);
	EXPECT_EQ(actual.useRadAddedMass, expected.useRadAddedMass);
	EXPECT_DOUBLE_EQ(actual.deltaFreqRadiation, expected.deltaFreqRadiation);
	EXPECT_EQ(NormalizedPath(actual.potentialExcFile), NormalizedPath(expected.potentialExcFile));
	EXPECT_EQ(actual.useExcitation, expected.useExcitation);
	EXPECT_EQ(NormalizedPath(actual.potentialDiffFile), NormalizedPath(expected.potentialDiffFile));
	EXPECT_EQ(actual.diffEvalType, expected.diffEvalType);
	EXPECT_EQ(NormalizedPath(actual.potentialSumFile), NormalizedPath(expected.potentialSumFile));
	EXPECT_EQ(actual.useSumFreqs, expected.useSumFreqs);
	EXPECT_DOUBLE_EQ(actual.buoyancyTuner, expected.buoyancyTuner);
	EXPECT_EQ(actual.beamType, expected.beamType);
	EXPECT_EQ(actual.transitionMass, expected.transitionMass);
	ExpectMatrixNear(actual.jointOffset, expected.jointOffset);
	ExpectMatrixNear(actual.tpOrientation, expected.tpOrientation);
	ExpectMatrixNear(actual.subMassMatrix, expected.subMassMatrix);
	ExpectMatrixNear(actual.hydroAddedMassMatrix, expected.hydroAddedMassMatrix);
	EXPECT_EQ(actual.subJoints, expected.subJoints);
	EXPECT_EQ(actual.rigidSubElements, expected.rigidSubElements);
	EXPECT_EQ(actual.hydroJointCoeff, expected.hydroJointCoeff);
	EXPECT_EQ(actual.hydroMemberCoeff, expected.hydroMemberCoeff);
	EXPECT_EQ(actual.subConstraints, expected.subConstraints);
	EXPECT_EQ(actual.subMembers, expected.subMembers);
	EXPECT_EQ(actual.moorElements, expected.moorElements);
	EXPECT_EQ(actual.moorMembers, expected.moorMembers);
	EXPECT_EQ(actual.outputPoints, expected.outputPoints);
	ExpectOutputEqual(actual.output, expected.output);
}

inline void ExpectSimEqual(const SimLInput &actual, const SimLInput &expected)
{
	EXPECT_DOUBLE_EQ(actual.tMax, expected.tMax);
	EXPECT_DOUBLE_EQ(actual.dt, expected.dt);
	EXPECT_EQ(actual.afShowLog, expected.afShowLog);
	EXPECT_EQ(actual.dtPut, expected.dtPut);
	EXPECT_EQ(actual.simulateType, expected.simulateType);
	EXPECT_DOUBLE_EQ(actual.rampUpTime, expected.rampUpTime);
	EXPECT_EQ(actual.wtType, expected.wtType);
	EXPECT_EQ(actual.solver, expected.solver);
	EXPECT_EQ(actual.linearization, expected.linearization);
	EXPECT_DOUBLE_EQ(actual.gravity, expected.gravity);
	EXPECT_DOUBLE_EQ(actual.airDensity, expected.airDensity);
	EXPECT_DOUBLE_EQ(actual.waterDensity, expected.waterDensity);
	EXPECT_EQ(NormalizedPath(actual.strFile), NormalizedPath(expected.strFile));
	EXPECT_EQ(NormalizedPath(actual.windFile), NormalizedPath(expected.windFile));
	EXPECT_EQ(NormalizedPath(actual.aeroFile), NormalizedPath(expected.aeroFile));
	EXPECT_EQ(NormalizedPath(actual.controlFile), NormalizedPath(expected.controlFile));
	EXPECT_EQ(NormalizedPath(actual.hydroLFile), NormalizedPath(expected.hydroLFile));
	EXPECT_EQ(actual.vtk.enabled, expected.vtk.enabled);
	EXPECT_DOUBLE_EQ(actual.vtk.dt, expected.vtk.dt);
	EXPECT_EQ(actual.vtk.type, expected.vtk.type);
	EXPECT_EQ(actual.vtk.sideNum, expected.vtk.sideNum);
	ExpectOutputEqual(actual.output, expected.output);
}

inline void ExpectStrEqual(const StrLInput &actual, const StrLInput &expected)
{
	EXPECT_EQ(actual.towerNum, expected.towerNum);
	EXPECT_DOUBLE_EQ(actual.towerYdeg, expected.towerYdeg);
	EXPECT_EQ(actual.numBld, expected.numBld);
	EXPECT_DOUBLE_EQ(actual.hubRadius, expected.hubRadius);
	EXPECT_DOUBLE_EQ(actual.rotorOverhang, expected.rotorOverhang);
	EXPECT_DOUBLE_EQ(actual.shaftTilt, expected.shaftTilt);
	EXPECT_DOUBLE_EQ(actual.preCone, expected.preCone);
	EXPECT_DOUBLE_EQ(actual.twr2Shft, expected.twr2Shft);
	EXPECT_DOUBLE_EQ(actual.hubMass, expected.hubMass);
	EXPECT_DOUBLE_EQ(actual.hubIner, expected.hubIner);
	EXPECT_DOUBLE_EQ(actual.gravity, expected.gravity);
	EXPECT_DOUBLE_EQ(actual.azimuth, expected.azimuth);
	EXPECT_DOUBLE_EQ(actual.azimB1Up, expected.azimB1Up);
	EXPECT_DOUBLE_EQ(actual.rotSpeed, expected.rotSpeed);
	EXPECT_DOUBLE_EQ(actual.nacYaw, expected.nacYaw);
	EXPECT_DOUBLE_EQ(actual.naccAx, expected.naccAx);
	EXPECT_DOUBLE_EQ(actual.naccAy, expected.naccAy);
	EXPECT_DOUBLE_EQ(actual.naccAz, expected.naccAz);
	EXPECT_DOUBLE_EQ(actual.naccDx, expected.naccDx);
	EXPECT_DOUBLE_EQ(actual.naccDy, expected.naccDy);
	EXPECT_DOUBLE_EQ(actual.naccDz, expected.naccDz);
	EXPECT_DOUBLE_EQ(actual.yawBrMass, expected.yawBrMass);
	EXPECT_DOUBLE_EQ(actual.nacMass, expected.nacMass);
	EXPECT_DOUBLE_EQ(actual.nacCmX, expected.nacCmX);
	EXPECT_DOUBLE_EQ(actual.nacCmY, expected.nacCmY);
	EXPECT_DOUBLE_EQ(actual.nacCmZ, expected.nacCmZ);
	EXPECT_DOUBLE_EQ(actual.nacYawIner, expected.nacYawIner);
	EXPECT_DOUBLE_EQ(actual.gearboxRatio, expected.gearboxRatio);
	EXPECT_DOUBLE_EQ(actual.gearboxEff, expected.gearboxEff);
	EXPECT_EQ(actual.drivetrainDof, expected.drivetrainDof);
	EXPECT_DOUBLE_EQ(actual.genIner, expected.genIner);
	EXPECT_DOUBLE_EQ(actual.dtTorSpr, expected.dtTorSpr);
	EXPECT_DOUBLE_EQ(actual.dtTorDmp, expected.dtTorDmp);
	EXPECT_EQ(actual.bladeNum, expected.bladeNum);
	EXPECT_EQ(NormalizedPath(actual.bladeAeroStructFile), NormalizedPath(expected.bladeAeroStructFile));
	EXPECT_EQ(actual.bladeFiles, expected.bladeFiles);
	EXPECT_DOUBLE_EQ(actual.towerHeight, expected.towerHeight);
	EXPECT_EQ(NormalizedPath(actual.towerFile), NormalizedPath(expected.towerFile));
	ExpectOutputEqual(actual.output, expected.output);
}
} // namespace module_io_test
