#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "WindL/IO/WindL_IO_Subs.hpp"
#include "WindL/SimWind.hpp"

namespace
{
bool HasRepoMarker(const std::filesystem::path &root)
{
	std::error_code ec;
	return std::filesystem::is_regular_file(root / "vsstudio" / "UnitTests.vcxproj", ec);
}

std::filesystem::path FindRepoRoot(std::filesystem::path dir)
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

std::filesystem::path RepoRoot()
{
	return FindRepoRoot(std::filesystem::current_path());
}

std::filesystem::path SimWindOutputDir()
{
	auto dir = RepoRoot() / "build" / "test" / "simwind";
	std::filesystem::create_directories(dir);
	return dir;
}

template <typename T>
T ReadScalar(std::ifstream &in)
{
	T value{};
	in.read(reinterpret_cast<char *>(&value), sizeof(T));
	return value;
}

struct BtsHeader
{
	std::int16_t format = 0;
	std::int32_t nz = 0;
	std::int32_t ny = 0;
	std::int32_t towerPoints = 0;
	std::int32_t nSteps = 0;
	float dz = 0.0f;
	float dy = 0.0f;
	float dt = 0.0f;
	float uHub = 0.0f;
	float hubHeight = 0.0f;
	float zBottom = 0.0f;
	float slope[3]{};
	float offset[3]{};
};

BtsHeader ReadBtsHeader(std::ifstream &in)
{
	BtsHeader header;
	header.format = ReadScalar<std::int16_t>(in);
	header.nz = ReadScalar<std::int32_t>(in);
	header.ny = ReadScalar<std::int32_t>(in);
	header.towerPoints = ReadScalar<std::int32_t>(in);
	header.nSteps = ReadScalar<std::int32_t>(in);
	header.dz = ReadScalar<float>(in);
	header.dy = ReadScalar<float>(in);
	header.dt = ReadScalar<float>(in);
	header.uHub = ReadScalar<float>(in);
	header.hubHeight = ReadScalar<float>(in);
	header.zBottom = ReadScalar<float>(in);
	for (int comp = 0; comp < 3; ++comp)
	{
		header.slope[comp] = ReadScalar<float>(in);
		header.offset[comp] = ReadScalar<float>(in);
	}
	const auto descLength = ReadScalar<std::int32_t>(in);
	in.ignore(descLength);
	return header;
}

double DecodeBtsValue(std::int16_t raw, float slope, float offset)
{
	if (std::abs(slope) <= 1.0e-12f)
		return static_cast<double>(raw);
	return (static_cast<double>(raw) - static_cast<double>(offset)) / static_cast<double>(slope);
}

std::vector<std::array<double, 3>> ReadBtsPlane(std::ifstream &in, const BtsHeader &header, int timeIndex)
{
	const std::streamoff planeBytes = static_cast<std::streamoff>(header.nz) * static_cast<std::streamoff>(header.ny) * 3 * static_cast<std::streamoff>(sizeof(std::int16_t));
	in.seekg(planeBytes * timeIndex, std::ios::cur);

	std::vector<std::array<double, 3>> plane(static_cast<std::size_t>(header.nz) * static_cast<std::size_t>(header.ny));
	for (auto &sample : plane)
	{
		for (int comp = 0; comp < 3; ++comp)
		{
			const auto raw = ReadScalar<std::int16_t>(in);
			sample[static_cast<std::size_t>(comp)] = DecodeBtsValue(raw, header.slope[comp], header.offset[comp]);
		}
	}
	return plane;
}

std::vector<std::array<double, 3>> ReadBtsPointSeries(const std::string &path, int pointIndex = -1)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return {};

	const BtsHeader header = ReadBtsHeader(in);
	const int nPoints = header.nz * header.ny;
	if (pointIndex < 0)
		pointIndex = (header.nz / 2) * header.ny + (header.ny / 2);
	if (pointIndex < 0 || pointIndex >= nPoints)
		return {};

