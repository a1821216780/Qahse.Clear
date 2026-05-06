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

/**
 * @class MSExcel
 * @brief 提供对 Excel 文件的读写操作封装，通过 OpenXLSX 库实现与 Excel 文件的交互。
 *
 * 支持多种数据类型的单元格读写：double、int、std::string、bool、std::vector<double>、
 * Eigen::VectorXd、Eigen::MatrixXd。支持 read、write、append 三种文件打开模式。
 */
class MSExcel
{
public:
	/**
	 * @brief 构造并初始化 MSExcel 对象，按指定模式打开或创建 Excel 文件。
	 * @details 支持以下模式（大小写不敏感）：
	 *  - \"read\" / \"r\"：以只读方式打开现有文件。
	 *  - \"write\" / \"w\"：创建新文件（若已存在则强制覆盖）。
	 *  - \"append\" / \"a\"：以增量方式打开现有文件，若文件不存在则先创建再打开。
	 * @param path 目标 Excel 文件的路径。
	 * @param mode 模式字符串，可选值为 \"read\"/\"r\"、\"write\"/\"w\"、\"append\"/\"a\"。
	 */
	explicit MSExcel(const std::string &path, const std::string &mode);

	/**
	 * @brief 析构函数，自动保存并关闭 Excel 文件。
	 */
	~MSExcel();

	// Prevent copying
	MSExcel(const MSExcel &) = delete;
	MSExcel &operator=(const MSExcel &) = delete;

	/**
	 * @brief 如果指定名称的工作表不存在，则在工作簿中新建工作表。
	 * @param name 要新建的工作表名称。
	 */
	void AddSheet(const std::string &name);

	/**
	 * @brief 检查指定名称的工作表是否存在。
	 * @param name 工作表名称。
	 * @return 工作表存在返回 true，否则返回 false。
	 */
	bool SheetExist(const std::string &name);

	/**
	 * @brief 获取指定名称的工作表，若不存在则自动创建。
	 * @param name 工作表名称。
	 * @return XLWorksheet 对象，可用于单元格读写。
	 */
	OpenXLSX::XLWorksheet GetSheet(const std::string &name);

	/**
	 * @brief 获取指定名称工作表的行数。
	 * @param sheetname 工作表名称。
	 * @return 行数整数；工作表不存在时返回 0。
	 */
	int RowCount(const std::string &sheetname);

	/**
	 * @brief 获取工作表对象的行数。
	 * @param worksheet XLWorksheet 对象。
	 * @return 行数整数。
	 */
	int RowCount(const OpenXLSX::XLWorksheet &worksheet);

	/**
	 * @brief 获取指定名称工作表的列数。
	 * @param sheetname 工作表名称。
	 * @return 列数整数；工作表不存在时返回 0。
	 */
	int ColumnCount(const std::string &sheetname);

	/**
	 * @brief 获取工作表对象的列数。
	 * @param worksheet XLWorksheet 对象。
	 * @return 列数整数。
	 */
	int ColumnCount(const OpenXLSX::XLWorksheet &worksheet);

	/**
	 * @brief 保存并关闭 Excel 文件。
	 */
	void Close();

	/**
	 * @brief 将数据写入指定工作表的单元格。
	 * @tparam T 数据类型（支持 double、int、std::string、bool、std::vector<double>、Eigen::VectorXd、Eigen::MatrixXd）。
	 * @param sheetname 目标工作表名称。
	 * @param value 要写入的数据。
	 * @param row 目标单元格的行索引（0 基索引）。
	 * @param column 目标单元格的列索引（0 基索引）。
	 * @param columnwr 向量写入方向：true 按列写入，false 按行写入（标量无意义）。
	 */
	template <typename T>
	void WCellValue(const std::string &sheetname, const T &value, int row, int column, bool columnwr = false);

	/**
	 * @brief 从工作表指定单元格读取数据。
	 * @tparam T 数据类型（支持 double、int、std::string、bool、std::vector<double>、Eigen::VectorXd、Eigen::MatrixXd）。
	 * @param sheetname 目标工作表名称。
	 * @param row 行索引（0 基）。
	 * @param column 列索引（0 基）。
	 * @param rowcount 自动读取的行数（向量/矩阵读取时使用，标量无意义）。
	 * @param columncount 自动读取的列数（矩阵读取时使用，标量无意义）。
	 * @return 单元格中的数据。
	 */
	template <typename T>
	T RCellValue(const std::string &sheetname, int row, int column, int rowcount = 0, int columncount = 0);

private:
	std::string path;                       /**< 目标 Excel 文件路径 */
	OpenXLSX::XLWorkbook workbook;          /**< 工作簿对象，对应 OpenXLSX 的内部工作簿 */
	OpenXLSX::XLDocument doc;               /**< 文档对象，对应 OpenXLSX 的文档 */
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
