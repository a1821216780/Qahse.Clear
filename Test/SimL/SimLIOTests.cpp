#include "../ModuleIOTestHelpers.hpp"

#include <filesystem>
#include <stdexcept>

#include <gtest/gtest.h>

#include "AeroL/IO/AeroL_IO_Subs.hpp"
#include "ControL/IO/ControL_IO_Subs.hpp"
#include "HydroL/IO/HydroL_IO_Subs.hpp"
#include "SimL/IO/SimL_IO_Subs.hpp"
#include "SimL/SimL.hpp"
#include "StrL/IO/StrL_IO_Subs.hpp"
#include "WaveL/IO/WaveL_IO_Subs.hpp"
#include "WindL/IO/WindL_IO_Subs.hpp"

using namespace module_io_test;

namespace
{
std::filesystem::path CaseYamlDir(const std::filesystem::path &simPath)
{
	auto dir = TestOutputDir() / "SimL" / "yaml_models" / simPath.parent_path().filename();
	std::filesystem::create_directories(dir);
	return dir;
}

void ExpectWindLEqual(const WindLInput &actual, const WindLInput &expected)
{
	EXPECT_EQ(actual.windType, expected.windType);
	EXPECT_EQ(actual.cycleWind, expected.cycleWind);
	EXPECT_DOUBLE_EQ(actual.hWindSpeed, expected.hWindSpeed);
	EXPECT_DOUBLE_EQ(actual.refHeight, expected.refHeight);
	EXPECT_DOUBLE_EQ(actual.plExp, expected.plExp);
	EXPECT_DOUBLE_EQ(actual.gridYMin, expected.gridYMin);
	EXPECT_DOUBLE_EQ(actual.gridYMax, expected.gridYMax);
	EXPECT_DOUBLE_EQ(actual.gridYStep, expected.gridYStep);
	EXPECT_DOUBLE_EQ(actual.gridZMin, expected.gridZMin);
	EXPECT_DOUBLE_EQ(actual.gridZMax, expected.gridZMax);
	EXPECT_DOUBLE_EQ(actual.gridZStep, expected.gridZStep);
	EXPECT_EQ(NormalizedPath(actual.turWindFilePath), NormalizedPath(expected.turWindFilePath));
	EXPECT_EQ(NormalizedPath(actual.bldWindFilePath), NormalizedPath(expected.bldWindFilePath));
	EXPECT_EQ(NormalizedPath(actual.iecWindFilePath), NormalizedPath(expected.iecWindFilePath));
	ASSERT_EQ(actual.windSpeedList.size(), expected.windSpeedList.size());
	for (std::size_t i = 0; i < expected.windSpeedList.size(); ++i)
	{
		EXPECT_DOUBLE_EQ(actual.windSpeedList[i].time, expected.windSpeedList[i].time);
		EXPECT_DOUBLE_EQ(actual.windSpeedList[i].speed, expected.windSpeedList[i].speed);
	}
}

void ExpectWaveLEqual(const WaveLInput &actual, const WaveLInput &expected)
{
	EXPECT_EQ(actual.waveType, expected.waveType);
	EXPECT_DOUBLE_EQ(actual.waterDepth, expected.waterDepth);
	EXPECT_DOUBLE_EQ(actual.gravity, expected.gravity);
	EXPECT_EQ(actual.waveStretching, expected.waveStretching);
	EXPECT_DOUBLE_EQ(actual.timeOffset, expected.timeOffset);
	EXPECT_DOUBLE_EQ(actual.hs, expected.hs);
	EXPECT_DOUBLE_EQ(actual.tp, expected.tp);
	EXPECT_DOUBLE_EQ(actual.waveDirMean, expected.waveDirMean);
	EXPECT_DOUBLE_EQ(actual.waveDirMax, expected.waveDirMax);
	EXPECT_DOUBLE_EQ(actual.waveDirSpread, expected.waveDirSpread);
	EXPECT_EQ(actual.waveFreqNum, expected.waveFreqNum);
	EXPECT_EQ(actual.waveDirNum, expected.waveDirNum);
	EXPECT_EQ(actual.freqDisc, expected.freqDisc);
	EXPECT_DOUBLE_EQ(actual.dfMax, expected.dfMax);
	EXPECT_EQ(actual.autoFreqRange, expected.autoFreqRange);
	EXPECT_DOUBLE_EQ(actual.freqStart, expected.freqStart);
	EXPECT_DOUBLE_EQ(actual.freqEnd, expected.freqEnd);
	EXPECT_EQ(actual.seed, expected.seed);
	EXPECT_EQ(actual.autoGamma, expected.autoGamma);
	EXPECT_DOUBLE_EQ(actual.gamma, expected.gamma);
	EXPECT_EQ(actual.autoSigma, expected.autoSigma);
	EXPECT_DOUBLE_EQ(actual.sigma1, expected.sigma1);
	EXPECT_DOUBLE_EQ(actual.sigma2, expected.sigma2);
	EXPECT_EQ(actual.torsethaugenDoublePeak, expected.torsethaugenDoublePeak);
	EXPECT_EQ(actual.autoOchi, expected.autoOchi);
	EXPECT_DOUBLE_EQ(actual.ochiHs1, expected.ochiHs1);
	EXPECT_DOUBLE_EQ(actual.ochiHs2, expected.ochiHs2);
	EXPECT_DOUBLE_EQ(actual.ochiF1, expected.ochiF1);
	EXPECT_DOUBLE_EQ(actual.ochiF2, expected.ochiF2);
	EXPECT_DOUBLE_EQ(actual.ochiLambda1, expected.ochiLambda1);
	EXPECT_DOUBLE_EQ(actual.ochiLambda2, expected.ochiLambda2);
	EXPECT_EQ(NormalizedPath(actual.spectrumFilePath), NormalizedPath(expected.spectrumFilePath));
	EXPECT_EQ(NormalizedPath(actual.componentFilePath), NormalizedPath(expected.componentFilePath));
	EXPECT_EQ(NormalizedPath(actual.timeSeriesFilePath), NormalizedPath(expected.timeSeriesFilePath));
	EXPECT_DOUBLE_EQ(actual.dftCutIn, expected.dftCutIn);
	EXPECT_DOUBLE_EQ(actual.dftCutOut, expected.dftCutOut);
	EXPECT_DOUBLE_EQ(actual.dftSample, expected.dftSample);
	EXPECT_DOUBLE_EQ(actual.dftThreshold, expected.dftThreshold);
	EXPECT_DOUBLE_EQ(actual.constCurrent, expected.constCurrent);
	EXPECT_DOUBLE_EQ(actual.constCurrentDir, expected.constCurrentDir);
	EXPECT_DOUBLE_EQ(actual.shearCurrent, expected.shearCurrent);
	EXPECT_DOUBLE_EQ(actual.shearCurrentDir, expected.shearCurrentDir);
	EXPECT_DOUBLE_EQ(actual.shearCurrentDepth, expected.shearCurrentDepth);
	EXPECT_DOUBLE_EQ(actual.profileCurrent, expected.profileCurrent);
	EXPECT_DOUBLE_EQ(actual.profileCurrentDir, expected.profileCurrentDir);
	EXPECT_DOUBLE_EQ(actual.profileCurrentExponent, expected.profileCurrentExponent);
	EXPECT_EQ(actual.sumPrint, expected.sumPrint);
	EXPECT_EQ(actual.saveComponents, expected.saveComponents);
	EXPECT_EQ(NormalizedPath(actual.savePath), NormalizedPath(expected.savePath));
	EXPECT_EQ(actual.saveName, expected.saveName);
}

void ExpectBladeAeroStructEqual(const BladeAeroStructInput &actual, const BladeAeroStructInput &expected)
{
	EXPECT_DOUBLE_EQ(actual.rayleighDamp, expected.rayleighDamp);
	EXPECT_DOUBLE_EQ(actual.stiffTuner, expected.stiffTuner);
	EXPECT_DOUBLE_EQ(actual.massTuner, expected.massTuner);
	EXPECT_EQ(actual.beamType, expected.beamType);
	EXPECT_EQ(actual.discCount, expected.discCount);
	EXPECT_EQ(actual.sectionRows, expected.sectionRows);
	ASSERT_EQ(actual.sections.size(), expected.sections.size());
	for (std::size_t i = 0; i < expected.sections.size(); ++i)
	{
		EXPECT_DOUBLE_EQ(actual.sections[i].radialPos, expected.sections[i].radialPos);
		EXPECT_DOUBLE_EQ(actual.sections[i].chord, expected.sections[i].chord);
		EXPECT_DOUBLE_EQ(actual.sections[i].twist, expected.sections[i].twist);
		EXPECT_DOUBLE_EQ(actual.sections[i].offsetY, expected.sections[i].offsetY);
		EXPECT_DOUBLE_EQ(actual.sections[i].offsetX, expected.sections[i].offsetX);
		EXPECT_DOUBLE_EQ(actual.sections[i].pitchAxisY, expected.sections[i].pitchAxisY);
		EXPECT_DOUBLE_EQ(actual.sections[i].pitchAxisX, expected.sections[i].pitchAxisX);
		EXPECT_EQ(actual.sections[i].polarFileId, expected.sections[i].polarFileId);
		EXPECT_EQ(actual.sections[i].polarFileToken, expected.sections[i].polarFileToken);
		EXPECT_DOUBLE_EQ(actual.sections[i].relThickness, expected.sections[i].relThickness);
	}
}

void ExpectTowerStructEqual(const TowerStructInput &actual, const TowerStructInput &expected)
{
	EXPECT_EQ(actual.sectionRows, expected.sectionRows);
}

void ExpectAirfoilDataEqual(const std::vector<AirfoilData> &actual, const std::vector<AirfoilData> &expected)
{
	ASSERT_EQ(actual.size(), expected.size());
	for (std::size_t i = 0; i < expected.size(); ++i)
	{
		SCOPED_TRACE("airfoil " + std::to_string(i));
		EXPECT_DOUBLE_EQ(actual[i].thickness, expected[i].thickness);
		EXPECT_DOUBLE_EQ(actual[i].reynoldsNumber, expected[i].reynoldsNumber);
		EXPECT_EQ(actual[i].reynoldsNumbers, expected[i].reynoldsNumbers);
		EXPECT_DOUBLE_EQ(actual[i].pitchMomentCenter, expected[i].pitchMomentCenter);
		EXPECT_EQ(actual[i].interpolationOrder, expected[i].interpolationOrder);
		ASSERT_EQ(actual[i].polar.size(), expected[i].polar.size());
		for (std::size_t r = 0; r < expected[i].polar.size(); ++r)
		{
			EXPECT_DOUBLE_EQ(actual[i].polar[r].alphaDeg, expected[i].polar[r].alphaDeg);
			EXPECT_DOUBLE_EQ(actual[i].polar[r].cl, expected[i].polar[r].cl);
			EXPECT_DOUBLE_EQ(actual[i].polar[r].cd, expected[i].polar[r].cd);
			EXPECT_DOUBLE_EQ(actual[i].polar[r].cm, expected[i].polar[r].cm);
		}
		ASSERT_EQ(actual[i].geometry.coordinates.size(), expected[i].geometry.coordinates.size());
		for (std::size_t r = 0; r < expected[i].geometry.coordinates.size(); ++r)
		{
			EXPECT_DOUBLE_EQ(actual[i].geometry.coordinates[r].x, expected[i].geometry.coordinates[r].x);
			EXPECT_DOUBLE_EQ(actual[i].geometry.coordinates[r].y, expected[i].geometry.coordinates[r].y);
		}
	}
}

template <typename T>
void TruncateVector(std::vector<T> &values, std::size_t maxCount)
{
	if (values.size() > maxCount)
		values.resize(maxCount);
}

void CompactResolvedForSimYamlTest(SimLResolvedInput &input)
{
	for (auto &airfoil : input.modules.aeroL.airfoilData)
	{
		TruncateVector(airfoil.polar, 6);
		for (auto &set : airfoil.polarSets)
			TruncateVector(set, 6);
		TruncateVector(airfoil.geometry.coordinates, 12);
	}
}

void ExpectSelfContainedSimYamlShape(const std::filesystem::path &simYaml,
                                     const SimLResolvedInput &expected)
{
	ASSERT_TRUE(std::filesystem::is_regular_file(simYaml));
	const YML simDoc(simYaml.string(), false);
	int qahseRootCount = 0;
	for (const auto &node : simDoc.nodeList)
		if (!node->parent && node->name == "Qahse")
			++qahseRootCount;
	EXPECT_EQ(qahseRootCount, 1);
	EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Information"));
	EXPECT_FALSE(simDoc.read("Information.YMLVersion").empty());
	EXPECT_FALSE(simDoc.read("Information.LastModifiedTime").empty());

	EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.SimL"));
	EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.AeroL"));
	EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.StrL"));
	EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.ControL"));
	EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.WindL"));
	EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.BladeAeroStruct"));
	EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.AeroL", "AirfoilData"));
	if (expected.modules.towerStruct)
		EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.TowerStruct"));
	if (expected.modules.hydroL)
	{
		EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.HydroL"));
		EXPECT_FALSE(module_io::YamlHasKey(simYaml.string(), "Qahse.HydroL", "Wamit"));
	}
	if (expected.modules.waveL)
		EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.WaveL"));
}

void ExpectSelfContainedSimYamlDataEqual(const std::filesystem::path &simYaml,
                                         const SimLResolvedInput &expected)
{
	auto expectedSim = expected.simL;
	expectedSim.strFile = simYaml.string();
	expectedSim.windFile = simYaml.string();
	expectedSim.aeroFile = simYaml.string();
	expectedSim.controlFile = simYaml.string();
	if (expected.modules.hydroL)
		expectedSim.hydroLFile = simYaml.string();
	ExpectSimEqual(ReadSimLInput(simYaml.string()), expectedSim);

	auto expectedAero = expected.modules.aeroL;
	expectedAero.bladeAeroStructFile = simYaml.string();
	expectedAero.bladeAeroStruct = expected.modules.bladeAeroStruct;
	const auto aeroFromSameFile = ReadAeroLInput(simYaml.string());
	ExpectAeroEqual(aeroFromSameFile, expectedAero);
	ExpectAirfoilDataEqual(aeroFromSameFile.airfoilData, expected.modules.aeroL.airfoilData);
	ASSERT_TRUE(aeroFromSameFile.bladeAeroStruct.has_value());
	ExpectBladeAeroStructEqual(*aeroFromSameFile.bladeAeroStruct, expected.modules.bladeAeroStruct);
	ExpectBladeAeroStructEqual(ReadBladeAeroStructInput(simYaml.string()), expected.modules.bladeAeroStruct);

	auto expectedStr = expected.modules.strL;
	expectedStr.bladeAeroStructFile = simYaml.string();
	if (expected.modules.towerStruct)
	{
		expectedStr.towerFile = simYaml.string();
		ExpectTowerStructEqual(ReadTowerStructInput(simYaml.string()), *expected.modules.towerStruct);
	}
	ExpectStrEqual(ReadStrLInput(simYaml.string()), expectedStr);
	ExpectControlEqual(ReadControLInput(simYaml.string()), expected.modules.controL);
	ExpectWindLEqual(windl_io_detail::ReadWindLInputFile(simYaml.string()), expected.modules.windL);

	if (expected.modules.hydroL)
	{
		auto expectedHydro = *expected.modules.hydroL;
		expectedHydro.waveLFile = simYaml.string();
		const auto hydroFromSameFile = ReadHydroLInput(simYaml.string());
		expectedHydro.wamit.reset();
		EXPECT_FALSE(hydroFromSameFile.wamit.has_value());
		ExpectHydroEqual(hydroFromSameFile, expectedHydro);
	}

	if (expected.modules.waveL)
		ExpectWaveLEqual(wavel_io_detail::ReadWaveLInputFile(simYaml.string()), *expected.modules.waveL);
}
} // namespace

