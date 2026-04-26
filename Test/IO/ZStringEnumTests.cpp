#include <gtest/gtest.h>

#include <Eigen/Dense>
#include <fstream>
#include <string>
#include <vector>

#include "../../src/IO/ZString.hpp"

namespace
{
    enum class Color
    {
        Red,
        Green,
        Blue
    };

    enum class Mode
    {
        Auto,
        Manual,
        Emergency
    };

    struct CaseRow
    {
        std::string kind;
        std::string type;
        std::string value;
        std::string expected;
    };

    std::vector<CaseRow> loadCases(const std::string &path)
    {
        std::vector<CaseRow> rows;
        for (const auto &line : ZString::ReadAllLines(path))
        {
            const std::string trimmed = ZString::Trim(line);
            if (trimmed.empty() || ZString::StartsWith(trimmed, "#"))
                continue;

            const auto parts = ZString::Split(trimmed, '|', false);
            if (parts.size() != 4)
                throw std::runtime_error("测试数据格式错误: " + trimmed);

            rows.push_back(CaseRow{parts[0], parts[1], parts[2], parts[3]});
        }
        return rows;
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
                (void)loadCases(candidate);
                return candidate;
            }
            catch (...)
            {
            }
        }

        throw std::runtime_error("无法找到测试数据文件: " + filename);
    }
}

TEST(ZStringEnumTest, ConvertsEnumsAndScalarsFromFile)
{
    const auto cases = loadCases(resolveDataPath("ZStringEnumCases.txt"));
    ASSERT_FALSE(cases.empty());

    for (const auto &item : cases)
    {
        if (item.kind == "enum" && item.type == "Color")
        {
            const Color parsed = ZString::StringToEnum<Color>(item.value);
            EXPECT_EQ(parsed, Color::Red);
            EXPECT_EQ(std::string(magic_enum::enum_name(parsed)), item.expected);
        }
        else if (item.kind == "enum" && item.type == "Mode")
        {
            const Mode parsed = ZString::StringToEnum<Mode>(item.value);
            EXPECT_EQ(parsed, Mode::Manual);
            EXPECT_EQ(std::string(magic_enum::enum_name(parsed)), item.expected);
        }
        else if (item.kind == "bool")
        {
            EXPECT_EQ(ZString::StringToBool(item.value), item.expected == "true");
        }
        else if (item.kind == "int")
        {
            EXPECT_EQ(ZString::StringToInt(item.value), std::stoi(item.expected));
        }
        else if (item.kind == "float")
        {
            EXPECT_FLOAT_EQ(ZString::StringToFloat(item.value), std::stof(item.expected));
        }
        else if (item.kind == "double")
        {
            EXPECT_DOUBLE_EQ(ZString::StringToDouble(item.value), std::stod(item.expected));
        }
        else if (item.kind == "vector" && item.type == "int")
        {
            const auto vec = ZString::StringToEigenVector<int>(item.value);
            ASSERT_EQ(vec.size(), 3);
            EXPECT_EQ(vec(0), 1);
            EXPECT_EQ(vec(1), 2);
            EXPECT_EQ(vec(2), 3);
        }
        else if (item.kind == "vector" && item.type == "float")
        {
            const auto vec = ZString::StringToEigenVector<float>(item.value);
            ASSERT_EQ(vec.size(), 3);
            EXPECT_FLOAT_EQ(vec(0), 1.5f);
            EXPECT_FLOAT_EQ(vec(1), 2.5f);
            EXPECT_FLOAT_EQ(vec(2), 3.5f);
        }
        else if (item.kind == "rowvector" && item.type == "double")
        {
            const auto vec = ZString::StringToEigenRowVector<double>(item.value);
            ASSERT_EQ(vec.size(), 3);
            EXPECT_DOUBLE_EQ(vec(0), 4.5);
            EXPECT_DOUBLE_EQ(vec(1), 5.5);
            EXPECT_DOUBLE_EQ(vec(2), 6.5);
        }
        else if (item.kind == "matrix" && item.type == "double")
        {
            const auto mat = ZString::StringToEigenMatrix<Eigen::Matrix2d>(item.value, 2);
            EXPECT_DOUBLE_EQ(mat(0, 0), 1.0);
            EXPECT_DOUBLE_EQ(mat(0, 1), 2.0);
            EXPECT_DOUBLE_EQ(mat(1, 0), 3.0);
            EXPECT_DOUBLE_EQ(mat(1, 1), 4.0);
        }
        else
        {
            FAIL() << "Unhandled test case: " << item.kind << "|" << item.type;
        }
    }
}

TEST(ZStringEnumTest, EnumConversionIsCaseInsensitive)
{
    EXPECT_EQ(ZString::StringToEnum<Color>("green"), Color::Green);
    EXPECT_EQ(ZString::StringToEnum<Mode>("MANUAL"), Mode::Manual);
}

TEST(ZStringEnumTest, EnumConversionRejectsUnknownValues)
{
    EXPECT_THROW((ZString::StringToEnum<Color>("not_a_color")), std::invalid_argument);
}
