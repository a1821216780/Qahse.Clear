#include <filesystem>

#include <gtest/gtest.h>

#include "WindL/IO/WindL_InputPar.hpp"

namespace
{

std::filesystem::path TestOutputDir()
{
    auto dir = std::filesystem::path("build") / "test" / "windl_io";
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace

TEST(WindLIO, ReadsTextKeywordAndLineNumber)
{
    WindL_InputPar input;
    input.LoadText("demo/WindL/Qahse_WindL_Main_DEMO");

    EXPECT_TRUE(input.WrTrwnd);
    EXPECT_EQ(input.WrTrwndLine(), 14);
    EXPECT_EQ(input.Mode, 0);
    EXPECT_EQ(input.NumPointY, 36);
    EXPECT_EQ(input.WrWndName, "Test_Demo_wind");
}

TEST(WindLIO, ConvertsTextYamlAndBinary)
{
    const auto dir = TestOutputDir();
    const auto yamlPath = dir / "Qahse_WindL_Main_DEMO.yml";
    const auto binaryPath = dir / "Qahse_WindL_Main_DEMO.bin";
    const auto roundTripPath = dir / "Qahse_WindL_Main_DEMO_roundtrip.qwd";

    WindL_InputPar input;
    input.LoadText("demo/WindL/Qahse_WindL_Main_DEMO.qwd");
    input.SaveYaml(yamlPath.string());
    input.SaveBinary(binaryPath.string());

    WindL_InputPar fromYaml;
    fromYaml.LoadYaml(yamlPath.string());
    EXPECT_TRUE(fromYaml.WrTrwnd);
    EXPECT_EQ(fromYaml.WrWndName, "Test_Demo_wind");
    EXPECT_DOUBLE_EQ(fromYaml.TimeStep, 0.05);

    WindL_InputPar fromBinary;
    fromBinary.LoadBinary(binaryPath.string());
    EXPECT_TRUE(fromBinary.WrTrwnd);
    EXPECT_EQ(fromBinary.WrWndName, "Test_Demo_wind");
    EXPECT_DOUBLE_EQ(fromBinary.TimeStep, 0.05);

    fromYaml.SaveText(roundTripPath.string(), "demo/WindL/Qahse_WindL_Main_DEMO.qwd");

    WindL_InputPar roundTrip;
    roundTrip.LoadText(roundTripPath.string());
    EXPECT_TRUE(roundTrip.WrTrwnd);
    EXPECT_EQ(roundTrip.WrWndName, "Test_Demo_wind");
}
