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

	const auto yaml = TestOutputDir() / "StrL" / "strl.yaml";
	WriteStrLInput(str, yaml.string());
	ExpectStrEqual(ReadStrLInput(yaml.string()), str);
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
	EXPECT_DOUBLE_EQ(blade2.stiffTuner, blade.stiffTuner);
	EXPECT_DOUBLE_EQ(blade2.massTuner, blade.massTuner);
	EXPECT_EQ(blade2.beamType, blade.beamType);
	EXPECT_EQ(blade2.discCount, blade.discCount);
	EXPECT_EQ(blade2.sectionRows, blade.sectionRows);
	ASSERT_EQ(blade2.sections.size(), blade.sections.size());
	EXPECT_DOUBLE_EQ(blade2.sections.front().radialPos, blade.sections.front().radialPos);
	EXPECT_DOUBLE_EQ(blade2.sections.back().chord, blade.sections.back().chord);

	const auto tower2 = ReadTowerStructInput(towerYaml.string());
	EXPECT_EQ(tower2.sectionRows, tower.sectionRows);
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
		"3 NumBld",
		"5.0 OverHang",
		"5.0 ShaftTilt",
		"2.5 PreCone",
		"0.0 Azimuth",
		"true DrivetrainDof",
		"1000 DTTorSpr",
		"20 DTTorDmp",
		"3 BladeNum",
		QuotePath(blade) + " BladeAeroStructFile",
		"77.6 TowerHeight",
		QuotePath(tower) + " TowerFile",
		"\"\" SubFile",
		"true SumPrint",
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
		"BladeAeroStructTable",
		"1.5 3.0 10.0 0.0 0.0 0.5 0.0 Polars/example.plr 100.0",
		"END",
	});

	EXPECT_THROW((void)ReadBladeAeroStructInput(path.string()), std::runtime_error);
}
