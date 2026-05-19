#include "../ModuleIOTestHelpers.hpp"

#include <filesystem>
#include <stdexcept>

#include <gtest/gtest.h>

#include "StrL/IO/StrL_IO_Subs.hpp"

using namespace module_io_test;

namespace
{
std::filesystem::path SemisubStrFile()
{
	return SemisubRoot() / "StrL" / "Qahse_StrL_Main_NREL_5MW_OC4_Semisub.dat";
}
} // namespace

TEST(StrLIO, ReadSemisubAndYamlRoundTrip)
{
	const auto str = ReadStrLInput(SemisubStrFile().string());
	EXPECT_TRUE(std::filesystem::is_regular_file(str.bladeAeroStructFile));
	EXPECT_TRUE(std::filesystem::is_regular_file(str.towerFile));
	EXPECT_EQ(str.towerNum, 1);
	EXPECT_DOUBLE_EQ(str.towerYdeg, 90.0);
	EXPECT_DOUBLE_EQ(str.hubRadius, 1.5);
	EXPECT_DOUBLE_EQ(str.twr2Shft, 1.96256);
	EXPECT_DOUBLE_EQ(str.hubMass, 56780.0);
	EXPECT_DOUBLE_EQ(str.hubIner, 115926.0);
	EXPECT_DOUBLE_EQ(str.gravity, 9.81);
	EXPECT_DOUBLE_EQ(str.rotSpeed, 12.1);
	EXPECT_DOUBLE_EQ(str.nacMass, 240000.0);
	EXPECT_DOUBLE_EQ(str.gearboxRatio, 97.0);
	EXPECT_DOUBLE_EQ(str.genIner, 534.116);
	EXPECT_EQ(str.output.bldOutSig, std::vector<int>({0, 2}));
	EXPECT_EQ(str.output.twrOutSig, std::vector<int>({0}));

	const auto yaml = TestOutputDir() / "StrL" / "strl.yaml";
	WriteStrLInput(str, yaml.string());
	const auto fromYaml = ReadStrLInput(yaml.string());
	ExpectStrEqual(fromYaml, str);
	EXPECT_EQ(fromYaml.towerNum, 1);
	EXPECT_DOUBLE_EQ(fromYaml.towerYdeg, 90.0);
}

TEST(StrLIO, BladeAndTowerStructFilesReadTables)
{
	const auto str = ReadStrLInput(SemisubStrFile().string());
	const auto blade = ReadBladeAeroStructInput(str.bladeAeroStructFile);
	const auto tower = ReadTowerStructInput(str.towerFile);

	ASSERT_FALSE(blade.sections.empty());
	EXPECT_GT(blade.sections.front().radialPos, 0.0);
	EXPECT_GT(blade.sections.front().chord, 0.0);
	EXPECT_GE(blade.sections.front().polarFileId, 1);
	EXPECT_FALSE(blade.sections.front().polarFileToken.empty());
	EXPECT_GE(blade.sectionRows.front().size(), 20u);
	ASSERT_FALSE(tower.sectionRows.empty());
	EXPECT_GE(tower.sectionRows.front().size(), 10u);
}

