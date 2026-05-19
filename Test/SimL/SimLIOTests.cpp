#include "../ModuleIOTestHelpers.hpp"

#include <filesystem>
#include <stdexcept>

#include <gtest/gtest.h>

#include "AeroL/IO/AeroL_IO_Subs.hpp"
#include "ControL/IO/ControL_IO_Subs.hpp"
#include "HydroL/IO/HydroL_IO_Subs.hpp"
#include "SimL/IO/SimL_IO_Subs.hpp"
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
	const auto sim = ReadSimLInput(SemisubMainFile().string());
	const auto aero = ReadAeroLInput(sim.aeroFile);
	const auto str = ReadStrLInput(sim.strFile);
	const auto hydro = ReadHydroLInput(sim.hydroLFile);
	const auto control = ReadControLInput(sim.controlFile);
	const auto wind = windl_io_detail::ReadWindLInputFile(sim.windFile);
	const auto wave = wavel_io_detail::ReadWaveLInputFile(hydro.waveLFile);

	EXPECT_FALSE(aero.airfoils.files.empty());
	EXPECT_TRUE(std::filesystem::is_regular_file(str.bladeAeroStructFile));
	EXPECT_TRUE(std::filesystem::is_regular_file(str.towerFile));
	EXPECT_TRUE(hydro.isFloating);
	EXPECT_DOUBLE_EQ(hydro.waterDepth, wave.waterDepth);
	EXPECT_EQ(control.pcMode, 1);
	EXPECT_EQ(static_cast<int>(wind.windType), 3);
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
		const auto sim = ReadSimLInput(simPath.string());
		const auto aero = ReadAeroLInput(sim.aeroFile);
		const auto str = ReadStrLInput(sim.strFile);
		const auto control = ReadControLInput(sim.controlFile);
		const auto wind = windl_io_detail::ReadWindLInputFile(sim.windFile);

		EXPECT_TRUE(std::filesystem::is_regular_file(aero.bladeAeroStructFile));
		EXPECT_TRUE(std::filesystem::is_regular_file(str.bladeAeroStructFile));
		EXPECT_TRUE(std::filesystem::is_regular_file(str.towerFile));
		EXPECT_GE(aero.airfoils.files.size(), 1u);
		EXPECT_GE(str.output.blade.nodes.size(), static_cast<std::size_t>(str.output.blade.count));
		EXPECT_GE(str.output.tower.nodes.size(), static_cast<std::size_t>(str.output.tower.count));
		EXPECT_GE(static_cast<int>(wind.windType), 1);

		if (sim.wtType == 2)
		{
			const auto hydro = ReadHydroLInput(sim.hydroLFile);
			EXPECT_GT(hydro.waterDepth, 0.0);
			if (!hydro.waveLFile.empty())
			{
				const auto wave = wavel_io_detail::ReadWaveLInputFile(hydro.waveLFile);
				EXPECT_DOUBLE_EQ(hydro.waterDepth, wave.waterDepth);
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