TEST(SimLIO, ReadSemisubAndYamlRoundTrip)
{
	const auto sim = ReadSimLInput(SemisubMainFile().string());
	EXPECT_DOUBLE_EQ(sim.tMax, 600.0);
	EXPECT_TRUE(std::filesystem::is_regular_file(sim.strFile));
	EXPECT_TRUE(std::filesystem::is_regular_file(sim.hydroLFile));

	const auto yaml = TestOutputDir() / "SimL" / "siml.yaml";
	WriteSimLInput(sim, yaml.string());
	ExpectSimEqual(ReadSimLInput(yaml.string()), sim);
}

TEST(SimLIO, RecursivelyReadsCurrentSemisubReferences)
{
	const auto resolved = ReadSimLResolvedInputFile(SemisubMainFile().string());
	const auto &aero = resolved.modules.aeroL;
	const auto &str = resolved.modules.strL;
	ASSERT_TRUE(resolved.modules.hydroL.has_value());
	ASSERT_TRUE(resolved.modules.waveL.has_value());
	const auto &hydro = *resolved.modules.hydroL;
	const auto &control = resolved.modules.controL;
	const auto &wind = resolved.modules.windL;
	const auto &wave = *resolved.modules.waveL;

	EXPECT_FALSE(aero.airfoils.files.empty());
	EXPECT_TRUE(std::filesystem::is_regular_file(str.bladeAeroStructFile));
	EXPECT_TRUE(std::filesystem::is_regular_file(str.towerFile));
	EXPECT_TRUE(hydro.isFloating);
	EXPECT_DOUBLE_EQ(hydro.waterDepth, wave.waterDepth);
	EXPECT_EQ(control.pcMode, 1);
	EXPECT_EQ(static_cast<int>(wind.windType), 3);
	EXPECT_FALSE(resolved.modules.aeroL.airfoilData.empty());
	EXPECT_FALSE(resolved.modules.bladeAeroStruct.sections.empty());
	ASSERT_TRUE(resolved.modules.towerStruct.has_value());
	EXPECT_FALSE(resolved.modules.towerStruct->sectionRows.empty());
	ASSERT_TRUE(resolved.modules.hydroL->wamit.has_value());
	EXPECT_FALSE(resolved.modules.hydroL->wamit->radiation.radiation.empty());
}

