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
