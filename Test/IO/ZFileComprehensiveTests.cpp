#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "../../src/IO/ZFile.hpp"

namespace
{
class ScopedTempRoot
{
public:
    ScopedTempRoot()
        : root_(std::filesystem::temp_directory_path() / "Qahse_Clear_ZFileComprehensiveTests")
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

TEST(ZFileComprehensiveTest, ExistsLengthAndReadFunctions)
{
    const ScopedTempRoot root;
    const auto file = root.path("readable.txt");
    const std::string content = "line1\nline2";
    {
        std::ofstream out(file.string(), std::ios::binary);
        out << content;
    }

    EXPECT_TRUE(ZFile::Exists(file.string()));
    EXPECT_EQ(ZFile::GetLength(file.string()), static_cast<std::uintmax_t>(content.size()));
    EXPECT_EQ(ZFile::ReadAllText(file.string()), content);

    const auto lines = ZFile::ReadAllLines(file.string());
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "line1");
    EXPECT_EQ(lines[1], "line2");

    const auto bytes = ZFile::ReadAllBytes(file.string());
    ASSERT_EQ(bytes.size(), content.size());
    EXPECT_EQ(bytes[0], static_cast<std::uint8_t>('l'));
    EXPECT_EQ(bytes[5], static_cast<std::uint8_t>('\n'));
}

TEST(ZFileComprehensiveTest, MissingFileThrowsOnReadAndLength)
{
    const ScopedTempRoot root;
    const auto missing = root.path("not-exist.txt").string();

    EXPECT_FALSE(ZFile::Exists(missing));
    EXPECT_THROW((void)ZFile::GetLength(missing), std::runtime_error);
    EXPECT_THROW((void)ZFile::ReadAllText(missing), std::runtime_error);
    EXPECT_THROW((void)ZFile::ReadAllLines(missing), std::runtime_error);
    EXPECT_THROW((void)ZFile::ReadAllBytes(missing), std::runtime_error);
}

TEST(ZFileComprehensiveTest, WriteAndAppendTextLinesAndBytes)
{
    const ScopedTempRoot root;

    const auto textFile = root.path("text.txt").string();
    ZFile::WriteAllText(textFile, "alpha");
    ZFile::AppendAllText(textFile, "beta");
    EXPECT_EQ(ZFile::ReadAllText(textFile), "alphabeta");

    ZFile::WriteAllText(textFile, "x");
    EXPECT_EQ(ZFile::ReadAllText(textFile), "x");

    const auto linesFile = root.path("lines.txt").string();
    ZFile::WriteAllLines(linesFile, {"a", "b"});
    ZFile::AppendAllLines(linesFile, {"c"});
    EXPECT_EQ(ZFile::ReadAllText(linesFile), "a\nb\nc\n");

    const auto readLines = ZFile::ReadAllLines(linesFile);
    ASSERT_EQ(readLines.size(), 3u);
    EXPECT_EQ(readLines[0], "a");
    EXPECT_EQ(readLines[1], "b");
    EXPECT_EQ(readLines[2], "c");

    const auto bytesFile = root.path("bytes.bin").string();
    const std::vector<std::uint8_t> bytes = {0x00, 0x01, 0x7F, 0xFF};
    ZFile::WriteAllBytes(bytesFile, bytes);
    EXPECT_EQ(ZFile::ReadAllBytes(bytesFile), bytes);

    ZFile::WriteAllBytes(bytesFile, {});
    EXPECT_EQ(ZFile::GetLength(bytesFile), 0u);

    const auto appendCreateFile = root.path("append-created.txt").string();
    EXPECT_FALSE(ZFile::Exists(appendCreateFile));
    ZFile::AppendAllText(appendCreateFile, "seed");
    EXPECT_TRUE(ZFile::Exists(appendCreateFile));
    EXPECT_EQ(ZFile::ReadAllText(appendCreateFile), "seed");
}

TEST(ZFileComprehensiveTest, WriteAndAppendThrowWhenParentDirectoryMissing)
{
    const ScopedTempRoot root;
    const auto missingDirFile = root.path("missing/child/file.txt").string();

    EXPECT_THROW(ZFile::WriteAllText(missingDirFile, "x"), std::runtime_error);
    EXPECT_THROW(ZFile::AppendAllText(missingDirFile, "x"), std::runtime_error);
    EXPECT_THROW(ZFile::WriteAllLines(missingDirFile, {"x"}), std::runtime_error);
    EXPECT_THROW(ZFile::AppendAllLines(missingDirFile, {"x"}), std::runtime_error);
    EXPECT_THROW(ZFile::WriteAllBytes(missingDirFile, {0x01}), std::runtime_error);
}

TEST(ZFileComprehensiveTest, DeleteHandlesMissingAndExistingFile)
{
    const ScopedTempRoot root;
    const auto file = root.path("delete.txt").string();

    EXPECT_NO_THROW(ZFile::Delete(file));

    ZFile::WriteAllText(file, "to-delete");
    EXPECT_TRUE(ZFile::Exists(file));

    ZFile::Delete(file);
    EXPECT_FALSE(ZFile::Exists(file));
}

TEST(ZFileComprehensiveTest, CopyValidatesSourceAndOverwriteBehavior)
{
    const ScopedTempRoot root;
    const auto source = root.path("source.txt").string();
    const auto destination = root.path("destination.txt").string();
    const auto missingSource = root.path("missing.txt").string();

    EXPECT_THROW(ZFile::Copy(missingSource, destination), std::runtime_error);

    ZFile::WriteAllText(source, "v1");
    ZFile::Copy(source, destination);
    EXPECT_EQ(ZFile::ReadAllText(destination), "v1");

    ZFile::WriteAllText(source, "v2");
    EXPECT_THROW(ZFile::Copy(source, destination, false), std::runtime_error);
    EXPECT_EQ(ZFile::ReadAllText(destination), "v1");

    ZFile::Copy(source, destination, true);
    EXPECT_EQ(ZFile::ReadAllText(destination), "v2");
}

TEST(ZFileComprehensiveTest, MoveValidatesSourceAndOverwriteBehavior)
{
    const ScopedTempRoot root;
    const auto source = root.path("move-source.txt").string();
    const auto destination = root.path("move-destination.txt").string();
    const auto movedAgain = root.path("move-final.txt").string();
    const auto missingSource = root.path("missing-source.txt").string();

    EXPECT_THROW(ZFile::Move(missingSource, destination), std::runtime_error);

    ZFile::WriteAllText(source, "payload");
    ZFile::WriteAllText(destination, "existing");
    EXPECT_THROW(ZFile::Move(source, destination, false), std::runtime_error);
    EXPECT_TRUE(ZFile::Exists(source));
    EXPECT_EQ(ZFile::ReadAllText(destination), "existing");

    ZFile::Move(source, destination, true);
    EXPECT_FALSE(ZFile::Exists(source));
    EXPECT_EQ(ZFile::ReadAllText(destination), "payload");

    ZFile::Move(destination, movedAgain);
    EXPECT_FALSE(ZFile::Exists(destination));
    EXPECT_TRUE(ZFile::Exists(movedAgain));
    EXPECT_EQ(ZFile::ReadAllText(movedAgain), "payload");
}
