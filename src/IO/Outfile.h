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
// 该文件定义了 OutFile 类，提供将数据写入文件或流的功能，支持数值格式化选项，包括
// 科学计数法和固定小数位数。它支持写入单个值、向量和矩阵，并包含管理输出流生命周期的方法。
//
// ──────────────────────────────────────────────────────────────────────────────


#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <Eigen/Dense>
#include "IOutFile.h"
#include "LogHelper.h"
#include "OtherHelper.hpp"

/**
 * @brief 表示用于将数据写入流的输出文件，支持各种格式化选项。
 *
 * OutFile 类提供将数据写入文件或流的功能，支持数值格式化选项，
 * 包括科学计数法和固定小数位数。它支持写入单个值、向量和矩阵，
 * 并包含管理输出流生命周期的方法。
 *
 * 主要特性：
 * - 支持科学计数法和固定小数位数格式化
 * - 可写入字符串、数值、向量和矩阵数据
 * - 自动管理文件扩展名
 * - 集成日志系统防止文件名冲突
 * - 支持Eigen矩阵格式输出
 */
class OutFile : public IOutFile
{
private:
	std::unique_ptr<std::ofstream> _writer; ///< 输出文件写入流（unique_ptr 管理生命周期）

	std::string filename_;                   ///< 输出文件完整路径（含扩展名）

	std::string filemat_;                    ///< fmt 格式化字符串（如 "{:.6f}" 或 "{:.4E}"）

	int decimalPlaces_;                      ///< 小数位数（固定小数记法）
	int scientificDigits_;                   ///< 科学计数法有效数字位数
	bool useScientific_;                     ///< 是否使用科学计数法模式

	/**
	 * @brief 初始化 fmt 格式化字符串
	 */
	inline void initializeFormat(int decimalPlaces, int Scientific);
	/**
	 * @brief 使用 fmt 库格式化 double 值
	 */
	inline std::string formatDouble(double value) const;
	/**
	 * @brief 使用 fmt 库格式化 float 值
	 */
	inline std::string formatFloat(float value) const;
	/**
	 * @brief 获取文件路径中的扩展名
	 */
	inline std::string GetFileExtension(const std::string &path) const;
	/**
	 * @brief 检查当前文件名是否与全局文件列表冲突
	 */
	inline void checkFileNameConflict() const;

public:
	/**
	 * @brief 与当前操作关联的流名称
	 */
	std::string StreamName;            ///< 输出流名称（仅含文件名，不含目录）

	/**
	 * @brief 流的文件路径
	 */
	std::string StreanFilePath;        ///< 输出流文件完整路径

	/**
	 * @brief 获取与当前实例关联的文件名
	 */
	std::string GetFilename() override;

	/**
	 * @brief 获取流文件路径
	 */
	std::string GetStreamFilePath() override;

	/**
	 * @brief 设置流文件路径
	 */
	void SetStreamFilePath(const std::string &path) override;
	/**
	 * @brief 构造函数
	 * @param path 将写入输出的文件路径。如果缺少文件扩展名，则添加默认扩展名".3zout"
	 * @param decimalPlaces 用于格式化数值输出的小数位数。如果指定了Scientific，则忽略此参数
	 * @param Scientific 用于科学计数法格式化的有效数字位数。如果设置为-1，则不使用科学计数法
	 * @throws std::runtime_error 如果内部写入器初始化失败
	 */
	explicit OutFile(const std::string &path, int decimalPlaces = 6, int Scientific = -1);

	OutFile(const OutFile &) = delete;

	/**
	 * @brief 析构函数，自动关闭文件流
	 */
	virtual ~OutFile();

