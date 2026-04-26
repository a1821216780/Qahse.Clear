#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "../../src/IO/ZPath.hpp"

namespace
{
class ScopedTempRoot
{
public:
    ScopedTempRoot()
        : root_(std::filesystem::temp_directory_path() / "Qahse_Clear_ZPathTests")
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    ~ScopedTempRoot()
    {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    const std::filesystem::path &path() const
    {
        return root_;
    }

private:
    std::filesystem::path root_;
};
}

TEST(ZPathTest, NormalizesSeparatorsAndDetectsEmptyString)
{
    const std::string mixed = "folder/sub\\leaf.txt";
    const std::string expected = std::filesystem::path(mixed).make_preferred().string();

    EXPECT_EQ(ZPath::NormalizeSeparators(mixed), expected);
    EXPECT_TRUE(ZPath::IsEmpty(""));
    EXPECT_FALSE(ZPath::IsEmpty(" "));
}

TEST(ZPathTest, HandlesAbsoluteAndFullPathQueries)
{
    const std::string relativeRaw = (std::filesystem::path("alpha") / "." / "beta" / ".." / "gamma.txt").string();
    const std::string expectedFull = std::filesystem::path(relativeRaw).lexically_normal().string();
    const std::string expectedAbs = std::filesystem::absolute(std::filesystem::path(relativeRaw)).lexically_normal().string();

    EXPECT_FALSE(ZPath::IsAbsolute(relativeRaw));
    EXPECT_EQ(ZPath::GetFullPath(relativeRaw), expectedFull);
    EXPECT_EQ(ZPath::GetABSPath(relativeRaw), expectedAbs);
    EXPECT_TRUE(ZPath::IsAbsolute(expectedAbs));
    EXPECT_TRUE(ZPath::GetABSPath("").empty());
}

TEST(ZPathTest, ExtractsFilePartsAndExtensions)
{
    const std::string path = (std::filesystem::path("data") / "archive.tar.gz").string();

    EXPECT_EQ(ZPath::GetDirectoryName(path), std::filesystem::path(path).parent_path().string());
    EXPECT_EQ(ZPath::GetFileName(path), std::filesystem::path(path).filename().string());
    EXPECT_EQ(ZPath::GetFileNameWithoutExtension(path), std::filesystem::path(path).stem().string());
    EXPECT_EQ(ZPath::GetExtension(path), std::filesystem::path(path).extension().string());
    EXPECT_TRUE(ZPath::HasExtension(path));

    auto removed = std::filesystem::path(path);
    EXPECT_EQ(ZPath::RemoveExtension(path), removed.replace_extension().string());

    const std::string noExt = (std::filesystem::path("data") / "readme").string();
    EXPECT_FALSE(ZPath::HasExtension(noExt));
}

TEST(ZPathTest, CombinesPathSegments)
{
    const std::string left = (std::filesystem::path("root") / "branch").string();
    const std::string right = "leaf.txt";
    const std::string expectedPair = (std::filesystem::path(left) / std::filesystem::path(right)).string();
    const std::string expectedMany = (std::filesystem::path("root") / "branch" / "leaf.txt").string();

    EXPECT_EQ(ZPath::Combine(left, right), expectedPair);
    EXPECT_EQ(ZPath::Combine({"root", "branch", "leaf.txt"}), expectedMany);
    EXPECT_EQ(ZPath::Combine(std::initializer_list<std::string>{}), std::filesystem::path().string());
}

TEST(ZPathTest, AddsAndRemovesTrailingSeparator)
{
    const std::string sample = (std::filesystem::path("root") / "child").string();
    const char sep = std::filesystem::path::preferred_separator;

    const std::string ensured = ZPath::EnsureTrailingSeparator(sample);
    ASSERT_FALSE(ensured.empty());
    EXPECT_EQ(ensured.back(), sep);
    EXPECT_EQ(ZPath::EnsureTrailingSeparator(ensured), ensured);
    EXPECT_TRUE(ZPath::EnsureTrailingSeparator("").empty());

    const std::string repeated = sample + std::string(2, sep);
    EXPECT_EQ(ZPath::RemoveTrailingSeparator(repeated), sample);
    EXPECT_EQ(ZPath::RemoveTrailingSeparator(ensured), sample);

    const std::string rootOnly(1, sep);
    EXPECT_EQ(ZPath::RemoveTrailingSeparator(rootOnly), rootOnly);
    EXPECT_TRUE(ZPath::RemoveTrailingSeparator("").empty());
}

TEST(ZPathTest, ReportsPathSeparator)
{
    EXPECT_EQ(ZPath::GetPathSeparator(), std::string(1, std::filesystem::path::preferred_separator));
}

TEST(ZPathTest, ChecksExistsDirectoryAndFileStates)
{
    const ScopedTempRoot root;
    const std::filesystem::path dir = root.path() / "nested";
    const std::filesystem::path file = dir / "data.txt";
    const std::filesystem::path missing = root.path() / "missing.txt";

    std::filesystem::create_directories(dir);
    {
        std::ofstream output(file.string(), std::ios::binary);
        output << "abc";
    }

    EXPECT_TRUE(ZPath::Exists(dir.string()));
    EXPECT_TRUE(ZPath::Exists(file.string()));
    EXPECT_FALSE(ZPath::Exists(missing.string()));

    EXPECT_TRUE(ZPath::IsDirectory(dir.string()));
    EXPECT_FALSE(ZPath::IsDirectory(file.string()));
    EXPECT_FALSE(ZPath::IsDirectory(missing.string()));

    EXPECT_TRUE(ZPath::IsFile(file.string()));
    EXPECT_FALSE(ZPath::IsFile(dir.string()));
    EXPECT_FALSE(ZPath::IsFile(missing.string()));
}
