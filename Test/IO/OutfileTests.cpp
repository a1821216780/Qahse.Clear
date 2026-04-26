#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "../../src/IO/Outfile.h"

namespace
{
class ScopedTempRoot
{
public:
    ScopedTempRoot()
        : root_(std::filesystem::temp_directory_path() / "Qahse_Clear_OutFileTests")
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

std::string readWholeFile(const std::string &file)
{
    std::ifstream in(file, std::ios::binary);
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}
}

TEST(OutFileTest, AddsDefaultExtensionAndWritesFormattedNumber)
{
    const ScopedTempRoot root;
    const auto basePath = root.path("report").string();

    std::size_t beforeSize = LogData::OutFilelist.size();

    std::string generatedPath;
    {
        OutFile out(basePath, 2, -1);
        generatedPath = out.GetFilename();
        EXPECT_TRUE(generatedPath.size() >= 6);
        EXPECT_EQ(generatedPath.substr(generatedPath.size() - 6), ".3zout");

        out.WriteLine(3.14159);
        out.Outfinish();
    }

    EXPECT_EQ(LogData::OutFilelist.size(), beforeSize);
    EXPECT_TRUE(std::filesystem::exists(generatedPath));

    const std::string content = readWholeFile(generatedPath);
    std::string normalized = content;
    normalized.erase(std::remove(normalized.begin(), normalized.end(), '\r'), normalized.end());
    EXPECT_EQ(normalized, "3.14\n");
}

TEST(OutFileTest, ScientificFormatContainsExponent)
{
    const ScopedTempRoot root;
    const auto path = root.path("sci.txt").string();

    {
        OutFile out(path, -1, 3);
        out.WriteLine(123.456);
        out.Outfinish();
    }

    const std::string content = readWholeFile(path);
    EXPECT_NE(content.find('E'), std::string::npos);
}
