//**********************************************************************************************************************************
// 许可证说明
// 版权所有(C) 2021, 2025  赵子祯
//
// 根据 Boost 软件许可证 - 版本 1.0 - 2003年8月17日
// 您不得使用此文件，除非符合许可证。
// 您可以在以下网址获得许可证副本
//
//     http://www.hawtc.cn/licenses.txt
//
// 该软件按“原样”提供，不提供任何明示或暗示的保证，包括但不限于适销性、特定用途的适用性、所有权和非侵权。在任何情况下，版权持有人或任何分
// 发软件的人都不对任何索赔、损害或其他责任负责，无论是在合同诉讼、侵权诉讼或其他诉讼中，还是在软件使用或其他交易中产生的。
//
//**********************************************************************************************************************************

// ───────────────────────────────── File Info ─────────────────────────────────
//
// 该文件定义了 MSExcel 类，提供对 Excel 文件的读写操作封装。支持多种数据类型的单元格
// 操作，并通过 OpenXLSX 库实现与 Excel 文件的交互。
//
// ──────────────────────────────────────────────────────────────────────────────



#pragma once

#include <OpenXLSX.hpp>
#include <Eigen/Dense>
#include <string>
#include <vector>

class MSExcel
{
public:
    // Constructor & Destructor
    explicit MSExcel(const std::string &path, const std::string &mode);
    ~MSExcel();

    // Prevent copying
    MSExcel(const MSExcel &) = delete;
    MSExcel &operator=(const MSExcel &) = delete;

    // Sheet operations
    void AddSheet(const std::string &name);
    bool SheetExist(const std::string &name);
    OpenXLSX::XLWorksheet GetSheet(const std::string &name);

    // Sheet information methods
    int RowCount(const std::string &sheetname);
    int RowCount(const OpenXLSX::XLWorksheet &worksheet);
    int ColumnCount(const std::string &sheetname);
    int ColumnCount(const OpenXLSX::XLWorksheet &worksheet);

    // Close Excel file
    void Close();

    // Template declarations for cell operations
    template <typename T>
    void WCellValue(const std::string &sheetname, const T &value, int row, int column, bool columnwr = false);

    template <typename T>
    T RCellValue(const std::string &sheetname, int row, int column, int rowcount = 0, int columncount = 0);

private:
    std::string path;
    OpenXLSX::XLWorkbook workbook;
    OpenXLSX::XLDocument doc;
};

// Template specialization declarations
template <>
void MSExcel::WCellValue<double>(const std::string &sheetname, const double &value, int row, int column, bool columnwr);
template <>
void MSExcel::WCellValue<int>(const std::string &sheetname, const int &value, int row, int column, bool columnwr);
template <>
void MSExcel::WCellValue<std::string>(const std::string &sheetname, const std::string &value, int row, int column, bool columnwr);
template <>
void MSExcel::WCellValue<bool>(const std::string &sheetname, const bool &value, int row, int column, bool columnwr);
template <>
void MSExcel::WCellValue<std::vector<double>>(const std::string &sheetname, const std::vector<double> &value, int row, int column, bool columnwr);
template <>
void MSExcel::WCellValue<Eigen::VectorXd>(const std::string &sheetname, const Eigen::VectorXd &value, int row, int column, bool columnwr);
template <>
void MSExcel::WCellValue<Eigen::MatrixXd>(const std::string &sheetname, const Eigen::MatrixXd &value, int row, int column, bool columnwr);

template <>
double MSExcel::RCellValue<double>(const std::string &sheetname, int row, int column, int rowcount, int columncount);
template <>
int MSExcel::RCellValue<int>(const std::string &sheetname, int row, int column, int rowcount, int columncount);
template <>
std::string MSExcel::RCellValue<std::string>(const std::string &sheetname, int row, int column, int rowcount, int columncount);
template <>
bool MSExcel::RCellValue<bool>(const std::string &sheetname, int row, int column, int rowcount, int columncount);
template <>
std::vector<double> MSExcel::RCellValue<std::vector<double>>(const std::string &sheetname, int row, int column, int rowcount, int columncount);
template <>
Eigen::VectorXd MSExcel::RCellValue<Eigen::VectorXd>(const std::string &sheetname, int row, int column, int rowcount, int columncount);
template <>
Eigen::MatrixXd MSExcel::RCellValue<Eigen::MatrixXd>(const std::string &sheetname, int row, int column, int rowcount, int columncount);
