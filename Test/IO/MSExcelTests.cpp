#include <gtest/gtest.h>

#include <Eigen/Dense>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "../../src/IO/MSExcel.h"

namespace
{
class ScopedTempRoot
{
public:
    ScopedTempRoot()
        : root_(std::filesystem::temp_directory_path() / "Qahse_Clear_MSExcelTests")
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

TEST(MSExcelTest, ScalarAndMatrixRoundTrip)
{
    const ScopedTempRoot root;
    const auto file = root.path("sheet.xlsx").string();

    {
        MSExcel excel(file, "write");
        excel.WCellValue<double>("Data", 12.5, 0, 0);
        excel.WCellValue<int>("Data", 7, 1, 0);
        excel.WCellValue<std::string>("Data", std::string("alpha"), 2, 0);

        Eigen::MatrixXd mat(2, 2);
        mat << 1.0, 2.0, 3.0, 4.0;
        excel.WCellValue<Eigen::MatrixXd>("Data", mat, 4, 1);
        excel.Close();
    }

    {
        MSExcel excel(file, "read");
        EXPECT_DOUBLE_EQ(excel.RCellValue<double>("Data", 0, 0), 12.5);
        EXPECT_EQ(excel.RCellValue<int>("Data", 1, 0), 7);
        EXPECT_EQ(excel.RCellValue<std::string>("Data", 2, 0), "alpha");

        const Eigen::MatrixXd got = excel.RCellValue<Eigen::MatrixXd>("Data", 4, 1, 2, 2);
        ASSERT_EQ(got.rows(), 2);
        ASSERT_EQ(got.cols(), 2);
        EXPECT_DOUBLE_EQ(got(0, 0), 1.0);
        EXPECT_DOUBLE_EQ(got(0, 1), 2.0);
        EXPECT_DOUBLE_EQ(got(1, 0), 3.0);
        EXPECT_DOUBLE_EQ(got(1, 1), 4.0);
        excel.Close();
    }
}

TEST(MSExcelTest, ReadVectorWithAutoRowCountStopsAtFirstEmptyCell)
{
    const ScopedTempRoot root;
    const auto file = root.path("vector.xlsx").string();

    {
        MSExcel excel(file, "write");
        const std::vector<double> values = {10.0, 20.5, -3.25};
        excel.WCellValue<std::vector<double>>("Data", values, 0, 0, true);
        excel.Close();
    }

    {
        MSExcel excel(file, "read");
        std::vector<double> result;
        EXPECT_NO_THROW(result = excel.RCellValue<std::vector<double>>("Data", 0, 0, 0, 0));
        ASSERT_EQ(result.size(), 3u);
        EXPECT_DOUBLE_EQ(result[0], 10.0);
        EXPECT_DOUBLE_EQ(result[1], 20.5);
        EXPECT_DOUBLE_EQ(result[2], -3.25);
        excel.Close();
    }
}
