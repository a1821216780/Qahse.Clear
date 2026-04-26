#include <gtest/gtest.h>

#include <Eigen/Dense>
#include <string>
#include <vector>

#include "../../src/IO/ZString.hpp"

namespace
{
    struct ConvertCase
    {
        std::string kind;
        std::string type;
        std::string value;
        std::string expected;
    };

    std::vector<ConvertCase> loadCases(const std::string &path)
    {
        std::vector<ConvertCase> rows;
        for (const auto &line : ZString::ReadAllLines(path))
        {
            const std::string trimmed = ZString::Trim(line);
            if (trimmed.empty() || ZString::StartsWith(trimmed, "#"))
                continue;
            if (trimmed.find('|') == std::string::npos)
                continue;

            const auto parts = ZString::Split(trimmed, '|', false);
            if (parts.size() != 4)
                throw std::runtime_error("测试数据格式错误: " + trimmed);

            rows.push_back(ConvertCase{parts[0], parts[1], parts[2], parts[3]});
        }
        return rows;
    }

    std::vector<std::string> loadRawBlock(const std::string &path)
    {
        std::vector<std::string> rows;
        bool inRawBlock = false;

        for (const auto &line : ZString::ReadAllLines(path))
        {
            const std::string trimmed = ZString::Trim(line);
            if (trimmed.empty())
            {
                continue;
            }

            if (ZString::StartsWith(trimmed, "#") || trimmed.find('|') != std::string::npos)
                continue;

            inRawBlock = true;
            rows.push_back(trimmed);
        }

        return rows;
    }

    std::string decodeEscapes(std::string value)
    {
        value = ZString::Replace(value, "\\r", "\r");
        value = ZString::Replace(value, "\\n", "\n");
        value = ZString::Replace(value, "\\t", "\t");
        return value;
    }

    std::string resolveDataPath(const std::string &filename)
    {
        const std::vector<std::string> candidates = {
            "Test/IO/data/" + filename,
            "../Test/IO/data/" + filename,
            "../../Test/IO/data/" + filename,
        };

        for (const auto &candidate : candidates)
        {
            try
            {
                (void)ZString::ReadAllLines(candidate);
                return candidate;
            }
            catch (...)
            {
            }
        }

        throw std::runtime_error("无法找到测试数据文件: " + filename);
    }
}

TEST(ZStringConvertTest, ConvertsIntBoolAndEigenFromFile)
{
    const auto cases = loadCases(resolveDataPath("ZStringConvertCases.txt"));
    ASSERT_FALSE(cases.empty());

    for (const auto &item : cases)
    {
        const std::string value = decodeEscapes(item.value);

        if (item.kind == "int")
        {
            EXPECT_EQ(ZString::StringToInt(value), std::stoi(item.expected));
        }
        else if (item.kind == "bool")
        {
            EXPECT_EQ(ZString::StringToBool(value), item.expected == "true");
        }
        else if (item.kind == "vector" && item.type == "int")
        {
            const auto vec = ZString::StringToEigenVector<int>(value);
            if (item.expected == "3")
            {
                ASSERT_EQ(vec.size(), 3);
                EXPECT_EQ(vec.coeff(0), 1);
                EXPECT_EQ(vec.coeff(1), 2);
                EXPECT_EQ(vec.coeff(2), 3);
            }
            else if (item.expected == "5")
            {
                ASSERT_EQ(vec.size(), 5);
                EXPECT_EQ(vec.coeff(0), 1);
                EXPECT_EQ(vec.coeff(1), 2);
                EXPECT_EQ(vec.coeff(2), 3);
                EXPECT_EQ(vec.coeff(3), 4);
                EXPECT_EQ(vec.coeff(4), 5);
            }
            else
            {
                ASSERT_EQ(vec.size(), 3);
                EXPECT_EQ(vec.coeff(0), 7);
                EXPECT_EQ(vec.coeff(1), 8);
                EXPECT_EQ(vec.coeff(2), 9);
            }
        }
        else if (item.kind == "vector" && item.type == "double")
        {
            const auto vec = ZString::StringToEigenVector<double>(value);
            ASSERT_EQ(vec.size(), 3);
            if (item.expected == "3")
            {
                EXPECT_DOUBLE_EQ(vec.coeff(0), 1.25);
                EXPECT_DOUBLE_EQ(vec.coeff(1), 2.5);
                EXPECT_DOUBLE_EQ(vec.coeff(2), 3.75);
            }
            else
            {
                EXPECT_DOUBLE_EQ(vec.coeff(0), 10.5);
                EXPECT_DOUBLE_EQ(vec.coeff(1), 11.5);
                EXPECT_DOUBLE_EQ(vec.coeff(2), 12.5);
            }
        }
        else if (item.kind == "rowvector" && item.type == "float")
        {
            const auto vec = ZString::StringToEigenRowVector<float>(value);
            ASSERT_EQ(vec.size(), 3);
            if (item.expected == "3")
            {
                EXPECT_FLOAT_EQ(vec.coeff(0), 4.0f);
                EXPECT_FLOAT_EQ(vec.coeff(1), 5.0f);
                EXPECT_FLOAT_EQ(vec.coeff(2), 6.0f);
            }
            else
            {
                EXPECT_FLOAT_EQ(vec.coeff(0), 13.0f);
                EXPECT_FLOAT_EQ(vec.coeff(1), 14.0f);
                EXPECT_FLOAT_EQ(vec.coeff(2), 15.0f);
            }
        }
        else if (item.kind == "matrix" && item.type == "double")
        {
            const auto mat = ZString::StringToEigenMatrix<Eigen::MatrixXd>(value, item.expected == "3x5" ? 3 : 2);
            const auto expectValue = [&](int row, int col, double expected)
            {
                const double actual = mat.coeff(row, col);
                EXPECT_DOUBLE_EQ(actual, expected);
            };

            if (item.expected == "2x2")
            {
                ASSERT_EQ(mat.rows(), 2);
                ASSERT_EQ(mat.cols(), 2);
                expectValue(0, 0, 1.0);
                expectValue(0, 1, 2.0);
                expectValue(1, 0, 3.0);
                expectValue(1, 1, 4.0);
            }
            else if (item.expected == "5x6")
            {
                ASSERT_EQ(mat.rows(), 2);
                ASSERT_EQ(mat.cols(), 2);
                expectValue(0, 0, 5.0);
                expectValue(0, 1, 6.0);
                expectValue(1, 0, 7.0);
                expectValue(1, 1, 8.0);
            }
            else if (item.expected == "3x5")
            {
                ASSERT_EQ(mat.rows(), 3);
                ASSERT_EQ(mat.cols(), 5);
                expectValue(0, 0, 1.0);
                expectValue(0, 1, 2.0);
                expectValue(0, 2, 3.0);
                expectValue(0, 3, 4.0);
                expectValue(0, 4, 6.0);
                expectValue(1, 0, 88.0);
                expectValue(1, 1, 99.0);
                expectValue(1, 2, 0.0);
                expectValue(1, 3, 0.0);
                expectValue(1, 4, 0.0);
                expectValue(2, 0, 9.0);
                expectValue(2, 1, 17.0);
                expectValue(2, 2, 78.0);
                expectValue(2, 3, 78.0);
                expectValue(2, 4, 0.0);
            }
            else
            {
                ASSERT_EQ(mat.rows(), 2);
                ASSERT_EQ(mat.cols(), 2);
                expectValue(0, 0, 9.0);
                expectValue(0, 1, 10.0);
                expectValue(1, 0, 11.0);
                expectValue(1, 1, 12.0);
            }
        }
        else
        {
            FAIL() << "Unhandled test case: " << item.kind << "|" << item.type;
        }
    }
}

