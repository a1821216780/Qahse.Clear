#include <gtest/gtest.h>

#include <Eigen/Dense>

#include <cmath>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "../../src/IO/ZString.hpp"

namespace
{
enum class DeviceMode
{
    Auto,
    Manual,
    Safe
};

class ScopedTempRoot
{
public:
    ScopedTempRoot()
        : root_(std::filesystem::temp_directory_path() / "Qahse_Clear_ZStringComprehensiveTests")
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    ~ScopedTempRoot()
    {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    std::filesystem::path path(const std::string &relative) const
    {
        return root_ / relative;
    }

private:
    std::filesystem::path root_;
};
}

TEST(ZStringComprehensiveTest, CaseConversionAndPalindrome)
{
    EXPECT_EQ(ZString::ToUpper(std::string("Abc123")), "ABC123");
    EXPECT_EQ(ZString::ToLower(std::string("AbC123")), "abc123");

    std::string upperInPlace = "hElLo";
    static_cast<void (*)(std::string &)>(&ZString::ToUpper)(upperInPlace);
    EXPECT_EQ(upperInPlace, "HELLO");

    std::string lowerInPlace = "WoRLD";
    static_cast<void (*)(std::string &)>(&ZString::ToLower)(lowerInPlace);
    EXPECT_EQ(lowerInPlace, "world");

    EXPECT_TRUE(ZString::IsPalindrome(""));
    EXPECT_TRUE(ZString::IsPalindrome("abba"));
    EXPECT_FALSE(ZString::IsPalindrome("abca"));
}

TEST(ZStringComprehensiveTest, TrimWhitespaceAndTextPredicates)
{
    const std::string raw = " \t  hello world  \r\n";
    EXPECT_EQ(ZString::Trim(raw), "hello world");
    EXPECT_EQ(ZString::TrimStart(raw), "hello world  \r\n");
    EXPECT_EQ(ZString::TrimEnd(raw), " \t  hello world");

    EXPECT_TRUE(ZString::IsNullOrWhiteSpace(""));
    EXPECT_TRUE(ZString::IsNullOrWhiteSpace(" \t\r\n"));
    EXPECT_FALSE(ZString::IsNullOrWhiteSpace(" a "));

    EXPECT_TRUE(ZString::StartsWith("AlphaBeta", "Alpha"));
    EXPECT_FALSE(ZString::StartsWith("AlphaBeta", "alpha"));
    EXPECT_TRUE(ZString::StartsWith("AlphaBeta", "alpha", true));
    EXPECT_FALSE(ZString::StartsWith("abc", "abcd"));

    EXPECT_TRUE(ZString::EndsWith("AlphaBeta", "Beta"));
    EXPECT_FALSE(ZString::EndsWith("AlphaBeta", "beta"));
    EXPECT_TRUE(ZString::EndsWith("AlphaBeta", "beta", true));
    EXPECT_FALSE(ZString::EndsWith("abc", "zabc"));

    EXPECT_TRUE(ZString::Contains("AlphaBeta", "haB"));
    EXPECT_FALSE(ZString::Contains("AlphaBeta", "hab"));
    EXPECT_TRUE(ZString::Contains("AlphaBeta", "hab", true));
    EXPECT_TRUE(ZString::Contains("AlphaBeta", ""));
}

TEST(ZStringComprehensiveTest, ReplaceOverloadsAndRegressionCases)
{
    EXPECT_EQ(ZString::Replace("a-b-c", "-", ":"), "a:b:c");
    EXPECT_EQ(ZString::Replace("aaaa", "aa", "a"), "aa");
    EXPECT_EQ(ZString::Replace("abc", "", "x"), "abc");

    EXPECT_EQ(ZString::Replace("abca", 'a', 'x'), "xbcx");
    EXPECT_EQ(ZString::Replace("aba", 'a', "xy"), "xybxy");
    EXPECT_EQ(ZString::Replace("abcabc", "abc", 'x'), "xx");
    EXPECT_EQ(ZString::Replace("abc", "", 'x'), "abc");
}

TEST(ZStringComprehensiveTest, SplitJoinConcatAndRepeat)
{
    const auto splitTextKeepEmpty = ZString::Split("a||b|", "|", false);
    ASSERT_EQ(splitTextKeepEmpty.size(), 4u);
    EXPECT_EQ(splitTextKeepEmpty[0], "a");
    EXPECT_EQ(splitTextKeepEmpty[1], "");
    EXPECT_EQ(splitTextKeepEmpty[2], "b");
    EXPECT_EQ(splitTextKeepEmpty[3], "");

    const auto splitTextDropEmpty = ZString::Split("a||b|", "|", true);
    ASSERT_EQ(splitTextDropEmpty.size(), 2u);
    EXPECT_EQ(splitTextDropEmpty[0], "a");
    EXPECT_EQ(splitTextDropEmpty[1], "b");

    const auto splitByChar = static_cast<std::vector<std::string> (*)(const std::string &, char)>(&ZString::Split)("x,,y,,", ',');
    ASSERT_EQ(splitByChar.size(), 2u);
    EXPECT_EQ(splitByChar[0], "x");
    EXPECT_EQ(splitByChar[1], "y");

    const auto splitByCharRemoveEmpty = ZString::Split("x,,y,,", ',', true);
    ASSERT_EQ(splitByCharRemoveEmpty.size(), 2u);
    EXPECT_EQ(splitByCharRemoveEmpty[0], "x");
    EXPECT_EQ(splitByCharRemoveEmpty[1], "y");

    const auto splitBySet = ZString::Split("a,b;c", ",;");
    ASSERT_EQ(splitBySet.size(), 3u);
    EXPECT_EQ(splitBySet[0], "a");
    EXPECT_EQ(splitBySet[1], "b");
    EXPECT_EQ(splitBySet[2], "c");

    const auto splitByNull = ZString::Split("abc", static_cast<const char *>(nullptr));
    ASSERT_EQ(splitByNull.size(), 1u);
    EXPECT_EQ(splitByNull[0], "abc");

    EXPECT_EQ(ZString::Split("abc", "", false).size(), 1u);
    EXPECT_TRUE(ZString::Split("", "", true).empty());

    EXPECT_EQ(ZString::Join({"a", "b", "c"}, ","), "a,b,c");
    EXPECT_EQ(ZString::Join({}, ","), "");

    EXPECT_EQ(ZString::Concat(std::vector<std::string>{"ab", "cd", "ef"}), "abcdef");
    EXPECT_EQ(ZString::Concat({"1", "2", "3"}), "123");

    EXPECT_EQ(ZString::Repeat("xy", 3), "xyxyxy");
    EXPECT_EQ(ZString::Repeat("xy", 0), "");
}

TEST(ZStringComprehensiveTest, StringToScalarConversionsAndWrappers)
{
    EXPECT_EQ(ZString::StringTo<std::string>("  hello "), "hello");
    EXPECT_EQ(ZString::StringTo<int>("42"), 42);
    EXPECT_EQ(ZString::StringTo<long long>("-9000000000"), -9000000000LL);
    EXPECT_EQ(ZString::StringTo<unsigned int>("0x10"), 16u);
    EXPECT_FLOAT_EQ(ZString::StringTo<float>("3.5"), 3.5f);
    EXPECT_DOUBLE_EQ(ZString::StringTo<double>("-2.25"), -2.25);
    EXPECT_NEAR(static_cast<double>(ZString::StringTo<long double>("1.125")), 1.125, 1e-12);

    EXPECT_TRUE(ZString::StringTo<bool>("2"));
    EXPECT_FALSE(ZString::StringTo<bool>("0"));

    EXPECT_EQ(ZString::StringToInt("7"), 7);
    EXPECT_EQ(ZString::StringToLongLong("8"), 8LL);
    EXPECT_FLOAT_EQ(ZString::StringToFloat("9.5"), 9.5f);
    EXPECT_DOUBLE_EQ(ZString::StringToDouble("10.5"), 10.5);
    EXPECT_NEAR(static_cast<double>(ZString::StringToLongDouble("11.5")), 11.5, 1e-12);

    EXPECT_THROW((void)ZString::StringTo<int>("12abc"), std::invalid_argument);
    EXPECT_THROW((void)ZString::StringTo<double>("1.2.3"), std::invalid_argument);
}

TEST(ZStringComprehensiveTest, StringToBoolRegressionCases)
{
    EXPECT_TRUE(ZString::StringToBool("1"));
    EXPECT_FALSE(ZString::StringToBool("0"));
    EXPECT_TRUE(ZString::StringToBool("-5"));
    EXPECT_THROW((void)ZString::StringToBool("true"), std::invalid_argument);
}

TEST(ZStringComprehensiveTest, EnumConversionAndCaseBehavior)
{
    EXPECT_EQ(ZString::StringToEnum<DeviceMode>("manual"), DeviceMode::Manual);
    EXPECT_EQ(ZString::StringToEnum<DeviceMode>("Safe"), DeviceMode::Safe);

    EXPECT_EQ(ZString::StringToEnum<DeviceMode>("Auto", false), DeviceMode::Auto);
    EXPECT_THROW((void)ZString::StringToEnum<DeviceMode>("auto", false), std::invalid_argument);
    EXPECT_THROW((void)ZString::StringToEnum<DeviceMode>("Unknown"), std::invalid_argument);
}

TEST(ZStringComprehensiveTest, EigenVectorAndMatrixParsing)
{
    const auto vec = ZString::StringToEigenVector<int>("1,2;3");
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec(0), 1);
    EXPECT_EQ(vec(1), 2);
    EXPECT_EQ(vec(2), 3);

