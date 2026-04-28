#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