	std::vector<std::array<double, 3>> series(static_cast<std::size_t>(header.nSteps));
	for (int t = 0; t < header.nSteps; ++t)
	{
		for (int p = 0; p < nPoints; ++p)
		{
			std::array<double, 3> sample{};
			for (int comp = 0; comp < 3; ++comp)
			{
				const auto raw = ReadScalar<std::int16_t>(in);
				sample[static_cast<std::size_t>(comp)] = DecodeBtsValue(raw, header.slope[comp], header.offset[comp]);
			}
			if (p == pointIndex)
				series[static_cast<std::size_t>(t)] = sample;
		}
	}
	return series;
}

double SampleCovariance(const std::vector<std::array<double, 3>> &series, int a, int b)
{
	if (series.empty())
		return 0.0;

	double meanA = 0.0;
	double meanB = 0.0;
	for (const auto &sample : series)
	{
		meanA += sample[static_cast<std::size_t>(a)];
		meanB += sample[static_cast<std::size_t>(b)];
	}
	meanA /= static_cast<double>(series.size());
	meanB /= static_cast<double>(series.size());

	double sum = 0.0;
	for (const auto &sample : series)
		sum += (sample[static_cast<std::size_t>(a)] - meanA) * (sample[static_cast<std::size_t>(b)] - meanB);
	return sum / static_cast<double>(series.size());
}

bool ContainsWarning(const SimWindResult &result, const std::string &needle)
{
	return std::any_of(result.warnings.begin(), result.warnings.end(), [&](const std::string &warning) {
		return warning.find(needle) != std::string::npos;
	});
}

WindLInput SmallInput(const std::string &name)
{
	WindLInput input;
	input.mode = Mode::GENERATE;
	input.turbModel = TurbModel::IEC_KAIMAL;
	input.windModel = WindModel::NTM;
	input.turbSeed = 12345;
	input.gridPtsY = 3;
	input.gridPtsZ = 3;
	input.fieldDimY = 20.0;
	input.fieldDimZ = 20.0;
	input.simTime = 12.8;
	input.timeStep = 0.2;
	input.hubHeight = 80.0;
	input.meanWindSpeed = 10.0;
	input.savePath = SimWindOutputDir().string();
	input.saveName = name;
	input.wrBlwnd = true;
	input.wrTrbts = true;
	input.wrTrwnd = true;
	input.sumPrint = true;
	return input;
}
} // namespace

TEST(WindL_SimWind, GeneratesReadableBtsWndAndSummary)
{
	const auto result = SimWind::Generate(SmallInput("small_iec"));

	ASSERT_FALSE(result.btsPath.empty());
	ASSERT_FALSE(result.bladedWndPath.empty());
	ASSERT_FALSE(result.turbsimWndPath.empty());
	ASSERT_FALSE(result.wndPath.empty());
	ASSERT_FALSE(result.sumPath.empty());
	EXPECT_TRUE(std::filesystem::is_regular_file(result.btsPath));
	EXPECT_TRUE(std::filesystem::is_regular_file(result.bladedWndPath));
	EXPECT_TRUE(std::filesystem::is_regular_file(result.turbsimWndPath));
	EXPECT_EQ(result.wndPath, result.bladedWndPath);
	EXPECT_TRUE(std::filesystem::is_regular_file(result.sumPath));

	EXPECT_EQ(result.gridPtsY, 3);
	EXPECT_EQ(result.gridPtsZ, 3);
	EXPECT_EQ(result.timeSteps, 64);
	EXPECT_NEAR(result.stats[0].mean, 10.0, 0.5);
	EXPECT_GT(result.stats[0].sigma, 0.1);
	EXPECT_GT(result.stats[1].sigma, 0.05);
	EXPECT_GT(result.stats[2].sigma, 0.02);

	std::ifstream bts(result.btsPath, std::ios::binary);
	ASSERT_TRUE(bts.good());
	const auto btsHeader = ReadBtsHeader(bts);
	EXPECT_EQ(btsHeader.format, 7);
	EXPECT_EQ(btsHeader.nz, 3);
	EXPECT_EQ(btsHeader.ny, 3);
	EXPECT_EQ(btsHeader.towerPoints, 0);
	EXPECT_EQ(btsHeader.nSteps, 64);

	std::ifstream wnd(result.bladedWndPath, std::ios::binary);
	ASSERT_TRUE(wnd.good());
	EXPECT_EQ(ReadScalar<std::int16_t>(wnd), -99);
	EXPECT_EQ(ReadScalar<std::int16_t>(wnd), 5);

	std::ifstream turbsimWnd(result.turbsimWndPath, std::ios::binary);
	ASSERT_TRUE(turbsimWnd.good());
	EXPECT_EQ(ReadScalar<std::int16_t>(turbsimWnd), -99);
	EXPECT_EQ(ReadScalar<std::int16_t>(turbsimWnd), 4);
	EXPECT_EQ(ReadScalar<std::int32_t>(turbsimWnd), 3);
}