TEST(SimLIO, LoadFromFileStoresResolvedModuleInputs)
{
	const auto sim = SimL::LoadFromFile(SemisubMainFile().string());
	EXPECT_DOUBLE_EQ(sim.Input().tMax, 600.0);
	EXPECT_FALSE(sim.Modules().aeroL.airfoilData.empty());
	EXPECT_FALSE(sim.Modules().bladeAeroStruct.sections.empty());
	ASSERT_TRUE(sim.Modules().hydroL.has_value());
	ASSERT_TRUE(sim.Modules().waveL.has_value());
	EXPECT_DOUBLE_EQ(sim.Modules().hydroL->waterDepth, sim.Modules().waveL->waterDepth);
	EXPECT_NE(sim.Summary().find("airfoils="), std::string::npos);
}

TEST(SimLIO, TextTemplateWriteUpdatesBoundFields)
{
	const auto dir = TestOutputDir() / "SimL" / "text_template";
	const auto str = dir / "str.dat";
	const auto wind = dir / "wind.dat";
	const auto aero = dir / "aero.dat";
	const auto control = dir / "control.dat";
	TouchFile(str);
	TouchFile(wind);
	TouchFile(aero);
	TouchFile(control);

	const auto inputPath = dir / "sim.hst";
	WriteLines(inputPath, {
		"600 TMax",
		"0.0125 DT",
		"false AfShowlog",
		"1 Dtput",
		"0 SimulateType",
		"0 RamupTime",
		"0 WTType",
		"1 Solver",
		"false Linearization",
		"9.81 Gravity",
		"1.225 AirDens",
		"1025 WtrDens",
		"1.464e-5 KinVisc",
		"335 SpdSound",
		QuotePath(str) + " StrFile",
		QuotePath(wind) + " WindFile",
		QuotePath(aero) + " AeroFile",
		QuotePath(control) + " ControlFile",
		"\"\" HydroLFile",
		"false AfVTK",
		"0.1 DT_VTK",
		"0 VTK_type",
		"12 VTK_SideNum",
		"0.05 DT_Out",
		"0 TStart",
		"0 OutType",
		"END",
	});

	auto sim = ReadSimLInput(inputPath.string());
	sim.tMax = 720.0;
	sim.solver = 2;
	const auto outPath = dir / "sim_written.hst";
	WriteSimLInput(sim, outPath.string(), inputPath.string());

	const auto written = ReadSimLInput(outPath.string());
	EXPECT_DOUBLE_EQ(written.tMax, 720.0);
	EXPECT_EQ(written.solver, 2);
	EXPECT_TRUE(std::filesystem::is_regular_file(written.aeroFile));
}