TEST(ZStringConvertTest, EigenMatrixRequiresRowCount)
{
    EXPECT_THROW((ZString::StringToEigenMatrix<Eigen::Matrix2d>("1,2;3,4", 0)), std::invalid_argument);
    EXPECT_THROW((ZString::StringToEigenMatrix<Eigen::Matrix2d>("1,2,3,4", 3)), std::invalid_argument);
}

TEST(ZStringConvertTest, EigenMatrixHandlesNewlinesSpacesAndTabs)
{
    const auto spaceSeparated = ZString::StringToEigenMatrix<Eigen::Matrix2d>("1 2\n3 4", 2);
    const auto expectSpaceValue = [&](int row, int col, double expected)
    {
        const double actual = spaceSeparated.coeff(row, col);
        EXPECT_DOUBLE_EQ(actual, expected);
    };
    expectSpaceValue(0, 0, 1.0);
    expectSpaceValue(0, 1, 2.0);
    expectSpaceValue(1, 0, 3.0);
    expectSpaceValue(1, 1, 4.0);

    const auto tabSeparated = ZString::StringToEigenMatrix<Eigen::Matrix2d>("5\t6\n7\t8", 2);
    const auto expectTabValue = [&](int row, int col, double expected)
    {
        const double actual = tabSeparated.coeff(row, col);
        EXPECT_DOUBLE_EQ(actual, expected);
    };
    expectTabValue(0, 0, 5.0);
    expectTabValue(0, 1, 6.0);
    expectTabValue(1, 0, 7.0);
    expectTabValue(1, 1, 8.0);
}

TEST(ZStringConvertTest, ReadsRawVectorAndMatrixBlockFromFile)
{
    const auto rawRows = loadRawBlock(resolveDataPath("ZStringConvertCases.txt"));
    ASSERT_EQ(rawRows.size(), 4);

    const Eigen::VectorXi rawVector = ZString::StringToEigenVector<int>(rawRows[0]);
    ASSERT_EQ(rawVector.size(), 5);
    EXPECT_EQ(rawVector.coeff(0), 1);
    EXPECT_EQ(rawVector.coeff(1), 2);
    EXPECT_EQ(rawVector.coeff(2), 3);
    EXPECT_EQ(rawVector.coeff(3), 4);
    EXPECT_EQ(rawVector.coeff(4), 5);

    const std::string matrixText = rawRows[1] + "\n" + rawRows[2] + "\n" + rawRows[3];
    const Eigen::MatrixXd rawMatrix = ZString::StringToEigenMatrix<Eigen::MatrixXd>(matrixText, 3);
    ASSERT_EQ(rawMatrix.rows(), 3);
    ASSERT_EQ(rawMatrix.cols(), 5);

    const auto expectRawValue = [&](int row, int col, double expected)
    {
        const double actual = rawMatrix.coeff(row, col);
        EXPECT_DOUBLE_EQ(actual, expected);
    };
    expectRawValue(0, 0, 1.0);
    expectRawValue(0, 1, 2.0);
    expectRawValue(0, 2, 3.0);
    expectRawValue(0, 3, 4.0);
    expectRawValue(0, 4, 6.0);
    expectRawValue(1, 0, 88.0);
    expectRawValue(1, 1, 99.0);
    expectRawValue(1, 2, 0.0);
    expectRawValue(1, 3, 0.0);
    expectRawValue(1, 4, 0.0);
    expectRawValue(2, 0, 9.0);
    expectRawValue(2, 1, 17.0);
    expectRawValue(2, 2, 78.0);
    expectRawValue(2, 3, 78.0);
    expectRawValue(2, 4, 0.0);
}
