#include <gtest/gtest.h>

#include <Eigen/Dense>

#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "../../src/IO/OtherHelper.hpp"

namespace
{
class ScopedTempRoot
{
public:
    ScopedTempRoot()
        : root_(std::filesystem::temp_directory_path() / "Qahse_Clear_OtherHelperTests")
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

TEST(OtherHelperTest, FormortPathAndFileDirectoryExistence)
{
    const ScopedTempRoot root;
    const auto dir = root.path("subdir");
    std::filesystem::create_directories(dir);

    EXPECT_TRUE(OtherHelper::DirectoryExists(dir.string()));
    EXPECT_FALSE(OtherHelper::DirectoryExists(root.path("nonexistent").string()));

    const std::string absPath = OtherHelper::FormortPath(dir.string());
    EXPECT_FALSE(absPath.empty());
    EXPECT_TRUE(OtherHelper::FileExists(absPath) == false);
    EXPECT_TRUE(OtherHelper::DirectoryExists(absPath));

    const std::string nonAbs = OtherHelper::FormortPath(".");
    EXPECT_FALSE(nonAbs.empty());
}

TEST(OtherHelperTest, FindBestMatchAndLevenshtein)
{
    auto [idx, val] = OtherHelper::FindBestMatch({"alpha", "beta", "gamma"}, "alpa");
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(val, "alpha");

    auto [idx2, val2] = OtherHelper::FindBestMatch({"cat", "dog", "bird"}, "dot");
    EXPECT_EQ(idx2, 1);

    auto [idx3, val3] = OtherHelper::FindBestMatch({}, "anything");
    EXPECT_EQ(idx3, -1);
    EXPECT_EQ(val3, "");

    EXPECT_EQ(OtherHelper::LevenshteinDistance("", ""), 0);
    EXPECT_EQ(OtherHelper::LevenshteinDistance("abc", "abc"), 0);
    EXPECT_EQ(OtherHelper::LevenshteinDistance("", "abc"), 3);
    EXPECT_EQ(OtherHelper::LevenshteinDistance("kitten", "sitting"), 3);
}

TEST(OtherHelperTest, StringManipulationHelpers)
{
    EXPECT_EQ(OtherHelper::ToLowerString("HeLLo"), "hello");
    EXPECT_EQ(OtherHelper::ToLowerString(""), "");

    EXPECT_EQ(OtherHelper::TrimString("  hello  "), "hello");
    EXPECT_EQ(OtherHelper::TrimString("\t\n\rhello\n\t"), "hello");
    EXPECT_EQ(OtherHelper::TrimString(""), "");
    EXPECT_EQ(OtherHelper::TrimString("   "), "");

    const auto tokens = OtherHelper::SplitString("a,b,c", ",");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "a");
    EXPECT_EQ(tokens[1], "b");
    EXPECT_EQ(tokens[2], "c");

    const auto tokensEmpty = OtherHelper::SplitString("abc", ",");
    ASSERT_EQ(tokensEmpty.size(), 1u);
    EXPECT_EQ(tokensEmpty[0], "abc");

    const auto tokensTrailing = OtherHelper::SplitString("a,b,", ",");
    ASSERT_EQ(tokensTrailing.size(), 3u);
    EXPECT_EQ(tokensTrailing[2], "");

    const std::string filled = OtherHelper::FillString("abc", ".", 2);
    EXPECT_EQ(filled, "a..b..c");

    EXPECT_EQ(OtherHelper::FillString("", ".", 1), "");
    EXPECT_EQ(OtherHelper::FillString("a", ".", 1), "a");
    EXPECT_EQ(OtherHelper::FillString("ab", "", 1), "ab");
    EXPECT_EQ(OtherHelper::FillString("ab", ".", 0), "ab");

    EXPECT_EQ(OtherHelper::CenterText("hello", 11, '-'), "---hello---");
    EXPECT_EQ(OtherHelper::CenterText("hello", 10, '-'), "--hello---");
    EXPECT_EQ(OtherHelper::CenterText("toolong", 3), "toolong");
    EXPECT_EQ(OtherHelper::CenterText("hi", 4, ' '), " hi ");

