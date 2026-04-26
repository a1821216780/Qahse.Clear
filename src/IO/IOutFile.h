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
// 该文件定义文件输出接口，统一封装文本、标量、向量和矩阵数据的写入方式。
// 接口面向 IO 模块中的各类输出实现，便于以一致的格式输出日志、表格和矩阵数据。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <string>
#include <vector>
#include <Eigen/Dense>

// 保留与原始接口兼容的类型别名
using Vector = Eigen::VectorXd;
using VectorF = Eigen::VectorXf;
using Matrix = Eigen::MatrixXd;
using MatrixF = Eigen::MatrixXf;

// 常用固定尺寸类型别名
using Vector2d = Eigen::Vector2d;
using Vector3d = Eigen::Vector3d;
using Vector4d = Eigen::Vector4d;
using Vector2f = Eigen::Vector2f;
using Vector3f = Eigen::Vector3f;
using Vector4f = Eigen::Vector4f;

using Matrix2d = Eigen::Matrix2d;
using Matrix3d = Eigen::Matrix3d;
using Matrix4d = Eigen::Matrix4d;
using Matrix2f = Eigen::Matrix2f;
using Matrix3f = Eigen::Matrix3f;
using Matrix4f = Eigen::Matrix4f;


/**
 * @brief 文件输出接口，定义常用数据的统一写入方式。
 *
 * 该接口为文件输出实现提供统一抽象，支持字符串、标量、向量、矩阵
 * 以及二维数组等常见数据类型的写入。
 */
class IOutFile
{
public:
	virtual ~IOutFile() = default;

	/**
	 * @brief 获取当前实例对应的文件名。
	 * @return 文件名字符串。
	 */
	virtual std::string GetFilename() = 0;

	/**
	 * @brief 获取当前流文件路径。
	 * @return 文件路径字符串。
	 */
	virtual std::string GetStreamFilePath() = 0;

	/**
	 * @brief 设置流文件路径。
	 * @param path 新的文件路径。
	 */
	virtual void SetStreamFilePath(const std::string &path) = 0;

	/**
	 * @brief 结束输出并按需释放临时资源。
	 *
	 * 当 remove 为 true 时，同时清理临时文件或相关资源。
	 *
	 * @param remove 是否删除临时资源，默认 true。
	 */
	virtual void Outfinish(bool remove = true) = 0;

	/**
	 * @brief 写入空行。
	 */
	virtual void WriteLine() = 0;

	/**
	 * @brief 写入一条 double 数据并换行。
	 * @param message 要写入的数值。
	 */
	virtual void WriteLine(double message) = 0;

	/**
	 * @brief 写入一条字符串并换行。
	 * @param message 字符串内容。
	 */
	virtual void WriteLine(const std::string &message) = 0;

	/**
	 * @brief 写入字符串，不换行，可指定分隔符。
	 * @param message 字符串内容。
	 * @param fg 分隔符，默认制表符。
	 */
	virtual void Write(const std::string &message, const std::string &fg = "\t") = 0;

	/**
	 * @brief 写入 double 数据，不换行，可指定分隔符。
	 * @param message 要写入的数值。
	 * @param fg 分隔符，默认制表符。
	 */
	virtual void Write(double message, const std::string &fg = "\t") = 0;

	/**
	 * @brief 写入 Eigen::VectorXd 数据，不换行，可指定分隔符。
	 * @param message 要写入的向量。
	 * @param fg 分隔符，默认制表符。
	 */
	virtual void Write(const Eigen::VectorXd &message, const std::string &fg = "\t") = 0;

	/**
	 * @brief 写入 Eigen::VectorXf 数据，不换行，可指定分隔符。
	 * @param message 要写入的向量。
	 * @param fg 分隔符，默认制表符。
	 */
	virtual void Write(const Eigen::VectorXf &message, const std::string &fg = "\t") = 0;

	/**
	 * @brief 写入字符串数组并换行。
	 * @param messages 字符串数组。
	 * @param row 是否按行写入，默认 false。
	 */
	virtual void WriteLine(const std::vector<std::string> &messages, bool row = false) = 0;

	/**
	 * @brief 写入 Eigen::VectorXd 并换行，支持小数位控制。
	 * @param message 要写入的向量。
	 * @param decimalPlaces 小数位数，默认 10。
	 */
	virtual void WriteLine(const Eigen::VectorXd &message, int decimalPlaces = 10) = 0;

	/**
	 * @brief 写入 Eigen::VectorXf 并换行，支持小数位控制。
	 *
	 * 每个元素按指定小数位顺序输出。
	 *
	 * @param message 要写入的向量。
	 * @param decimalPlaces 小数位数，默认 10。
	 */
	virtual void WriteLine(const Eigen::VectorXf &message, int decimalPlaces = 10) = 0;

	/**
	 * @brief 写入 Eigen::MatrixXd 并换行。
	 * @param message 要写入的矩阵。
	 */
	virtual void WriteLine(const Eigen::MatrixXd &message) = 0;

	/**
	 * @brief 写入 Eigen::MatrixXf 并换行。
	 *
	 * 适合紧凑格式输出，便于查看结果矩阵。
	 *
	 * @param message 要写入的矩阵。
	 */
	virtual void WriteLine(const Eigen::MatrixXf &message) = 0;

	/**
	 * @brief 写入二维 double 数组并换行。
	 * @param message 要写入的二维数组。
	 */
	virtual void WriteLine(const std::vector<std::vector<double>> &message) = 0;

	// 支持 Eigen 固定尺寸类型的便捷重载

	/**
	 * @brief 写入固定尺寸向量。
	 * @tparam Size 向量长度。
	 * @param message 固定尺寸向量。
	 * @param fg 分隔符，默认制表符。
	 */
	template <int Size>
	void Write(const Eigen::Matrix<double, Size, 1> &message, const std::string &fg = "\t")
	{
		Write(message.template cast<Eigen::VectorXd>(), fg);
	}

	/**
	 * @brief 写入固定尺寸矩阵。
	 * @tparam Rows 行数。
	 * @tparam Cols 列数。
	 * @param message 固定尺寸矩阵。
	 * @param fg 分隔符，默认制表符。
	 */
	template <int Rows, int Cols>
	void Write(const Eigen::Matrix<double, Rows, Cols> &message, const std::string &fg = "\t")
	{
		Write(message.template cast<Eigen::MatrixXd>(), fg);
	}

	/**
	 * @brief 写入固定尺寸向量并换行。
	 * @tparam Size 向量长度。
	 * @param message 固定尺寸向量。
	 * @param decimalPlaces 小数位数，默认 10。
	 */
	template <int Size>
	void WriteLine(const Eigen::Matrix<double, Size, 1> &message, int decimalPlaces = 10)
	{
		WriteLine(message.template cast<Eigen::VectorXd>(), decimalPlaces);
	}

	/**
	 * @brief 写入固定尺寸矩阵并换行。
	 * @tparam Rows 行数。
	 * @tparam Cols 列数。
	 * @param message 固定尺寸矩阵。
	 */
	template <int Rows, int Cols>
	void WriteLine(const Eigen::Matrix<double, Rows, Cols> &message)
	{
		WriteLine(message.template cast<Eigen::MatrixXd>());
	}
};

