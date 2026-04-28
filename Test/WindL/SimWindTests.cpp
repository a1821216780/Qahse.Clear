#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

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
	EXPECT_EQ(ReadScalar<std::int16_t>(bts), 7);
	EXPECT_EQ(ReadScalar<std::int32_t>(bts), 3);
	EXPECT_EQ(ReadScalar<std::int32_t>(bts), 3);
	EXPECT_EQ(ReadScalar<std::int32_t>(bts), 0);
	EXPECT_EQ(ReadScalar<std::int32_t>(bts), 64);

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
	EXPECT_EQ(headerBytes, 148);
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