    const std::string rng = OtherHelper::RandomString(16);
    EXPECT_EQ(rng.size(), 16u);
    for (char c : rng)
    {
        EXPECT_TRUE((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
    }
}

TEST(OtherHelperTest, GetFileExtensionAndSetFileExtension)
{
    const ScopedTempRoot root;
    EXPECT_EQ(OtherHelper::GetFileExtension("data.YAML"), ".yaml");
    EXPECT_EQ(OtherHelper::GetFileExtension("archive.tar.gz"), ".gz");
    EXPECT_EQ(OtherHelper::GetFileExtension("README"), "");
    EXPECT_EQ(OtherHelper::GetFileExtension("path/to/file.TXT"), ".txt");

    const auto file = root.path("test.txt");
    {
        std::ofstream out(file.string(), std::ios::binary);
        out << "content";
    }
    EXPECT_TRUE(OtherHelper::FileExists(file.string()));

    std::string filePath = file.string();
    OtherHelper::SetFileExtension(filePath, ".csv");
    EXPECT_TRUE(OtherHelper::FileExists(filePath));
    EXPECT_EQ(OtherHelper::GetFileExtension(filePath), ".csv");
    EXPECT_FALSE(OtherHelper::FileExists(file.string()));

    const std::string missing = root.path("missing.txt").string();
    std::string missingPath = missing;
    OtherHelper::SetFileExtension(missingPath, ".csv");
}

TEST(OtherHelperTest, ReadOutputWordAndMatchingLineIndexes)
{
    const std::vector<std::string> data = {
        "Alpha", "", "Beta", "END", "Gamma"
    };

    const auto words = OtherHelper::ReadOutputWord(data, 0, false);
    ASSERT_EQ(words.size(), 2u);
    EXPECT_EQ(words[0], "Alpha");
    EXPECT_EQ(words[1], "Beta");

    const auto deduped = OtherHelper::ReadOutputWord(data, 0, true);
    EXPECT_EQ(deduped.size(), 2u);

    const std::vector<std::string> lines = {
        "WindSpeed=10", "Temperature=25", "WindDir=270", "WindSpeed=15"
    };

    auto indexes = OtherHelper::GetMatchingLineIndexes(lines, "WindSpeed", "test.yaml", false, false);
    ASSERT_EQ(indexes.size(), 2u);
    EXPECT_EQ(indexes[0], 0);
    EXPECT_EQ(indexes[1], 3);

    EXPECT_TRUE(OtherHelper::GetMatchingLineIndexes("WindSpeed=10", "WindSpeed"));
    EXPECT_FALSE(OtherHelper::GetMatchingLineIndexes("WindSpeed=10", "Pressure"));
}

TEST(OtherHelperTest, ProjectNameAndBuildMode)
{
    EXPECT_EQ(OtherHelper::GetCurrentProjectName(), "Qahse");
    EXPECT_EQ(OtherHelper::GetCurrentExeName(), "Qahse.exe");
    EXPECT_EQ(OtherHelper::GetCurrentVersion(""), "3.0.0");
    EXPECT_EQ(OtherHelper::GetCurrentAssemblyVersion(), "3.0.0");

    const std::string buildMode = OtherHelper::GetCurrentBuildMode();
    EXPECT_TRUE(buildMode == "Debug" || buildMode == "Release");

    const std::string arch = OtherHelper::GetBuildMode();
    EXPECT_TRUE(arch == "_x64" || arch == "_x32");

    EXPECT_GT(OtherHelper::GetThreadCount(), 0);
}

TEST(OtherHelperTest, FindFilesWithExtension)
{
    const ScopedTempRoot root;
    const auto dir = root.path("search");
    std::filesystem::create_directories(dir);
    {
        std::ofstream(dir / "a.txt") << "";
        std::ofstream(dir / "b.TXT") << "";
        std::ofstream(dir / "c.dat") << "";
        std::filesystem::create_directories(dir / "nested");
        std::ofstream(dir / "nested" / "d.txt") << "";
    }

    const auto txtFiles = OtherHelper::FindFilesWithExtension(dir.string(), ".txt");
    EXPECT_EQ(txtFiles.size(), 3u);

    const auto datFiles = OtherHelper::FindFilesWithExtension(dir.string(), ".dat");
    EXPECT_EQ(datFiles.size(), 1u);

    const auto missing = OtherHelper::FindFilesWithExtension(root.path("no_dir").string(), ".txt");
    EXPECT_TRUE(missing.empty());
}

TEST(OtherHelperTest, CreateDirectoriesAndSetCurrentDirectory)
{
    const ScopedTempRoot root;
    const auto newDir = root.path("new/nested/dir");
    EXPECT_FALSE(OtherHelper::DirectoryExists(newDir.string()));

    OtherHelper::CreateDirectories(newDir.string());
    EXPECT_TRUE(OtherHelper::DirectoryExists(newDir.string()));

    const auto markerFile = root.path("marker.txt");
    {
        std::ofstream out(markerFile.string());
        out << "test";
    }

    std::filesystem::path originalDir = std::filesystem::current_path();
    OtherHelper::SetCurrentDirectoryW(markerFile.string());
    std::filesystem::current_path(originalDir);
}

TEST(OtherHelperTest, TimeFunctionsReturnValidRanges)
{
    const std::string year = OtherHelper::GetCurrentYear();
    const int yearInt = OtherHelper::GetCurrentYear(true);
    EXPECT_EQ(year, std::to_string(yearInt));
    EXPECT_GE(yearInt, 2020);
    EXPECT_LE(yearInt, 2100);

    const int month = OtherHelper::GetCurrentMonth(true);
    EXPECT_GE(month, 1);
    EXPECT_LE(month, 12);

    const int day = OtherHelper::GetCurrentDay(true);
    EXPECT_GE(day, 1);
    EXPECT_LE(day, 31);

    const int hour = OtherHelper::GetCurrentHour(true);
    EXPECT_GE(hour, 0);
    EXPECT_LE(hour, 23);

    const int minute = OtherHelper::GetCurrentMinute(true);
    EXPECT_GE(minute, 0);
    EXPECT_LE(minute, 59);

    const int second = OtherHelper::GetCurrentSecond(true);
    EXPECT_GE(second, 0);
    EXPECT_LE(second, 60);

    EXPECT_EQ(OtherHelper::GetCurrentMonth(), std::to_string(month));
    EXPECT_EQ(OtherHelper::GetCurrentDay(), std::to_string(day));
    EXPECT_EQ(OtherHelper::GetCurrentHour(), std::to_string(hour));
    EXPECT_EQ(OtherHelper::GetCurrentMinute(), std::to_string(minute));
    EXPECT_EQ(OtherHelper::GetCurrentSecond(), std::to_string(second));

    const std::string timeW = OtherHelper::GetCurrentTimeW();
    EXPECT_FALSE(timeW.empty());
    EXPECT_NE(timeW.find('-'), std::string::npos);

    const std::string buildTime = OtherHelper::GetBuildTime();
    EXPECT_FALSE(buildTime.empty());
}

TEST(OtherHelperTest, GetSafeLocalTimeValueVersion)
{
    const std::time_t now = std::time(nullptr);
    std::tm result = OtherHelper::GetSafeLocalTime(now);
    EXPECT_GE(result.tm_year + 1900, 2020);
}

TEST(OtherHelperTest, AreEqualAndDistinctAndDuplicates)
{
    EXPECT_TRUE(OtherHelper::AreEqual(1, 1));
    EXPECT_FALSE(OtherHelper::AreEqual(1, 2));
    EXPECT_TRUE(OtherHelper::AreEqual(1.0, 1.0));
    EXPECT_TRUE(OtherHelper::AreEqual(std::string("abc"), std::string("abc")));

    const std::vector<int> withDups = {3, 1, 2, 1, 3};
    const auto distinct = OtherHelper::Distinct(withDups);
    ASSERT_EQ(distinct.size(), 3u);
    EXPECT_EQ(distinct[0], 1);
    EXPECT_EQ(distinct[1], 2);
    EXPECT_EQ(distinct[2], 3);

    const auto duplicates = OtherHelper::Duplicates(withDups);
    EXPECT_EQ(duplicates.size(), 2u);

    const std::array<int, 5> arrWithDups = {5, 3, 3, 5, 1};
    const auto arrDistinct = OtherHelper::Distinct(arrWithDups);
    EXPECT_EQ(arrDistinct[0], 1);

    const auto arrDups = OtherHelper::Duplicates(arrWithDups);
    EXPECT_EQ(arrDups.size(), 2u);
}

TEST(OtherHelperTest, TostringSpecializations)
{
    EXPECT_EQ(OtherHelper::Tostring<int>(42), "42");
    EXPECT_EQ(OtherHelper::Tostring<bool>(true), "1");
    EXPECT_EQ(OtherHelper::Tostring<bool>(false), "0");

    const double pi = 3.141592653589793;
    const std::string piStr = OtherHelper::Tostring<double>(pi);
    EXPECT_FALSE(piStr.empty());
    EXPECT_NE(piStr.find("3"), std::string::npos);

    const std::string floatStr = OtherHelper::Tostring<float>(2.5f);
    EXPECT_FALSE(floatStr.empty());

    EXPECT_EQ(OtherHelper::Tostring<std::string>("hello"), "hello");

    Eigen::VectorXd vec(3);
    vec << 1.0, 2.0, 3.0;
    const std::string vecStr = OtherHelper::Tostring<Eigen::VectorXd>(vec, ',', true);
    EXPECT_NE(vecStr.find("1"), std::string::npos);

    Eigen::VectorXf vecF(3);
    vecF << 1.0f, 2.0f, 3.0f;
    const std::string vecFStr = OtherHelper::Tostring<Eigen::VectorXf>(vecF, ',', true);
    EXPECT_NE(vecFStr.find("1"), std::string::npos);

    Eigen::MatrixXf matF(2, 2);
    matF << 1.0f, 2.0f, 3.0f, 4.0f;
    const std::string matFStr = OtherHelper::Tostring<Eigen::MatrixXf>(matF, '\t', true);
    EXPECT_FALSE(matFStr.empty());

    Eigen::MatrixXd matD(2, 2);
    matD << 1.0, 2.0, 3.0, 4.0;
    const std::string matDStr = OtherHelper::Tostring<Eigen::MatrixXd>(matD, '\t', true);
    EXPECT_FALSE(matDStr.empty());

    const std::vector<double> dVec = {1.5, 2.5};
    const std::string dVecStr = OtherHelper::Tostring<std::vector<double>>(dVec, ',', false);
    EXPECT_NE(dVecStr.find("1.5"), std::string::npos);

    const std::vector<float> fVec = {1.5f, 2.5f};
    const std::string fVecStr = OtherHelper::Tostring<std::vector<float>>(fVec, ',', false);
    EXPECT_FALSE(fVecStr.empty());

    const std::vector<int> iVec = {10, 20};
    const std::string iVecStr = OtherHelper::Tostring<std::vector<int>>(iVec, ',', false);
    EXPECT_NE(iVecStr.find("10"), std::string::npos);

    const std::vector<bool> bVec = {true, false};
    const std::string bVecStr = OtherHelper::Tostring<std::vector<bool>>(bVec, ',', false);
    EXPECT_NE(bVecStr.find("1"), std::string::npos);
}

TEST(OtherHelperTest, ParseLineBasicParsing)
{
    const std::vector<std::string> linesInt = {"key1 42", "key2 99"};
    OtherHelper::ParseLineArgs<int> argsInt{
        linesInt, "test.txt", {0, "key1"}, std::nullopt, 0, ' ', '\t', 0, nullptr, false, nullptr, false, ""};
    EXPECT_EQ(OtherHelper::ParseLine(argsInt), 42);

    const std::vector<std::string> linesDouble = {"value 3.14", "other 2.71"};
    OtherHelper::ParseLineArgs<double> argsDouble{
        linesDouble, "test.txt", {0, "value"}, std::nullopt, 0, ' ', '\t', 0, nullptr, false, nullptr, false, ""};
    EXPECT_DOUBLE_EQ(OtherHelper::ParseLine(argsDouble), 3.14);

    const std::vector<std::string> linesMulti = {"data 10 20 30"};
    OtherHelper::ParseLineArgs<int> argsMulti{
        linesMulti, "test.txt", {0, "data"}, std::nullopt, 0, ' ', '\t', 2, nullptr, false, nullptr, false, ""};
    EXPECT_EQ(OtherHelper::ParseLine(argsMulti), 30);

    const std::vector<std::string> linesDefault = {"no match here"};
    OtherHelper::ParseLineArgs<int> argsDefault{
        linesDefault, "test.txt", {0, "missing"}, 77, 0, ' ', '\t', 0, nullptr, false, nullptr, false, ""};
    EXPECT_EQ(OtherHelper::ParseLine(argsDefault), 77);

    const std::vector<std::string> linesOOR = {"key 42"};
    OtherHelper::ParseLineArgs<int> argsOutOfRange{
        linesOOR, "test.txt", {5, "key"}, 100, 0, ' ', '\t', 0, nullptr, false, nullptr, false, ""};
    EXPECT_EQ(OtherHelper::ParseLine(argsOutOfRange), 100);
}

TEST(OtherHelperTest, IsStructAndStructName)
{
    struct MyStruct
    {
        int x;
        double y;
    };

    EXPECT_TRUE(OtherHelper::IsStruct<MyStruct>());
    EXPECT_FALSE(OtherHelper::IsStruct<int>());
    EXPECT_FALSE(OtherHelper::IsStruct<double>());

    EXPECT_TRUE(OtherHelper::IsStructOrStructArray<MyStruct>());
    EXPECT_FALSE(OtherHelper::IsStructOrStructArray<int>());

    MyStruct s{};
    const std::string name = OtherHelper::GetStructName(s);
    EXPECT_FALSE(name.empty());
}

TEST(OtherHelperTest, EnumConversion)
{
    enum class Color
    {
        Red,
        Green,
        Blue
    };

    Color result = Color::Red;
    EXPECT_FALSE(OtherHelper::TryConvertToEnum<Color>("Red", result));

    EXPECT_FALSE(OtherHelper::TryConvertToEnum<Color>("UnknownColor", result));

    EXPECT_THROW(OtherHelper::ConvertToEnum<Color>("Red"), std::invalid_argument);
}

TEST(OtherHelperTest, ConvertMatrixTitleToOutfile)
{
    Eigen::MatrixXd mat(2, 3);
    mat << 1, 2, 3, 4, 5, 6;

    const auto lines = OtherHelper::ConvertMatrixTitleToOutfile<Eigen::MatrixXd>(
        "Title", {"col1", "col2", "col3"}, {"rowA", "rowB"}, mat);

    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "Title\tcol1\tcol2\tcol3");
}

TEST(OtherHelperTest, IsListReturnsFalseForBasicTypes)
{
    int x = 42;
    EXPECT_FALSE(OtherHelper::IsList(x));

    std::string s = "hello";
    EXPECT_FALSE(OtherHelper::IsList(s));
}

TEST(OtherHelperTest, MathAccReturnsNonNull)
{
    const std::string acc = OtherHelper::GetMathAcc();
    EXPECT_TRUE(acc == "MKL" || acc == "Blas" || acc == "CUDA" || acc == "pps");
}

TEST(OtherHelperTest, GetStructFieldsReturnsEmptyForSimpleTypes)
{
    const auto fields = OtherHelper::GetStructFields(42);
    EXPECT_TRUE(fields.empty());

    const auto nameType = OtherHelper::GetStructNameAndType(42);
    EXPECT_TRUE(nameType.empty());
}

TEST(OtherHelperTest, CopyDirectoryFiles)
{
#ifdef _WIN32
    const ScopedTempRoot root;
    const auto srcDir = root.path("copy_src");
    const auto dstDir = root.path("copy_dst");

    std::filesystem::create_directories(srcDir);
    {
        std::ofstream(srcDir / "a.txt") << "hello";
        std::ofstream(srcDir / "b.dat") << "data";
    }

    OtherHelper::CopyFileW(srcDir.string(), dstDir.string(), "*.txt", true);

    EXPECT_TRUE(OtherHelper::FileExists((dstDir / "a.txt").string()));
    EXPECT_FALSE(OtherHelper::FileExists((dstDir / "b.dat").string()));

    OtherHelper::CopyFileW(srcDir.string(), dstDir.string(), "*", true);
    EXPECT_TRUE(OtherHelper::FileExists((dstDir / "b.dat").string()));
#endif
}