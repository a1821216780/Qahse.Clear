#include <gtest/gtest.h>

#include <Eigen/Dense>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "../../src/IO/Yaml.hpp"

namespace
{
	std::filesystem::path makeYamlTestRoot()
	{
		std::filesystem::path root = std::filesystem::temp_directory_path() / "Qahse_Clear_YamlTests";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root);
		return root;
	}

	bool hasChildNamed(const std::vector<YML::Node *> &children, const std::string &name)
	{
		return std::any_of(children.begin(), children.end(), [&](const YML::Node *node)
						   { return node != nullptr && node->name == name; });
	}

	enum class SampleMode
	{
		Alpha,
		Beta
	};
}

TEST(YamlTest, AddsReadsModifiesAndDeletesNestedNodes)
{
	YML yaml;
	yaml.AddNode("database.host", "localhost");
	yaml.AddNode("database.port", "3306");
	yaml.AddNode("database.options.enabled", "true");

	EXPECT_EQ(yaml.read("database.host"), "localhost");
	EXPECT_EQ(YML::YmlToInt(yaml.read("database.port")), 3306);
	EXPECT_TRUE(YML::YmlToBool(yaml.read("database.options.enabled")));
	EXPECT_TRUE(yaml.ChickfindNodeByKey("database.options.enabled"));
	EXPECT_EQ(yaml.GetNodeKey(yaml.findNodeByKey("database.options.enabled")), "database.options.enabled");

	const auto children = yaml.FindChildren("database");
	EXPECT_EQ(children.size(), 3u);
	EXPECT_TRUE(hasChildNamed(children, "host"));
	EXPECT_TRUE(hasChildNamed(children, "port"));
	EXPECT_TRUE(hasChildNamed(children, "options"));

	yaml.modify("database.host", "127.0.0.1");
	EXPECT_EQ(yaml.read("database.host"), "127.0.0.1");

	yaml.DeleteNode("database.options");
	EXPECT_FALSE(yaml.ChickfindNodeByKey("database.options"));
	EXPECT_FALSE(yaml.ChickfindNodeByKey("database.options.enabled"));
}

TEST(YamlTest, ClonesAndMergesYamlTrees)
{
	YML original;
	original.AddNode("solver.maxIter", "100");
	original.AddNode("solver.tolerance", "0.001");

	YML cloned = original.Clone();
	cloned.modify("solver.maxIter", "200");

	EXPECT_EQ(original.read("solver.maxIter"), "100");
	EXPECT_EQ(cloned.read("solver.maxIter"), "200");

	YML patch;
	patch.AddNode("solver.tolerance", "0.0001");
	patch.AddNode("solver.method", "bicgstab");
	original.AddYAML(patch);

	EXPECT_EQ(original.read("solver.tolerance"), "0.0001");
	EXPECT_EQ(original.read("solver.method"), "bicgstab");
}

TEST(YamlTest, ParsesIndentedYamlAndMatrixBlocks)
{
	const std::string text =
		"# ignored\n"
		"root:\n"
		"  title: demo\n"
		"  count: 3\n"
		"matrix:\n"
		"  -  [ 1.0 , 2.0 ]\n"
		"  -  [ 3.0 , 4.0 ]\n";

	YML yaml = YML::Parse(text);
	EXPECT_EQ(yaml.read("root.title"), "demo");
	EXPECT_EQ(YML::YmlToInt(yaml.read("root.count")), 3);

	const auto matrix = YML::YmlToMatrix(yaml.read("matrix"));
	ASSERT_EQ(matrix.RowCount, 2);
	ASSERT_EQ(matrix.ColumnCount, 2);
	EXPECT_DOUBLE_EQ(matrix.data(0, 0), 1.0);
	EXPECT_DOUBLE_EQ(matrix.data(0, 1), 2.0);
	EXPECT_DOUBLE_EQ(matrix.data(1, 0), 3.0);
	EXPECT_DOUBLE_EQ(matrix.data(1, 1), 4.0);
}

TEST(YamlTest, ConvertsYamlValuesToCppAndEigenTypes)
{
	EXPECT_TRUE(YML::YmlToBool("True"));
	EXPECT_EQ(YML::YmlToInt("-42"), -42);
	EXPECT_DOUBLE_EQ(YML::YmlToDouble("3.25"), 3.25);
	EXPECT_EQ(YML::YmlToEnum<SampleMode>("beta"), SampleMode::Beta);

	const auto ints = YML::YmlToIntArray("[ 1 , 2 , 3 ]");
	ASSERT_EQ(ints.size(), 3u);
	EXPECT_EQ(ints[0], 1);
	EXPECT_EQ(ints[1], 2);
	EXPECT_EQ(ints[2], 3);

	const Eigen::VectorXd vector = YML::YmlToVector("[ 4.5 , 5.5 , 6.5 ]");
	ASSERT_EQ(vector.size(), 3);
	EXPECT_DOUBLE_EQ(vector(0), 4.5);
	EXPECT_DOUBLE_EQ(vector(1), 5.5);
	EXPECT_DOUBLE_EQ(vector(2), 6.5);
}

TEST(YamlTest, FormatsCppAndEigenValuesAsYamlStrings)
{
	EXPECT_EQ(YML::ToYmlValueString("  hello  "), "hello");
	EXPECT_EQ(YML::ToYmlValueString(std::vector<int>{1, 2, 3}), "[ 1 , 2 , 3 ]");
	EXPECT_EQ(YML::ToYmlValueString(std::vector<std::string>{"a", "b"}), "[ a , b ]");
	EXPECT_EQ(YML::ToYmlValueString(true), "True");
	EXPECT_EQ(YML::ToYmlValueString(SampleMode::Alpha), "Alpha");

	Eigen::Vector3d vector;
	vector << 1.0, 2.0, 3.0;
	EXPECT_EQ(YML::ToYmlValueString(vector), "[ 1 , 2 , 3 ]");

	Eigen::Matrix2d matrix;
	matrix << 1.0, 2.0,
		3.0, 4.0;
	const std::string matrixText = YML::ToYmlValueString(matrix, 0);
	const auto parsed = YML::YmlToMatrix(matrixText);
	ASSERT_EQ(parsed.RowCount, 2);
	ASSERT_EQ(parsed.ColumnCount, 2);
	EXPECT_DOUBLE_EQ(parsed.data(0, 0), 1.0);
	EXPECT_DOUBLE_EQ(parsed.data(0, 1), 2.0);
	EXPECT_DOUBLE_EQ(parsed.data(1, 0), 3.0);
	EXPECT_DOUBLE_EQ(parsed.data(1, 1), 4.0);
}

TEST(YamlTest, SavesAndLoadsYamlFiles)
{
	const auto root = makeYamlTestRoot();
	const auto filePath = (root / "config.yml").string();

	YML yaml;
	yaml.AddNode("app.name", "Qahse");
	yaml.AddNode("app.version", "1");
	yaml.save(filePath);

	YML loaded(filePath, false);
	EXPECT_EQ(loaded.read("app.name"), "Qahse");
	EXPECT_EQ(YML::YmlToInt(loaded.read("app.version")), 1);

	std::filesystem::remove_all(root);
}