TEST(WindL_SimWind, BladedMannHeaderUsesMannModelId)
{
	auto input = SmallInput("small_mann");
	input.turbModel = TurbModel::B_MANN;
	input.wrTrbts = false;
	input.wrTrwnd = false;

	const auto result = SimWind::Generate(input);
	ASSERT_TRUE(std::filesystem::is_regular_file(result.bladedWndPath));

	std::ifstream wnd(result.bladedWndPath, std::ios::binary);
	ASSERT_TRUE(wnd.good());
	EXPECT_EQ(ReadScalar<std::int16_t>(wnd), -99);
	EXPECT_EQ(ReadScalar<std::int16_t>(wnd), 8);
	const auto headerBytes = ReadScalar<std::int32_t>(wnd);
	EXPECT_EQ(headerBytes, 132);
	EXPECT_EQ(ReadScalar<std::int32_t>(wnd), 3);
}

TEST(WindL_SimWind, GenerateFromFileHonorsExistingQwdInputs)
{
	const auto source = RepoRoot() / "Test" / "WindL" / "Qahse_WindL_Main_DEMO.qwd";
	auto input = ReadWindLInput(source.string());
	input.gridPtsY = 2;
	input.gridPtsZ = 2;
	input.fieldDimY = 12.0;
	input.fieldDimZ = 12.0;
	input.simTime = 3.2;
	input.timeStep = 0.2;
	input.savePath = SimWindOutputDir().string();
	input.saveName = "from_qwd_small";
	input.wrBlwnd = false;
	input.wrTrwnd = false;
	input.wrTrbts = true;

	const auto qwd = SimWindOutputDir() / "from_qwd_small.qwd";
	WriteWindLInput(input, qwd.string());

	const auto result = SimWind::GenerateFromFile(qwd.string());
	EXPECT_TRUE(std::filesystem::is_regular_file(result.btsPath));
	EXPECT_TRUE(std::filesystem::is_regular_file(result.sumPath));
	EXPECT_TRUE(result.bladedWndPath.empty());
	EXPECT_TRUE(result.turbsimWndPath.empty());
	EXPECT_TRUE(result.wndPath.empty());
	EXPECT_EQ(result.gridPtsY, 2);
	EXPECT_EQ(result.gridPtsZ, 2);
}

TEST(WindL_SimWind, GeneratesAllExposedIecWindConditions)
{
	struct Case
	{
		WindModel model;
		EWMType ewmType;
		const char *name;
	};

	const std::vector<Case> cases{
	    {WindModel::NTM, EWMType::Turbulent, "iec_ntm"},
	    {WindModel::ETM, EWMType::Turbulent, "iec_etm"},
	    {WindModel::EWM1, EWMType::Turbulent, "iec_ewm1_turb"},
	    {WindModel::EWM50, EWMType::Turbulent, "iec_ewm50_turb"},
	    {WindModel::EWM1, EWMType::Steady, "iec_ewm1_steady"},
	    {WindModel::EWM50, EWMType::Steady, "iec_ewm50_steady"},
	    {WindModel::EOG, EWMType::Turbulent, "iec_eog"},
	    {WindModel::EDC, EWMType::Turbulent, "iec_edc"},
	    {WindModel::ECD, EWMType::Turbulent, "iec_ecd"},
	    {WindModel::EWS, EWMType::Turbulent, "iec_ews"},
	    {WindModel::UNIFORM, EWMType::Turbulent, "iec_uniform"}};

	for (const auto &item : cases)
	{
		auto input = SmallInput(item.name);
		input.windModel = item.model;
		input.ewmType = item.ewmType;
		input.simTime = 6.4;
		input.timeStep = 0.2;
		input.scaleIEC = 1;
		input.wrBlwnd = false;
		input.wrTrwnd = false;

		const auto result = SimWind::Generate(input);
		EXPECT_TRUE(std::filesystem::is_regular_file(result.btsPath)) << item.name;
		EXPECT_TRUE(std::filesystem::is_regular_file(result.sumPath)) << item.name;
		EXPECT_EQ(result.timeSteps, 32) << item.name;
		EXPECT_GT(result.stats[0].mean, 0.0) << item.name;

		if (item.model == WindModel::NTM || item.model == WindModel::ETM || item.ewmType == EWMType::Turbulent)
			EXPECT_GE(result.stats[0].sigma, 0.0) << item.name;
	}
}