TEST(SimLIO, ReadAllSimLDemosRecursively)
{
	const auto cases = SimLCases();
	ASSERT_GE(cases.size(), 5u);

	for (const auto &simPath : cases)
	{
		SCOPED_TRACE(simPath.string());
		const auto resolved = ReadSimLResolvedInputFile(simPath.string());
		const auto &sim = resolved.simL;
		const auto &aero = resolved.modules.aeroL;
		const auto &str = resolved.modules.strL;
		const auto &control = resolved.modules.controL;
		const auto &wind = resolved.modules.windL;

		EXPECT_TRUE(std::filesystem::is_regular_file(aero.bladeAeroStructFile));
		EXPECT_TRUE(std::filesystem::is_regular_file(str.bladeAeroStructFile));
		EXPECT_TRUE(std::filesystem::is_regular_file(str.towerFile));
		EXPECT_GE(aero.airfoils.files.size(), 1u);
		EXPECT_EQ(resolved.modules.aeroL.airfoilData.size(), aero.airfoils.files.size());
		EXPECT_FALSE(resolved.modules.bladeAeroStruct.sections.empty());
		EXPECT_GE(str.output.blade.nodes.size(), static_cast<std::size_t>(str.output.blade.count));
		EXPECT_GE(str.output.tower.nodes.size(), static_cast<std::size_t>(str.output.tower.count));
		EXPECT_GE(static_cast<int>(wind.windType), 1);

		if (sim.wtType == 2)
		{
			ASSERT_TRUE(resolved.modules.hydroL.has_value());
			const auto &hydro = *resolved.modules.hydroL;
			EXPECT_GT(hydro.waterDepth, 0.0);
			if (!hydro.waveLFile.empty())
			{
				ASSERT_TRUE(resolved.modules.waveL.has_value());
				EXPECT_DOUBLE_EQ(hydro.waterDepth, resolved.modules.waveL->waterDepth);
			}
		}
		(void)control;
	}
}

