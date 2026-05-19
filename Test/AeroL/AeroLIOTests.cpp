#include "../ModuleIOTestHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>

#include <gtest/gtest.h>

#include "AeroL/Airfoil.hpp"
#include "AeroL/IO/AeroL_IO_Subs.hpp"
#include "SimL/IO/SimL_IO_Subs.hpp"
#include "StrL/IO/StrL_IO_Subs.hpp"

using namespace module_io_test;

namespace
{
std::filesystem::path SemisubAeroFile()
{
	return SemisubRoot() / "AeroL" / "Qahse_AeroL_Main_NREL_5MW_OC4_Semisub.dat";
}
} // namespace

TEST(AeroLIO, ReadSemisubAndYamlRoundTrip)
{
	const auto aero = ReadAeroLInput(SemisubAeroFile().string());
	EXPECT_FALSE(aero.airfoils.files.empty());
	EXPECT_TRUE(std::filesystem::is_regular_file(aero.bladeAeroStructFile));

	const auto yaml = TestOutputDir() / "AeroL" / "aerol.yaml";
	WriteAeroLInput(aero, yaml.string());
	ExpectAeroEqual(ReadAeroLInput(yaml.string()), aero);
}

TEST(AeroLIO, ParsesAirfoilPolarAndGeometryFiles)
{
	const auto aero = ReadAeroLInput(SemisubAeroFile().string());
	const auto airfoils = ReadAeroLAirfoilFiles(aero);
	ASSERT_EQ(airfoils.size(), aero.airfoils.files.size());

	const auto &du21 = airfoils.at(6);
	EXPECT_DOUBLE_EQ(du21.thickness, 21.0);
	EXPECT_DOUBLE_EQ(du21.reynoldsNumber, 0.75);
	EXPECT_TRUE(du21.polarName.empty());
	EXPECT_TRUE(du21.airfoilName.empty());
	EXPECT_EQ(du21.declaredPolarCount, 142);
	ASSERT_EQ(du21.polar.size(), 142u);
	EXPECT_DOUBLE_EQ(du21.polar.front().alphaDeg, -180.0);
	EXPECT_DOUBLE_EQ(du21.polar.front().cd, 0.0185);
	const auto zeroAlpha = std::find_if(du21.polar.begin(), du21.polar.end(), [](const AirfoilPolarPoint &point) {
		return std::abs(point.alphaDeg) < 1.0e-12;
	});
	ASSERT_NE(zeroAlpha, du21.polar.end());
	EXPECT_NEAR(zeroAlpha->cl, 0.521, 1.0e-12);
	EXPECT_DOUBLE_EQ(du21.polar.back().alphaDeg, 180.0);
	EXPECT_TRUE(std::filesystem::is_regular_file(du21.geometryFile));
	EXPECT_EQ(du21.geometry.declaredCoordinateCount, static_cast<int>(du21.geometry.coordinates.size() + 1));
	EXPECT_TRUE(du21.geometry.hasExplicitReference);
	EXPECT_NEAR(du21.geometry.reference.x, 0.25, 1.0e-12);
	ASSERT_GT(du21.geometry.coordinates.size(), 300u);
	EXPECT_DOUBLE_EQ(du21.geometry.coordinates.front().x, 1.0);
	ASSERT_EQ(du21.lookup.alphaDeg.size(), du21.polar.size());
	EXPECT_NE(du21.lookup.interpolators, nullptr);
}

TEST(AeroLIO, AirfoilLookupInterpolatesFromCachedColumns)
{
	const auto aero = ReadAeroLInput(SemisubAeroFile().string());
	const auto airfoils = ReadAeroLAirfoilFiles(aero);
	const auto &du21 = airfoils.at(6);

	const auto interpolated = EvaluateAirfoilCoefficients(du21, -177.5);
	EXPECT_NEAR(interpolated.cl, 0.197, 1.0e-12);
	EXPECT_NEAR(interpolated.cd, 0.02585, 1.0e-12);
	EXPECT_NEAR(interpolated.cm, 0.0989, 1.0e-12);
	EXPECT_NEAR(interpolated.dClDAlpha, 0.0788, 1.0e-12);
	EXPECT_NEAR(interpolated.dCdDAlpha, 0.00294, 1.0e-12);
	EXPECT_NEAR(interpolated.dCmDAlpha, 0.03956, 1.0e-12);
}

TEST(AeroLIO, BladePolarFileIdsReferToAeroLAirfoilList)
{
	const auto aero = ReadAeroLInput(SemisubAeroFile().string());
	const auto blade = ReadBladeAeroStructInput(aero.bladeAeroStructFile);
	ASSERT_FALSE(blade.sections.empty());
	ASSERT_FALSE(aero.airfoils.files.empty());

	for (const auto &section : blade.sections)
	{
		EXPECT_GE(section.polarFileId, 1);
		EXPECT_LE(section.polarFileId, static_cast<int>(aero.airfoils.files.size()));
	}
}