TEST(WindL_SimWind, UniformWindProducesConstantField)
{
	auto input = SmallInput("uniform_constant");
	input.windModel = WindModel::UNIFORM;
	input.simTime = 6.4;
	input.timeStep = 0.2;
	input.scaleIEC = 1;
	input.wrBlwnd = false;
	input.wrTrwnd = false;

	const auto result = SimWind::Generate(input);
	ASSERT_TRUE(std::filesystem::is_regular_file(result.btsPath));
	EXPECT_NEAR(result.stats[0].mean, input.meanWindSpeed, 1.0e-9);
	EXPECT_NEAR(result.stats[0].sigma, 0.0, 1.0e-12);
	EXPECT_NEAR(result.stats[1].sigma, 0.0, 1.0e-12);
	EXPECT_NEAR(result.stats[2].sigma, 0.0, 1.0e-12);

	std::ifstream bts(result.btsPath, std::ios::binary);
	ASSERT_TRUE(bts.good());
	const auto header = ReadBtsHeader(bts);
	ASSERT_EQ(header.nSteps, 32);
	const auto plane = ReadBtsPlane(bts, header, 0);
	ASSERT_EQ(plane.size(), 9U);
	for (const auto &sample : plane)
	{
		EXPECT_NEAR(sample[0], input.meanWindSpeed, 1.0e-6);
		EXPECT_NEAR(sample[1], 0.0, 1.0e-6);
		EXPECT_NEAR(sample[2], 0.0, 1.0e-6);
	}
}

TEST(WindL_SimWind, UserShearProfileAffectsUniformMeanField)
{
	UserShearData shear;
	shear.numHeights = 1;
	shear.heights = {79.0};
	shear.windSpeeds = {6.0};
	shear.windDirections = {30.0};
	shear.standardDeviations = {0.4};
	shear.lengthScales = {5.0};

	const auto shearPath = SimWindOutputDir() / "user_shear_uniform.dat";
	WriteUserShear(shear, shearPath.string());

	auto input = SmallInput("user_shear_uniform");
	input.windModel = WindModel::UNIFORM;
	input.shearType = ShearType::USER;
	input.windProfileType = WindProfileType::USER;
	input.userShearFile = shearPath.string();
	input.fieldDimZ = 2.0;
	input.gridPtsZ = 1;
	input.gridPtsY = 1;
	input.fieldDimY = 2.0;
	input.wrBlwnd = false;
	input.wrTrwnd = false;

	const auto result = SimWind::Generate(input);
	ASSERT_TRUE(std::filesystem::is_regular_file(result.btsPath));

	std::ifstream bts(result.btsPath, std::ios::binary);
	ASSERT_TRUE(bts.good());
	const auto header = ReadBtsHeader(bts);
	const auto plane = ReadBtsPlane(bts, header, 0);
	ASSERT_EQ(plane.size(), 1U);
	EXPECT_NEAR(plane[0][0], 6.0 * std::cos(30.0 * 3.14159265358979323846 / 180.0), 1.0e-5);
	EXPECT_NEAR(plane[0][1], 6.0 * std::sin(30.0 * 3.14159265358979323846 / 180.0), 1.0e-5);
}