TEST(SimLIO, ConvertsEveryDemoModelInputToYamlAndMatchesTextData)
{
	const auto cases = SimLCases();
	ASSERT_GE(cases.size(), 5u);

	for (const auto &simPath : cases)
	{
		SCOPED_TRACE(simPath.string());
		auto resolved = ReadSimLResolvedInputFile(simPath.string());
		CompactResolvedForSimYamlTest(resolved);
		const auto outDir = CaseYamlDir(simPath);

		const auto simYaml = outDir / "SimL.yml";
		WriteSimLInput(resolved.simL, simYaml.string());
		ExpectSimEqual(ReadSimLInput(simYaml.string()), resolved.simL);

		const auto aeroYaml = outDir / "AeroL.yml";
		WriteAeroLInput(resolved.modules.aeroL, aeroYaml.string());
		ExpectAeroEqual(ReadAeroLInput(aeroYaml.string()), resolved.modules.aeroL);

		const auto strYaml = outDir / "StrL.yml";
		WriteStrLInput(resolved.modules.strL, strYaml.string());
		ExpectStrEqual(ReadStrLInput(strYaml.string()), resolved.modules.strL);

		const auto controlYaml = outDir / "ControL.yml";
		WriteControLInput(resolved.modules.controL, controlYaml.string());
		ExpectControlEqual(ReadControLInput(controlYaml.string()), resolved.modules.controL);

		const auto windYaml = outDir / "WindL.yml";
		windl_io_detail::WriteWindLInputYaml(resolved.modules.windL, windYaml.string());
		ExpectWindLEqual(windl_io_detail::ReadWindLInputFile(windYaml.string()), resolved.modules.windL);

		const auto bladeYaml = outDir / "BladeAeroStruct.yml";
		WriteBladeAeroStructInputYaml(resolved.modules.bladeAeroStruct, bladeYaml.string());
		ExpectBladeAeroStructEqual(ReadBladeAeroStructInput(bladeYaml.string()), resolved.modules.bladeAeroStruct);

		if (resolved.modules.towerStruct)
		{
			const auto towerYaml = outDir / "TowerStruct.yml";
			WriteTowerStructInputYaml(*resolved.modules.towerStruct, towerYaml.string());
			ExpectTowerStructEqual(ReadTowerStructInput(towerYaml.string()), *resolved.modules.towerStruct);
		}

		if (resolved.modules.hydroL)
		{
			const auto hydroYaml = outDir / "HydroL.yml";
			WriteHydroLInput(*resolved.modules.hydroL, hydroYaml.string());
			ExpectHydroEqual(ReadHydroLInput(hydroYaml.string()), *resolved.modules.hydroL);
		}

		if (resolved.modules.waveL)
		{
			const auto waveYaml = outDir / "WaveL.yml";
			wavel_io_detail::WriteWaveLInputYaml(*resolved.modules.waveL, waveYaml.string());
			ExpectWaveLEqual(wavel_io_detail::ReadWaveLInputFile(waveYaml.string()), *resolved.modules.waveL);
		}
	}
}

