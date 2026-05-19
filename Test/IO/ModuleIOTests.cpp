#include "../ModuleIOTestHelpers.hpp"

#include <gtest/gtest.h>

#include "IO/ModuleIO.hpp"

using namespace module_io_test;

TEST(ModuleIO, YamlRowParserReadsTwoDimensionalStringTables)
{
	const auto path = TestOutputDir() / "IO" / "rows.yaml";
	WriteLines(path, {
		"Qahse:",
		"  ModuleIO:",
		"    Rows:",
		"      - [ 1, 2.5, JointA ]",
		"      - [ 2, 3.5, JointB ]",
	});

	const auto rows = module_io::ReadYamlRows(path.string(), "Qahse.ModuleIO", "Rows");
	ASSERT_EQ(rows.size(), 2u);
	ASSERT_EQ(rows.front().size(), 3u);
	EXPECT_EQ(rows[0][0], "1");
	EXPECT_EQ(rows[0][2], "JointA");
	EXPECT_EQ(rows[1][1], "3.5");
	EXPECT_EQ(rows[1][2], "JointB");
}