TEST(WindL_SimWind, UserShearSigmaAndLengthScaleAffectTurbulence)
{
	UserShearData low;
	low.numHeights = 1;
	low.heights = {79.0};
	low.windSpeeds = {10.0};
	low.windDirections = {0.0};
	low.standardDeviations = {0.1};
	low.lengthScales = {2.0};

	UserShearData high = low;
	high.standardDeviations = {1.0};
	high.lengthScales = {20.0};

	const auto lowPath = SimWindOutputDir() / "user_shear_low.dat";
	const auto highPath = SimWindOutputDir() / "user_shear_high.dat";
	WriteUserShear(low, lowPath.string());
	WriteUserShear(high, highPath.string());

	auto inputLow = SmallInput("user_shear_low");
	inputLow.shearType = ShearType::USER;
	inputLow.windProfileType = WindProfileType::USER;
	inputLow.userShearFile = lowPath.string();
	inputLow.scaleIEC = 0;
	inputLow.fieldDimZ = 2.0;
	inputLow.gridPtsZ = 1;
	inputLow.gridPtsY = 3;
	inputLow.fieldDimY = 6.0;
	inputLow.wrBlwnd = false;
	inputLow.wrTrwnd = false;

	auto inputHigh = inputLow;
	inputHigh.saveName = "user_shear_high";
	inputHigh.userShearFile = highPath.string();

	const auto lowResult = SimWind::Generate(inputLow);
	const auto highResult = SimWind::Generate(inputHigh);
	EXPECT_GT(highResult.stats[0].sigma, lowResult.stats[0].sigma * 2.5);
}

TEST(WindL_SimWind, UserWindSpeedInterpolatesInSpaceAndTime)
{
	UserWindSpeedData data;
	data.nComp = 3;
	data.nPoints = 2;
	data.refPtID = 1;
	data.points = {{-1.0, 79.0}, {1.0, 79.0}};
	data.time = {0.0, 1.0};
	data.components = {
	    {{0.0, 2.0}, {0.0, 0.0}, {0.0, 0.0}},
	    {{2.0, 4.0}, {0.0, 0.0}, {0.0, 0.0}}};

	const auto dataPath = SimWindOutputDir() / "user_wind_speed_interp.dat";
	WriteUserWindSpeed(data, dataPath.string());

	auto input = SmallInput("user_wind_speed_interp");
	input.turbModel = TurbModel::USER_WIND_SPEED;
	input.userTurbFile = dataPath.string();
	input.gridPtsY = 3;
	input.gridPtsZ = 1;
	input.fieldDimY = 2.0;
	input.fieldDimZ = 2.0;
	input.simTime = 1.0;
	input.timeStep = 0.5;
	input.wrBlwnd = false;
	input.wrTrwnd = false;

	const auto result = SimWind::Generate(input);
	ASSERT_TRUE(std::filesystem::is_regular_file(result.btsPath));

	std::ifstream bts(result.btsPath, std::ios::binary);
	ASSERT_TRUE(bts.good());
	const auto header = ReadBtsHeader(bts);
	const auto plane = ReadBtsPlane(bts, header, 1);
	ASSERT_EQ(plane.size(), 3U);
	EXPECT_NEAR(plane[1][0], 2.0, 1.0e-5);
}

TEST(WindL_SimWind, ApiCoherenceRejectsNonUComponents)
{
	auto input = SmallInput("api_coherence_invalid");
	input.cohMod1 = CohModel::API;
	input.cohMod2 = CohModel::API;
	input.cohMod3 = CohModel::NONE;
	input.wrBlwnd = false;
	input.wrTrwnd = false;

	try
	{
		(void)SimWind::Generate(input);
		FAIL() << "Expected SimWind to reject API coherence on the v component.";
	}
	catch (const std::runtime_error &error)
	{
		EXPECT_NE(std::string(error.what()).find("API coherence model is valid only for the u component"), std::string::npos);
	}
}