TEST(SimLIO, WritesSelfContainedSimYamlAndModulesReadFromIt)
{
	const std::vector<std::filesystem::path> cases{SemisubMainFile()};

	for (const auto &simPath : cases)
	{
		SCOPED_TRACE(simPath.string());
		const auto resolved = ReadSimLResolvedInputFile(simPath.string());
		const auto outDir = CaseYamlDir(simPath);
		const auto simYaml = outDir / (simPath.stem().string() + ".sim");
		WriteSimLResolvedInputFile(resolved, simYaml.string());

		ExpectSelfContainedSimYamlShape(simYaml, resolved);

		const auto fromSim = ReadSimLResolvedInputFile(simYaml.string());
		EXPECT_DOUBLE_EQ(fromSim.simL.tMax, resolved.simL.tMax);
		EXPECT_DOUBLE_EQ(fromSim.simL.dt, resolved.simL.dt);
		EXPECT_EQ(fromSim.simL.wtType, resolved.simL.wtType);
		EXPECT_EQ(fromSim.simL.solver, resolved.simL.solver);

		ExpectAirfoilDataEqual(fromSim.modules.aeroL.airfoilData, resolved.modules.aeroL.airfoilData);
		ExpectBladeAeroStructEqual(fromSim.modules.bladeAeroStruct, resolved.modules.bladeAeroStruct);
		if (resolved.modules.towerStruct)
		{
			ASSERT_TRUE(fromSim.modules.towerStruct.has_value());
			ExpectTowerStructEqual(*fromSim.modules.towerStruct, *resolved.modules.towerStruct);
		}
		ExpectWindLEqual(fromSim.modules.windL, resolved.modules.windL);

		const auto aeroFromSameFile = ReadAeroLInput(simYaml.string());
		ExpectAirfoilDataEqual(aeroFromSameFile.airfoilData, resolved.modules.aeroL.airfoilData);
		ExpectBladeAeroStructEqual(ReadBladeAeroStructInput(simYaml.string()), resolved.modules.bladeAeroStruct);

		if (resolved.modules.hydroL)
		{
			EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.HydroL"));
			EXPECT_FALSE(module_io::YamlHasKey(simYaml.string(), "Qahse.HydroL", "Wamit"));
			const auto hydroFromSameFile = ReadHydroLInput(simYaml.string());
			EXPECT_FALSE(hydroFromSameFile.wamit.has_value());
			auto expectedHydro = *resolved.modules.hydroL;
			expectedHydro.waveLFile = simYaml.string();
			expectedHydro.wamit.reset();
			ExpectHydroEqual(hydroFromSameFile, expectedHydro);
		}
		if (resolved.modules.waveL)
		{
			EXPECT_TRUE(module_io::YamlHasKey(simYaml.string(), "Qahse.WaveL"));
			ExpectWaveLEqual(wavel_io_detail::ReadWaveLInputFile(simYaml.string()), *resolved.modules.waveL);
		}
	}
}

