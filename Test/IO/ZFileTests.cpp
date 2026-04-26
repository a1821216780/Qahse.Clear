#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "../../src/IO/ZFile.hpp"

namespace
{
	std::filesystem::path makeTestRoot()
	{
		std::filesystem::path root = std::filesystem::temp_directory_path() / "Qahse_Clear_ZFileTests";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root);
		return root;
	}
}

TEST(ZFileTest, ReadWriteAppendAndDeleteText)
{
	const auto root = makeTestRoot();
	const auto filePath = (root / "sample.txt").string();

	ZFile::WriteAllText(filePath, "alpha");
	EXPECT_TRUE(ZFile::Exists(filePath));
	EXPECT_EQ(ZFile::ReadAllText(filePath), "alpha");

	ZFile::AppendAllText(filePath, "\nbeta");
	EXPECT_EQ(ZFile::ReadAllText(filePath), "alpha\nbeta");
	EXPECT_EQ(ZFile::GetLength(filePath), 10u);

	ZFile::Delete(filePath);
	EXPECT_FALSE(ZFile::Exists(filePath));

	std::filesystem::remove_all(root);
}

TEST(ZFileTest, ReadWriteCopyMoveAndBytes)
{
	const auto root = makeTestRoot();
	const auto sourcePath = (root / "source.bin").string();
	const auto copiedPath = (root / "copied.bin").string();
	const auto movedPath = (root / "moved.bin").string();

	const std::vector<std::uint8_t> bytes = {0x01, 0x02, 0x03, 0xFF};
	ZFile::WriteAllBytes(sourcePath, bytes);
	EXPECT_EQ(ZFile::ReadAllBytes(sourcePath), bytes);

	ZFile::Copy(sourcePath, copiedPath);
	EXPECT_EQ(ZFile::ReadAllBytes(copiedPath), bytes);

	ZFile::Move(copiedPath, movedPath, true);
	EXPECT_FALSE(ZFile::Exists(copiedPath));
	EXPECT_EQ(ZFile::ReadAllBytes(movedPath), bytes);

	std::filesystem::remove_all(root);
}