TEST(WindL_SimWind, GeneratesAllBuiltInSpectralModels)
{
	struct Case
	{
		TurbModel model;
		const char *name;
	};

	const std::vector<Case> cases{
	    {TurbModel::IEC_KAIMAL, "spectrum_iec_kaimal"},
	    {TurbModel::IEC_VKAIMAL, "spectrum_iec_vkaimal"},
	    {TurbModel::B_KAL, "spectrum_bladed_kaimal"},
	    {TurbModel::B_VKAL, "spectrum_bladed_von_karman"},
	    {TurbModel::B_IVKAL, "spectrum_bladed_improved_von_karman"},
	    {TurbModel::B_MANN, "spectrum_mann"}};

	for (const auto &item : cases)
	{
		auto input = SmallInput(item.name);
		input.turbModel = item.model;
		input.simTime = 6.4;
		input.timeStep = 0.2;
		input.scaleIEC = 1;
		input.wrBlwnd = false;
		input.wrTrwnd = false;
		if (item.model == TurbModel::B_MANN)
		{
			input.mannNx = 32;
			input.mannNy = 4;
			input.mannNz = 4;
		}

		const auto result = SimWind::Generate(input);
		EXPECT_TRUE(std::filesystem::is_regular_file(result.btsPath)) << item.name;
		EXPECT_TRUE(std::filesystem::is_regular_file(result.sumPath)) << item.name;
		EXPECT_GT(result.stats[0].sigma, 0.01) << item.name;
	}
}

TEST(WindL_SimWind, UserSpectraEndpointHoldKeepsLastPsdValue)
{
	UserSpectraData data;
	data.numFrequencies = 3;
	data.frequencies = {0.1, 0.2, 0.3};
	data.uPsd = {10.0, 10.0, 10.0};
	data.vPsd = {5.0, 5.0, 5.0};
	data.wPsd = {2.0, 2.0, 2.0};

	const auto spectraPath = SimWindOutputDir() / "user_spectra_endpoint_hold.dat";
	WriteUserSpectra(data, spectraPath.string());

	auto input = SmallInput("user_spectra_endpoint_hold");
	input.turbModel = TurbModel::USER_SPECTRA;
	input.userTurbFile = spectraPath.string();
	input.gridPtsY = 1;
	input.gridPtsZ = 1;
	input.fieldDimY = 2.0;
	input.fieldDimZ = 2.0;
	input.simTime = 20.0;
	input.timeStep = 0.1;
	input.scaleIEC = 0;
	input.wrBlwnd = false;
	input.wrTrwnd = false;

	const auto result = SimWind::Generate(input);
	EXPECT_GT(result.stats[0].sigma, 3.0);
	EXPECT_GT(result.stats[1].sigma, 2.0);
	EXPECT_GT(result.stats[2].sigma, 1.0);
}

TEST(WindL_SimWind, UserSpectraSpecScaleChangesSigma)
{
	UserSpectraData base;
	base.numFrequencies = 3;
	base.frequencies = {0.1, 0.2, 0.3};
	base.uPsd = {4.0, 4.0, 4.0};
	base.vPsd = {1.0, 1.0, 1.0};
	base.wPsd = {0.5, 0.5, 0.5};

	UserSpectraData scaled = base;
	scaled.specScale1 = 4.0;

	const auto basePath = SimWindOutputDir() / "user_spectra_scale_base.dat";
	const auto scaledPath = SimWindOutputDir() / "user_spectra_scale_scaled.dat";
	WriteUserSpectra(base, basePath.string());
	WriteUserSpectra(scaled, scaledPath.string());

	auto inputBase = SmallInput("user_spectra_scale_base");
	inputBase.turbModel = TurbModel::USER_SPECTRA;
	inputBase.userTurbFile = basePath.string();
	inputBase.gridPtsY = 1;
	inputBase.gridPtsZ = 1;
	inputBase.fieldDimY = 2.0;
	inputBase.fieldDimZ = 2.0;
	inputBase.simTime = 12.8;
	inputBase.timeStep = 0.1;
	inputBase.scaleIEC = 0;
	inputBase.wrBlwnd = false;
	inputBase.wrTrwnd = false;

	auto inputScaled = inputBase;
	inputScaled.saveName = "user_spectra_scale_scaled";
	inputScaled.userTurbFile = scaledPath.string();

	const auto baseResult = SimWind::Generate(inputBase);
	const auto scaledResult = SimWind::Generate(inputScaled);
	EXPECT_GT(scaledResult.stats[0].sigma, baseResult.stats[0].sigma * 1.7);
}