	// IOutFile接口实现
	/**
	 * @brief 将字符串写入输出文件
	 * @param message 要写入的字符串
	 * @param fg 写入后的尾随分隔符，默认 "\t"
	 */
	void Write(const std::string &message, const std::string &fg = "\t") override;
	/**
	 * @brief 将 double 值格式化写入输出文件
	 * @param message 要写入的数值
	 * @param fg 尾随分隔符，默认 "\t"
	 */
	void Write(double message, const std::string &fg = "\t") override;
	/**
	 * @brief 将 float 值格式化写入输出文件
	 * @param message 要写入的数值
	 * @param fg 尾随分隔符，默认 "\t"
	 */
	void Write(float message, const std::string &fg = "\t");
	/**
	 * @brief 写入一个换行符
	 */
	void WriteLine() override;
	/**
	 * @brief 将 double 值格式化写入并换行
	 * @param message 要写入的数值
	 */
	void WriteLine(double message) override;
	/**
	 * @brief 将 float 值格式化写入并换行
	 * @param message 要写入的数值
	 */
	void WriteLine(float message);
	/**
	 * @brief 将字符串写入并换行
	 * @param message 要写入的字符串
	 */
	void WriteLine(const std::string &message) override;
	/**
	 * @brief 关闭文件流并可选从全局列表移除
	 * @param remove 是否从全局文件列表中移除，默认 true
	 */
	void Outfinish(bool remove = true) override;

	/**
	 * @brief 将Eigen向量写入输出，元素之间用制表符分隔
	 * @param message 包含要写入元素的向量
	 * @param fg 元素之间使用的分隔符字符串。默认为制表符
	 */
	void Write(const Eigen::VectorXd &message, const std::string &fg = "\t");
	/**
	 * @brief 将 Eigen::VectorXf 向量写入输出（自动转为 double）
	 * @param message 要写入的单精度向量
	 * @param fg 元素间分隔符，默认为 "\t"
	 */
	void Write(const Eigen::VectorXf &message, const std::string &fg = "\t");

	/**
	 * @brief 将Eigen矩阵写入输出，每行之间用换行符分隔
	 * @param message 要写入其行的矩阵
	 * @param fg 用于分隔每行内元素的分隔符。默认为制表符
	 */
	void Write(const Eigen::MatrixXd &message, const std::string &fg = "\t");
	/**
	 * @brief 将 Eigen::MatrixXf 矩阵写入输出（自动转为 double）
	 * @param message 要写入的单精度矩阵
	 * @param fg 行内元素间分隔符，默认为 "\t"
	 */
	void Write(const Eigen::MatrixXf &message, const std::string &fg = "\t");

	/**
	 * @brief 批量写入多个字符串消息
	 * @param fg 应用于每个消息的格式化字符串
	 * @param messages 要写入的消息向量
	 */
	void Write(const std::string &fg, const std::vector<std::string> &messages);

	/**
	 * @brief 批量写入多个数值
	 * @param fg 用于格式化每个值的前景字符串
	 * @param messages 要写入输出的double值向量
	 */
	void Write(const std::string &fg, const std::vector<double> &messages);

	/**
	 * @brief 写入格式化的标题和其他信息
	 * @param title 要写入的标题
	 */
	void WriteTitle(const std::string &title);

	/**
	 * @brief 将Eigen向量写入输出并换行
	 * @param message 包含要写入数据的向量
	 * @param decimalPlaces 格式化输出的小数位数。默认为10
	 */
	void WriteLine(const Eigen::VectorXd &message, int decimalPlaces = 10);
	/**
	 * @brief 将 Eigen::VectorXf 向量写入输出并换行（自动转为 double）
	 * @param message 要写入的单精度向量
	 * @param decimalPlaces 格式化输出的小数位数，默认为 10
	 */
	void WriteLine(const Eigen::VectorXf &message, int decimalPlaces = 10);

	/**
	 * @brief 将Eigen矩阵写入输出并换行
	 * @param message 要写入的矩阵
	 */
	void WriteLine(const Eigen::MatrixXd &message);
	/**
	 * @brief 将 Eigen::MatrixXf 矩阵写入输出并换行（自动转为 double）
	 * @param message 要写入的单精度矩阵
	 */
	void WriteLine(const Eigen::MatrixXf &message);

	/**
	 * @brief 写入字符串数组，每个字符串后跟换行符
	 * @param messages 要写入的字符串向量
	 */
	void WriteLine(const std::vector<std::string> &messages, bool row = false);

	/**
	 * @brief 写入数值数组并换行
	 * @param messages 要处理和写入的double数组
	 */
	void WriteLine(const std::vector<double> &messages);

	/**
	 * @brief 写入二维数组并换行
	 * @param message 要处理并写入输出的二维double数组
	 */
	void WriteLine(const std::vector<std::vector<double>> &message);
};
