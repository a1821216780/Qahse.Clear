#include "../ModuleIOTestHelpers.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "HydroL/IO/HydroL_IO_Subs.hpp"
#include "HydroL/Wamit.hpp"
#include "WaveL/IO/WaveL_IO_Subs.hpp"

using namespace module_io_test;

namespace
{
std::filesystem::path SemisubHydroFile()
{
	return SemisubRoot() / "HydroL" / "Qahse_HydroL_Main_NREL_5MW_OC4_Semisub.dat";
}
} // namespace

TEST(HydroLIO, ReadSemisubAndYamlRoundTrip)
{
	const auto hydro = ReadHydroLInput(SemisubHydroFile().string());
	const auto wave = wavel_io_detail::ReadWaveLInputFile(hydro.waveLFile);
	EXPECT_TRUE(hydro.isFloating);
	EXPECT_DOUBLE_EQ(hydro.waterDepth, wave.waterDepth);
	EXPECT_DOUBLE_EQ(hydro.unitLengthWamit, 1.0);
	EXPECT_DOUBLE_EQ(hydro.diffractionOffset, 0.0);
	EXPECT_DOUBLE_EQ(hydro.deltaTIrf, 0.025);
	EXPECT_FALSE(hydro.constrainedFloater);
	ASSERT_EQ(hydro.tpOrientation.rows(), 2);
	ASSERT_EQ(hydro.tpOrientation.cols(), 3);
	EXPECT_DOUBLE_EQ(hydro.tpOrientation(0, 0), 1.0);
	EXPECT_DOUBLE_EQ(hydro.tpOrientation(1, 1), 1.0);

	const auto yaml = TestOutputDir() / "HydroL" / "hydrol.yaml";
	WriteHydroLInput(hydro, yaml.string());
	ExpectHydroEqual(ReadHydroLInput(yaml.string()), hydro);
}

TEST(HydroLIO, ReadsVariantSpecificMassAndOutputBlocks)
{
	const auto root = RepoRoot() / "demo" / "SimL";
	const auto mono = ReadHydroLInput((root / "IEA_22MW_Monopile" / "HydroL" /
		"Qahse_HydroL_Main_IEA_22MW_Monopile.dat").string());
	EXPECT_EQ(mono.transitionMass, std::vector<std::string>({"ADDMASS_10"}));

	const auto spar = ReadHydroLInput((root / "NREL_5MW_OC3_Spar" / "HydroL" /
		"Qahse_HydroL_Main_NREL_5MW_OC3_Spar.dat").string());
	EXPECT_EQ(spar.outputPoints.size(), 9u);
	EXPECT_EQ(spar.outputPoints.front(), "SUB_1_0.2");
	EXPECT_EQ(spar.outputPoints.back(), "MOO_3_1.0");

	const auto yaml = TestOutputDir() / "HydroL" / "hydrol_variant.yaml";
	WriteHydroLInput(spar, yaml.string());
	ExpectHydroEqual(ReadHydroLInput(yaml.string()), spar);
}

TEST(HydroLIO, ParsesReferencedWamitFiles)
{
	const auto hydro = ReadHydroLInput(SemisubHydroFile().string());
	const auto data = ReadHydroLWamitFiles(hydro);

	ASSERT_GT(data.radiation.radiation.size(), 8000u);
	EXPECT_EQ(data.radiation.type, WamitFileType::RADIATION);
	EXPECT_DOUBLE_EQ(data.radiation.radiation.front().period, -1.0);
	EXPECT_EQ(data.radiation.radiation.front().row, 1);
	EXPECT_EQ(data.radiation.radiation.front().column, 1);
	EXPECT_NEAR(data.radiation.radiation.front().addedMass, 8.526887e3, 1.0e-6);
	EXPECT_FALSE(data.radiation.radiation.front().hasDamping);
	const auto firstDamped = std::find_if(data.radiation.radiation.begin(), data.radiation.radiation.end(),
	                                      [](const WamitRadiationEntry &entry) { return entry.hasDamping; });
	ASSERT_NE(firstDamped, data.radiation.radiation.end());
	EXPECT_NEAR(firstDamped->damping, 1.604159e-2, 1.0e-10);

	ASSERT_GT(data.excitation.excitation.size(), 100000u);
	EXPECT_EQ(data.excitation.type, WamitFileType::EXCITATION);
	EXPECT_DOUBLE_EQ(data.excitation.excitation.front().period, 628.319);
	EXPECT_DOUBLE_EQ(data.excitation.excitation.front().headingDeg, -180.0);
	EXPECT_EQ(data.excitation.excitation.front().dof, 1);
	EXPECT_NEAR(data.excitation.excitation.front().magnitude, 5.067605, 1.0e-6);
	EXPECT_NEAR(data.excitation.excitation.front().phaseDeg, -90.19937, 1.0e-5);

	ASSERT_GT(data.difference.qtf.size(), 9000u);
	EXPECT_EQ(data.difference.type, WamitFileType::DIFFERENCE_QTF);
	EXPECT_DOUBLE_EQ(data.difference.qtf.front().period1, 25.133);
	EXPECT_DOUBLE_EQ(data.difference.qtf.front().period2, 25.133);
	EXPECT_EQ(data.difference.qtf.front().dof, 1);
	EXPECT_NEAR(data.difference.qtf.front().magnitude, 0.427149, 1.0e-6);

	ASSERT_GT(data.sum.qtf.size(), 9000u);
	EXPECT_EQ(data.sum.type, WamitFileType::SUM_QTF);
	EXPECT_DOUBLE_EQ(data.sum.qtf.front().period1, 25.133);
	EXPECT_DOUBLE_EQ(data.sum.qtf.front().period2, 25.133);
	EXPECT_EQ(data.sum.qtf.front().dof, 1);
	EXPECT_NEAR(data.sum.qtf.front().magnitude, 1.93304, 1.0e-5);
}