TEST(WindL_SimWind, UsrVkmUsesProfileSigmaAndLengthScale)
{
	UserShearData low;
	low.numHeights = 2;
	low.heights = {70.0, 90.0};
	low.windSpeeds = {10.0, 10.0};
	low.windDirections = {0.0, 0.0};
	low.standardDeviations = {0.15, 0.15};
	low.lengthScales = {3.0, 3.0};

	UserShearData high = low;
	high.standardDeviations = {0.8, 0.8};
	high.lengthScales = {20.0, 20.0};

	const auto lowPath = SimWindOutputDir() / "usrvkm_low.dat";
	const auto highPath = SimWindOutputDir() / "usrvkm_high.dat";
	WriteUserShear(low, lowPath.string());
	WriteUserShear(high, highPath.string());

	auto inputLow = SmallInput("usrvkm_low");
	inputLow.turbModel = TurbModel::USRVKM;
	inputLow.userShearFile = lowPath.string();
	inputLow.scaleIEC = 0;
	inputLow.gridPtsY = 1;
	inputLow.gridPtsZ = 3;
	inputLow.fieldDimY = 2.0;
	inputLow.fieldDimZ = 20.0;
	inputLow.wrBlwnd = false;
	inputLow.wrTrwnd = false;

	auto inputHigh = inputLow;
	inputHigh.saveName = "usrvkm_high";
	inputHigh.userShearFile = highPath.string();

	const auto lowResult = SimWind::Generate(inputLow);
	const auto highResult = SimWind::Generate(inputHigh);
	EXPECT_GT(highResult.stats[0].sigma, lowResult.stats[0].sigma * 2.5);
}

TEST(WindL_SimWind, UserDirectionWrapInterpolationAvoidsAngleJump)
{
	UserShearData shear;
	shear.numHeights = 2;
	shear.heights = {70.0, 90.0};
	shear.windSpeeds = {8.0, 8.0};
	shear.windDirections = {359.0, 1.0};
	shear.standardDeviations = {0.1, 0.1};
	shear.lengthScales = {5.0, 5.0};

	const auto shearPath = SimWindOutputDir() / "user_shear_wrap.dat";
	WriteUserShear(shear, shearPath.string());

	auto input = SmallInput("user_shear_wrap");
	input.windModel = WindModel::UNIFORM;
	input.shearType = ShearType::USER;
	input.windProfileType = WindProfileType::USER;
	input.userShearFile = shearPath.string();
	input.gridPtsY = 1;
	input.gridPtsZ = 3;
	input.fieldDimY = 2.0;
	input.fieldDimZ = 20.0;
	input.wrBlwnd = false;
	input.wrTrwnd = false;

	const auto result = SimWind::Generate(input);
	const auto plane = ReadBtsPointSeries(result.btsPath);
	ASSERT_FALSE(plane.empty());
	EXPECT_GT(plane[0][0], 7.9);
	EXPECT_NEAR(plane[0][1], 0.0, 0.2);
}

TEST(WindL_SimWind, ExplicitReynoldsStressTargetsAffectHubCovariance)
{
	auto input = SmallInput("reynolds_targets");
	input.gridPtsY = 1;
	input.gridPtsZ = 1;
	input.fieldDimY = 2.0;
	input.fieldDimZ = 2.0;
	input.simTime = 25.6;
	input.timeStep = 0.2;
	input.scaleIEC = 1;
	input.uStar = 0.6;
	input.reynoldsUW = -0.30;
	input.reynoldsUV = 0.12;
	input.reynoldsVW = -0.08;
	input.wrBlwnd = false;
	input.wrTrwnd = false;

	const auto result = SimWind::Generate(input);
	const auto series = ReadBtsPointSeries(result.btsPath, 0);
	ASSERT_FALSE(series.empty());

	EXPECT_NEAR(SampleCovariance(series, 0, 2), input.reynoldsUW, 0.12);
	EXPECT_NEAR(SampleCovariance(series, 0, 1), input.reynoldsUV, 0.12);
	EXPECT_NEAR(SampleCovariance(series, 1, 2), input.reynoldsVW, 0.12);
}