    const auto rowVec = ZString::StringToEigenRowVector<double>("1 2 3");
    ASSERT_EQ(rowVec.size(), 3);
    EXPECT_DOUBLE_EQ(rowVec(0), 1.0);
    EXPECT_DOUBLE_EQ(rowVec(1), 2.0);
    EXPECT_DOUBLE_EQ(rowVec(2), 3.0);

    const auto fixed = ZString::StringToEigen<Eigen::Vector3i>("4,5,6");
    EXPECT_EQ(fixed(0), 4);
    EXPECT_EQ(fixed(1), 5);
    EXPECT_EQ(fixed(2), 6);

    EXPECT_THROW((void)ZString::StringToEigen<Eigen::Vector3i>("1,2"), std::invalid_argument);
    EXPECT_THROW((void)ZString::StringToEigen<Eigen::Matrix2d>("1 2;3 4"), std::invalid_argument);

    const auto matrix2x2 = ZString::StringToEigenMatrix<Eigen::Matrix2d>("1,2;3,4", 2);
    EXPECT_DOUBLE_EQ(matrix2x2(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(matrix2x2(0, 1), 2.0);
    EXPECT_DOUBLE_EQ(matrix2x2(1, 0), 3.0);
    EXPECT_DOUBLE_EQ(matrix2x2(1, 1), 4.0);

    const auto matrix2x3 = ZString::StringToEigenMatrix<Eigen::MatrixXd>("1,2,3,4,5,6", 2);
    ASSERT_EQ(matrix2x3.rows(), 2);
    ASSERT_EQ(matrix2x3.cols(), 3);
    EXPECT_DOUBLE_EQ(matrix2x3(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(matrix2x3(1, 2), 6.0);

    EXPECT_THROW((void)ZString::StringToEigenMatrix<Eigen::MatrixXd>("", 2), std::invalid_argument);
    EXPECT_THROW((void)ZString::StringToEigenMatrix<Eigen::MatrixXd>("1,2,3", 0), std::invalid_argument);
    EXPECT_THROW((void)ZString::StringToEigenMatrix<Eigen::MatrixXd>("1,2;3", 2), std::invalid_argument);
    EXPECT_THROW((void)ZString::StringToEigenMatrix<Eigen::Matrix2d>("1,2,3,4", 3), std::invalid_argument);
}

TEST(ZStringComprehensiveTest, FileReadWriteAllLinesAndRegression)
{
    const ScopedTempRoot root;
    const auto file = root.path("text-lines.txt").string();

    ZString::WriteAllLines(file, {"A", "B", "C"});
    const auto lines = ZString::ReadAllLines(file);
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "A");
    EXPECT_EQ(lines[1], "B");
    EXPECT_EQ(lines[2], "C");

    ZString::WriteAllLines(file, {});
    const auto emptyLines = ZString::ReadAllLines(file);
    EXPECT_TRUE(emptyLines.empty());

    const auto missingRead = root.path("missing.txt").string();
    EXPECT_THROW((void)ZString::ReadAllLines(missingRead), std::runtime_error);

    const auto missingWrite = root.path("missing/dir/file.txt").string();
    EXPECT_NO_THROW(ZString::WriteAllLines(missingWrite, {"x"}));
    EXPECT_FALSE(std::filesystem::exists(missingWrite));
}