TEST(StrLIO, BladeAndTowerYamlRoundTripPreservesTables)
{
	const auto str = ReadStrLInput(SemisubStrFile().string());
	const auto blade = ReadBladeAeroStructInput(str.bladeAeroStructFile);
	const auto tower = ReadTowerStructInput(str.towerFile);

	const auto bladeYaml = TestOutputDir() / "StrL" / "blade_aero_struct.yaml";
	const auto towerYaml = TestOutputDir() / "StrL" / "tower_struct.yaml";
	WriteBladeAeroStructInputYaml(blade, bladeYaml.string());
	WriteTowerStructInputYaml(tower, towerYaml.string());

	const auto blade2 = ReadBladeAeroStructInput(bladeYaml.string());
	EXPECT_DOUBLE_EQ(blade2.rayleighDamp, blade.rayleighDamp);
	EXPECT_EQ(blade2.rayleighDampAniso, blade.rayleighDampAniso);
	EXPECT_DOUBLE_EQ(blade2.stiffTuner, blade.stiffTuner);
	EXPECT_DOUBLE_EQ(blade2.massTuner, blade.massTuner);
	EXPECT_EQ(blade2.beamType, blade.beamType);
	EXPECT_EQ(blade2.discCount, blade.discCount);
	EXPECT_EQ(blade2.sectionRows, blade.sectionRows);
	ASSERT_EQ(blade2.sections.size(), blade.sections.size());
	EXPECT_DOUBLE_EQ(blade2.sections.front().radialPos, blade.sections.front().radialPos);
	EXPECT_DOUBLE_EQ(blade2.sections.back().chord, blade.sections.back().chord);

	const auto tower2 = ReadTowerStructInput(towerYaml.string());
	EXPECT_DOUBLE_EQ(tower2.rayleighDamp, tower.rayleighDamp);
	EXPECT_DOUBLE_EQ(tower2.stiffTuner, tower.stiffTuner);
	EXPECT_DOUBLE_EQ(tower2.massTuner, tower.massTuner);
	EXPECT_EQ(tower2.beamType, tower.beamType);
	EXPECT_EQ(tower2.discCount, tower.discCount);
	EXPECT_EQ(tower2.sectionRows, tower.sectionRows);
}

TEST(StrLIO, ReadsAllDemoTowerMetadataAndFpmBladeDamping)
{
	for (const auto &simPath : SimLCases())
	{
		SCOPED_TRACE(simPath.string());
		const auto str = ReadStrLInput((simPath.parent_path() / "StrL" /
			("Qahse_StrL_Main_" + simPath.parent_path().filename().string() + ".dat")).string());
		const auto tower = ReadTowerStructInput(str.towerFile);
		EXPECT_GT(tower.discCount, 0);
		EXPECT_GT(tower.stiffTuner, 0.0);
		EXPECT_GT(tower.massTuner, 0.0);
		if (std::filesystem::path(str.towerFile).filename().string().find("IEA_22") != std::string::npos)
			EXPECT_EQ(tower.beamType, 1);

		const auto blade = ReadBladeAeroStructInput(str.bladeAeroStructFile);
		if (std::filesystem::path(str.bladeAeroStructFile).filename().string().find("FPM") != std::string::npos)
			EXPECT_EQ(blade.rayleighDampAniso.size(), 5u);
	}
}

TEST(StrLIO, AllDemoBladeAeroStructTablesHaveConsistentRowColumnCounts)
{
	std::size_t checked = 0;
	const auto root = RepoRoot() / "demo" / "SimL";
	for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
	{
		if (!entry.is_regular_file())
			continue;
		const auto path = entry.path();
		const auto filename = path.filename().string();
		if (filename.find("Blade") == std::string::npos ||
		    filename.find("_new.str") == std::string::npos)
			continue;

		SCOPED_TRACE(path.string());
		const auto blade = ReadBladeAeroStructInput(path.string());
		ASSERT_FALSE(blade.sectionRows.empty());
		ASSERT_EQ(blade.sections.size(), blade.sectionRows.size());

		const std::size_t expectedColumns = blade.beamType == 2 ? 54u : 26u;
		for (const auto &row : blade.sectionRows)
			EXPECT_EQ(row.size(), expectedColumns);
		for (const auto &section : blade.sections)
		{
			EXPECT_GT(section.radialPos, 0.0);
			EXPECT_GT(section.chord, 0.0);
			EXPECT_GE(section.polarFileId, 1);
		}
		++checked;
	}
	EXPECT_GT(checked, 0u);
}