TEST(SimLIO, ConvertsAllDemoMainFilesToAdjacentSimAndVerifies)
{
	const auto cases = SimLCases();
	ASSERT_GE(cases.size(), 5u);

	for (const auto &simPath : cases)
	{
		SCOPED_TRACE(simPath.string());
		const auto resolved = ReadSimLResolvedInputFile(simPath.string());
		auto simYaml = simPath;
		simYaml.replace_extension(".sim");
		WriteSimLResolvedInputFile(resolved, simYaml.string());

		ExpectSelfContainedSimYamlShape(simYaml, resolved);
		ExpectSelfContainedSimYamlDataEqual(simYaml, resolved);
	}
}

TEST(SimLIO, ValidationRejectsMissingReferencedStrFile)
{
	const auto dir = TestOutputDir() / "SimL" / "validation";
	const auto wind = dir / "wind.dat";
	const auto aero = dir / "aero.dat";
	const auto control = dir / "control.dat";
	TouchFile(wind);
	TouchFile(aero);
	WriteLines(control, {
		"0 PCMode",
		"END",
	});

	const auto path = dir / "sim_missing_str.hst";
	WriteLines(path, {
		"10 TMax",
		"0.01 DT",
		"0 WTType",
		"missing_str.dat StrFile",
		QuotePath(wind) + " WindFile",
		QuotePath(aero) + " AeroFile",
		QuotePath(control) + " ControlFile",
		"END",
	});
	EXPECT_THROW((void)ReadSimLInput(path.string()), std::runtime_error);
}