TEST(HydroLIO, TextTemplateWriteUpdatesBoundFields)
{
	const auto dir = TestOutputDir() / "HydroL" / "text_template";
	const auto wave = dir / "wave.dat";
	TouchFile(wave);

	const auto inputPath = dir / "hydro.dat";
	WriteLines(inputPath, {
		"200 WaterDepth",
		"1025 WaterDensity",
		"true IsFloating",
		"0 AdvancedBuoyancy",
		"0 WaveKinEvalMorison",
		"1 WaveKinEvalPotential",
		"30 WaveKinTau",
		QuotePath(wave) + " WaveLFile",
		"true StaticBuoyancy",
		"false UseRadiation",
		"false UseRadAddedMass",
		"false UseExcitation",
		"0 DiffEvalType",
		"false UseSumFreqs",
		"1 BuoyancyTuner",
		"1 StiffTuner",
		"1 MassTuner",
		"1 BeamType",
		"JointOffset",
		"0 0 0",
		"------",
		"SubJoints",
		"1 0 0 -1",
		"2 0 0 0",
		"------",
		"HydroMemberCoeff",
		"1 2 0.8 1 0",
		"------",
		"SubMembers",
		"1 1 2 1 0 1 1 0 0 2 MainMember",
		"------",
		"END",
	});

	auto hydro = ReadHydroLInput(inputPath.string());
	hydro.waterDepth = 210.0;
	hydro.beamType = 2;
	const auto outPath = dir / "hydro_written.dat";
	WriteHydroLInput(hydro, outPath.string(), inputPath.string());

	const auto written = ReadHydroLInput(outPath.string());
	EXPECT_DOUBLE_EQ(written.waterDepth, 210.0);
	EXPECT_EQ(written.beamType, 2);
	EXPECT_EQ(written.subJoints.size(), 2u);
	EXPECT_EQ(written.subMembers.size(), 1u);
}

TEST(HydroLIO, ReadSaveVariants)
{
	const auto root = RepoRoot() / "demo" / "SimL" / "save";
	const std::vector<std::string> files = {
		"NREL_5MW_OC3_Spar_Sub.str",
		"NREL_5MW_OC4_Jacket_Sub.str",
		"IEA15MW_UMaineSemi_Sub.str",
		"IEA_22_280_RWT_MONO_Sub.str",
	};

	for (const auto &file : files)
	{
		SCOPED_TRACE(file);
		const auto input = ReadHydroLInput((root / file).string());
		EXPECT_GT(input.waterDepth, 0.0);
		EXPECT_FALSE(input.subJoints.empty());
	}
}

TEST(HydroLIO, ValidationRejectsMissingPotentialWhenRadiationEnabled)
{
	const auto dir = TestOutputDir() / "HydroL" / "validation";
	const auto wave = dir / "wave.dat";
	TouchFile(wave);

	const auto path = dir / "hydro_missing_potential.dat";
	WriteLines(path, {
		"200 WaterDepth",
		QuotePath(wave) + " WaveLFile",
		"true UseRadiation",
		"missing.1 PotentialRadFile",
		"0 DiffEvalType",
		"false UseSumFreqs",
		"END",
	});
	EXPECT_THROW((void)ReadHydroLInput(path.string()), std::runtime_error);
}