TEST(AeroLIO, AllSimLDemoAirfoilTablesMatchBladePolarIds)
{
	for (const auto &simPath : SimLCases())
	{
		SCOPED_TRACE(simPath.string());
		const auto sim = ReadSimLInput(simPath.string());
		const auto aero = ReadAeroLInput(sim.aeroFile);
		const auto blade = ReadBladeAeroStructInput(aero.bladeAeroStructFile);
		const auto airfoils = ReadAeroLAirfoilFiles(aero);

		ASSERT_EQ(airfoils.size(), aero.airfoils.files.size());
		ASSERT_FALSE(airfoils.empty());
		ASSERT_FALSE(blade.sections.empty());
		for (const auto &file : aero.airfoils.files)
		{
			EXPECT_EQ(std::filesystem::path(file).extension(), ".dat");
			EXPECT_TRUE(std::filesystem::is_regular_file(file));
		}
		for (const auto &section : blade.sections)
		{
			EXPECT_GE(section.polarFileId, 1);
			EXPECT_LE(section.polarFileId, static_cast<int>(aero.airfoils.files.size()));
		}
	}
}

TEST(AeroLIO, ParsesQBladeNativeAirfoilAndPolarFiles)
{
	const std::filesystem::path qbladeRoot =
		"F:/QBladeCE_2.0.9.7/Project/NREL_5MW_OC4_Semisub_v1.3/Aero";
	const auto geometryPath = qbladeRoot / "Airfoils" / "DU21_A17_coords.afl";
	const auto polarPath = qbladeRoot / "Polars" / "DU21_A17_coords_DU21_A17.dat.plr";
	if (!std::filesystem::is_regular_file(geometryPath) || !std::filesystem::is_regular_file(polarPath))
		GTEST_SKIP() << "QBlade reference project is not available on this machine";

	const auto geometry = ReadAirfoilGeometryFile(geometryPath.string());
	EXPECT_EQ(geometry.name, "DU21_A17_coords");
	EXPECT_FALSE(geometry.hasExplicitReference);
	EXPECT_EQ(geometry.declaredCoordinateCount, static_cast<int>(geometry.coordinates.size()));
	ASSERT_GT(geometry.coordinates.size(), 300u);
	EXPECT_NEAR(geometry.reference.x, 0.25, 1.0e-12);
	EXPECT_NEAR(geometry.coordinates.front().x, 1.0, 1.0e-12);

	const auto polar = ReadAirfoilFile(polarPath.string());
	EXPECT_EQ(polar.polarName, "DU21_A17.dat");
	EXPECT_EQ(polar.airfoilName, "../Airfoils/DU21_A17_coords.afl");
	EXPECT_DOUBLE_EQ(polar.thickness, 21.0);
	ASSERT_EQ(polar.reynoldsNumbers.size(), 1u);
	EXPECT_DOUBLE_EQ(polar.reynoldsNumber, 0.75);
	ASSERT_EQ(polar.polarSets.size(), 1u);
	ASSERT_EQ(polar.polar.size(), 142u);
	EXPECT_DOUBLE_EQ(polar.polar.front().alphaDeg, -180.0);
	EXPECT_DOUBLE_EQ(polar.polar.back().alphaDeg, 180.0);
	ASSERT_GT(polar.geometry.coordinates.size(), 300u);
}

TEST(AeroLIO, TextTemplateWriteUpdatesBoundFields)
{
	const auto dir = TestOutputDir() / "AeroL" / "text_template";
	const auto af1 = dir / "af1.dat";
	const auto af2 = dir / "af2.dat";
	const auto blade = dir / "blade.str";
	TouchFile(af1);
	TouchFile(af2);
	TouchFile(blade);

	const auto inputPath = dir / "aero.dat";
	WriteLines(inputPath, {
		"1 ApOfMb",
		"HAWT RotorType",
		"1.5 HubRad",
		"3 BladeNum",
		"3.0 CutInWindSpeed",
		"25.0 CutOutWindSpeed",
		"5000 RatedPower",
		"12.1 RatedRotorSpeed",
		"1.225 AirDens",
		"1.464e-5 KinVisc",
		"335 SpdSound",
		"2 NumAFfiles",
		"1 InterpOrd",
		"AFNames",
		QuotePath(af1),
		QuotePath(af2),
		QuotePath(blade) + " BladeAeroStructFile",
		"false UnsteadyAero",
		"1 DynStallType",
		"1 WakeType",
		"true IfPitch",
		"0 FixedPitch",
		"0 FixedRotationalSpeed",
		"true SumPrint",
		"0 NBlOuts",
		"0 NTwOuts",
		"END",
	});

	auto aero = ReadAeroLInput(inputPath.string());
	aero.ratedPower = 6789.0;
	aero.wakeType = 0;
	const auto outPath = dir / "aero_written.dat";
	WriteAeroLInput(aero, outPath.string(), inputPath.string());

	const auto written = ReadAeroLInput(outPath.string());
	EXPECT_DOUBLE_EQ(written.ratedPower, 6789.0);
	EXPECT_EQ(written.wakeType, 0);
	EXPECT_EQ(written.airfoils.files.size(), 2u);
}

TEST(AeroLIO, ValidationRejectsAirfoilCountMismatch)
{
	const auto dir = TestOutputDir() / "AeroL" / "validation";
	const auto af1 = dir / "af1.dat";
	const auto blade = dir / "blade.str";
	TouchFile(af1);
	TouchFile(blade);

	const auto path = dir / "aero_mismatch.dat";
	WriteLines(path, {
		"2 NumAFfiles",
		"AFNames",
		QuotePath(af1),
		"------",
		QuotePath(blade) + " BladeAeroStructFile",
		"END",
	});
	EXPECT_THROW((void)ReadAeroLInput(path.string()), std::runtime_error);
}