TEST(StrLIO, TextTemplateWriteUpdatesBoundFields)
{
	const auto dir = TestOutputDir() / "StrL" / "text_template";
	const auto blade = dir / "blade.str";
	const auto tower = dir / "tower.str";
	TouchFile(blade);
	TouchFile(tower);

	const auto inputPath = dir / "str.dat";
	WriteLines(inputPath, {
		"2 TowerNum",
		"180 TowerYdeg",
		"3 NumBld",
		"1.5 HubRad",
		"5.0 OverHang",
		"5.0 ShaftTilt",
		"2.5 PreCone",
		"1.2 Twr2Shft",
		"10 HubMass",
		"20 HubIner",
		"9.81 Gravity",
		"0.0 Azimuth",
		"0.0 AzimB1Up",
		"12.1 RotSpeed",
		"0.0 NacYaw",
		"11 NACCAX",
		"6 NACCAY",
		"6 NACCAZ",
		"0.6 NACCDX",
		"0.6 NACCDY",
		"0.6 NACCDZ",
		"0 YawBrMass",
		"240000 NacMass",
		"1.9 NacCmX",
		"0 NacCmY",
		"1.75 NacCmZ",
		"2607890 NacYawIner",
		"97 GearboxRatio",
		"1 GearboxEff",
		"true DrivetrainDof",
		"534.116 GenIner",
		"1000 DTTorSpr",
		"20 DTTorDmp",
		"3 BladeNum",
		QuotePath(blade) + " BladeAeroStructFile",
		"77.6 TowerHeight",
		QuotePath(tower) + " TowerFile",
		"\"\" SubFile",
		"true SumPrint",
		"0,2 BldOutSig",
		"0,1 TwrOutSig",
		"1 NBlOuts",
		"0 BlOutNd",
		"1 NTwOuts",
		"0 TwOutNd",
		"END",
	});

	auto str = ReadStrLInput(inputPath.string());
	str.rotorOverhang = 6.25;
	str.output.blade.nodes = {0};
	const auto outPath = dir / "str_written.dat";
	WriteStrLInput(str, outPath.string(), inputPath.string());

	const auto written = ReadStrLInput(outPath.string());
	EXPECT_DOUBLE_EQ(written.rotorOverhang, 6.25);
	EXPECT_EQ(written.towerNum, 2);
	EXPECT_DOUBLE_EQ(written.towerYdeg, 180.0);
	EXPECT_DOUBLE_EQ(written.hubRadius, 1.5);
	EXPECT_EQ(written.output.twrOutSig, std::vector<int>({0, 1}));
	EXPECT_EQ(written.output.blade.nodes, std::vector<int>({0}));
}

TEST(StrLIO, ValidationRejectsOutputNodeCountMismatch)
{
	const auto dir = TestOutputDir() / "StrL" / "validation";
	const auto blade = dir / "blade.str";
	const auto tower = dir / "tower.str";
	TouchFile(blade);
	TouchFile(tower);

	const auto path = dir / "str_bad_output.dat";
	WriteLines(path, {
		"3 NumBld",
		QuotePath(blade) + " BladeAeroStructFile",
		"77 TowerHeight",
		QuotePath(tower) + " TowerFile",
		"2 NBlOuts",
		"0 BlOutNd",
		"END",
	});
	EXPECT_THROW((void)ReadStrLInput(path.string()), std::runtime_error);
}

TEST(StrLIO, BladePolarFileIdRejectsQBladePathToken)
{
	const auto dir = TestOutputDir() / "StrL" / "polar_id_validation";
	const auto path = dir / "blade_bad_polar_id.str";
	WriteLines(path, {
		"0.0 BladeRayleighDamp",
		"1.0 BladeStiffTuner",
		"1.0 BladeMassTuner",
		"1 BladeBeamType",
		"1 BladeDiscCount",
		"RadialPos Chord Twist OffsetY OffsetX PitchAxisY PitchAxisX PolarFileID RelThickness",
		"1.5 3.0 10.0 0.0 0.0 0.5 0.0 Polars/example.plr 100.0",
		"END",
	});

	EXPECT_THROW((void)ReadBladeAeroStructInput(path.string()), std::runtime_error);
}