TEST(WindL_SimWind, IecWindProfileUsesDedicatedExponentBranch)
{
	auto input = SmallInput("iec_profile_branch");
	input.windModel = WindModel::EOG;
	input.eventStart = 999.0;
	input.shearType = ShearType::PL;
	input.windProfileType = WindProfileType::IEC;
	input.iecEdition = IecStandard::ED3;
	input.gridPtsY = 1;
	input.gridPtsZ = 3;
	input.fieldDimY = 2.0;
	input.fieldDimZ = 20.0;
	input.wrBlwnd = false;
	input.wrTrwnd = false;

	const auto result = SimWind::Generate(input);
	EXPECT_FALSE(ContainsWarning(result, "WindProfileType=IEC currently falls back"));

	const auto lowerSeries = ReadBtsPointSeries(result.btsPath, 0);
	const auto topSeries = ReadBtsPointSeries(result.btsPath, 2);
	ASSERT_FALSE(lowerSeries.empty());
	ASSERT_FALSE(topSeries.empty());
	const double expectedLow = input.meanWindSpeed * std::pow(70.0 / input.hubHeight, 0.14);
	const double expectedTop = input.meanWindSpeed * std::pow(90.0 / input.hubHeight, 0.14);
	EXPECT_NEAR(lowerSeries[0][0], expectedLow, 1.0e-3);
	EXPECT_NEAR(topSeries[0][0], expectedTop, 1.0e-3);
}

TEST(WindL_SimWind, LogProfileUsesZL)
{
	const auto stablePsi = [](double zOverL)
	{
		if (zOverL >= 0.0)
			return -5.0 * std::min(zOverL, 1.0);
		const double tmp = std::pow(1.0 - 15.0 * zOverL, 0.25);
		double psi = -std::log(0.125 * std::pow(1.0 + tmp, 2.0) * (1.0 + tmp * tmp)) +
		             2.0 * std::atan(tmp) - 0.5 * 3.14159265358979323846;
		return -psi;
	};
	const auto expectedLogSpeed = [&](double z, double zRef, double uRef, double z0, double hubHeight, double zL)
	{
		const double moninLength = std::abs(zL) <= 1.0e-12 ? std::numeric_limits<double>::infinity() : hubHeight / zL;
		const double zOverLHeight = std::isfinite(moninLength) ? z / moninLength : 0.0;
		const double zOverLRef = std::isfinite(moninLength) ? zRef / moninLength : 0.0;
		return uRef * (std::log(z / z0) - stablePsi(zOverLHeight)) /
		       (std::log(zRef / z0) - stablePsi(zOverLRef));
	};

	auto stable = SmallInput("log_profile_stable");
	stable.windModel = WindModel::EOG;
	stable.eventStart = 999.0;
	stable.shearType = ShearType::LOG;
	stable.windProfileType = WindProfileType::LOG;
	stable.roughness = 0.03;
	stable.zOverL = 0.2;
	stable.gridPtsY = 1;
	stable.gridPtsZ = 3;
	stable.fieldDimY = 2.0;
	stable.fieldDimZ = 20.0;
	stable.wrBlwnd = false;
	stable.wrTrwnd = false;

	auto unstable = stable;
	unstable.saveName = "log_profile_unstable";
	unstable.zOverL = -0.2;

	const auto stableResult = SimWind::Generate(stable);
	const auto unstableResult = SimWind::Generate(unstable);
	const auto stableLower = ReadBtsPointSeries(stableResult.btsPath, 0);
	const auto unstableLower = ReadBtsPointSeries(unstableResult.btsPath, 0);
	ASSERT_FALSE(stableLower.empty());
	ASSERT_FALSE(unstableLower.empty());

	const double expectedStable = expectedLogSpeed(70.0, stable.hubHeight, stable.meanWindSpeed, stable.roughness, stable.hubHeight, stable.zOverL);
	const double expectedUnstable = expectedLogSpeed(70.0, unstable.hubHeight, unstable.meanWindSpeed, unstable.roughness, unstable.hubHeight, unstable.zOverL);

	EXPECT_NEAR(stableLower[0][0], expectedStable, 1.0e-3);
	EXPECT_NEAR(unstableLower[0][0], expectedUnstable, 1.0e-3);
	EXPECT_GT(std::abs(stableLower[0][0] - unstableLower[0][0]), 1.0e-3);
}
