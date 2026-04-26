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
// 该文件提供一个类似 C# System.String 的跨平台字符串工具类。
// 支持大小写转换、裁剪、替换、分割、拼接、文件读写和常用文本判断操作，
// 适合 IO 模块和工具代码直接使用。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <algorithm>
#include <Eigen/Dense>
#include <cstddef>
#include <cctype>
#include <initializer_list>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "magic_enum.hpp"
#include "ZPath.hpp"

class ZString
{
public:
	/**
	 * @brief 将字符串转换为大写。
	 * @param value 输入字符串。
	 * @return 大写字符串。
	 */
	static std::string ToUpper(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
					   { return static_cast<char>(std::toupper(ch)); });
		return value;
	}

	/**
	 * @brief 原地将字符串转换为大写。
	 * @param value 输入字符串。
	 */
	static void ToUpper(std::string &value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
					   { return static_cast<char>(std::toupper(ch)); });
	}

	/**
	 * @brief 将字符串转换为小写。
	 * @param value 输入字符串。
	 * @return 小写字符串。
	 */
	static std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
					   { return static_cast<char>(std::tolower(ch)); });
		return value;
	}

	/**
	 * @brief 原地将字符串转换为小写。
	 * @param value 输入字符串。
	 */
	static void ToLower(std::string &value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
					   { return static_cast<char>(std::tolower(ch)); });
	}

	/**
	 * @brief 判断给定字符串是否为回文字符串。
	 * @param value 输入字符串。
	 * @return 正读反读相同返回 true。
	 */
	static bool IsPalindrome(const std::string &value)
	{
		if (value.empty())
			return true;

		size_t left = 0;
		size_t right = value.size() - 1;
		while (left < right)
		{
			if (value[left] != value[right])
				return false;
			++left;
			--right;
		}
		return true;
	}

	/**
	 * @brief 判断字符串是否为有效的标识符（字母或下划线开头，后续字符为字母、数字或下划线）。
	 * @param value 输入字符串。
	 * @return 如果是有效标识符则返回 true，否则返回 false。
	 */
	static bool IsIdentifier(const std::string &value)
	{
		if (value.empty())
			return false;
		const auto first = static_cast<unsigned char>(value.front());
		if (std::isalpha(first) == 0 && value.front() != '_')
			return false;

		return std::all_of(value.begin() + 1, value.end(), [](unsigned char ch)
						   { return std::isalnum(ch) != 0 || ch == '_'; });
	}

	/**
	 * @brief 去除首尾空白字符。
	 * @param value 输入字符串。
	 * @return 去除首尾空白后的字符串。
	 */
	static std::string Trim(const std::string &value)
	{
		return TrimEnd(TrimStart(value));
	}

	/**
	 * @brief 去除左侧空白字符。
	 * @param value 输入字符串。
	 * @return 去除左侧空白后的字符串。
	 */
	static std::string TrimStart(const std::string &value)
	{
		auto it = std::find_if_not(value.begin(), value.end(), [](unsigned char ch)
								   { return std::isspace(ch) != 0; });
		return std::string(it, value.end());
	}

	/**
	 * @brief 去除右侧空白字符。
	 * @param value 输入字符串。
	 * @return 去除右侧空白后的字符串。
	 */
	static std::string TrimEnd(const std::string &value)
	{
		auto it = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch)
								   { return std::isspace(ch) != 0; });
		return std::string(value.begin(), it.base());
	}

	/**
	 * @brief 判断字符串是否为空或仅包含空白字符。
	 * @param value 输入字符串。
	 * @return 空白字符串返回 true。
	 */
	static bool IsNullOrWhiteSpace(const std::string &value)
	{
		return std::all_of(value.begin(), value.end(), [](unsigned char ch)
						   { return std::isspace(ch) != 0; });
	}

	/**
	 * @brief 判断字符串是否以指定前缀开头。
	 * @param value 输入字符串。
	 * @param prefix 前缀。
	 * @param ignoreCase 是否忽略大小写。
	 * @return 以指定前缀开头返回 true。
	 */
	static bool StartsWith(const std::string &value, const std::string &prefix, bool ignoreCase = false)
	{
		if (prefix.size() > value.size())
			return false;

		if (!ignoreCase)
			return std::equal(prefix.begin(), prefix.end(), value.begin());

		for (size_t i = 0; i < prefix.size(); ++i)
		{
			if (std::tolower(static_cast<unsigned char>(value[i])) !=
				std::tolower(static_cast<unsigned char>(prefix[i])))
				return false;
		}
		return true;
	}

	/**
	 * @brief 判断字符串是否以指定后缀结尾。
	 * @param value 输入字符串。
	 * @param suffix 后缀。
	 * @param ignoreCase 是否忽略大小写。
	 * @return 以指定后缀结尾返回 true。
	 */
	static bool EndsWith(const std::string &value, const std::string &suffix, bool ignoreCase = false)
	{
		if (suffix.size() > value.size())
			return false;

		auto offset = value.size() - suffix.size();
		if (!ignoreCase)
			return std::equal(suffix.begin(), suffix.end(), value.begin() + static_cast<std::ptrdiff_t>(offset));

		for (size_t i = 0; i < suffix.size(); ++i)
		{
			if (std::tolower(static_cast<unsigned char>(value[offset + i])) !=
				std::tolower(static_cast<unsigned char>(suffix[i])))
				return false;
		}
		return true;
	}

	/**
	 * @brief 判断字符串是否包含子串。
	 * @param value 输入字符串。
	 * @param token 子串。
	 * @param ignoreCase 是否忽略大小写。
	 * @return 包含子串返回 true。
	 */
	static bool Contains(const std::string &value, const std::string &token, bool ignoreCase = false)
	{
		if (token.empty())
			return true;

		if (!ignoreCase)
			return value.find(token) != std::string::npos;

		return ToLower(value).find(ToLower(token)) != std::string::npos;
	}

	/**
	 * @brief 替换字符串中的所有匹配项。
	 * @param value 输入字符串。
	 * @param oldValue 旧字符串。
	 * @param newValue 新字符串。
	 * @return 替换后的字符串。
	 */
	static std::string Replace(const std::string &value, const std::string &oldValue, const std::string &newValue)
	{
		if (oldValue.empty())
			return value;

		std::string result = value;
		size_t pos = 0;
		while ((pos = result.find(oldValue, pos)) != std::string::npos)
		{
			result.replace(pos, oldValue.size(), newValue);
			pos += newValue.size();
		}
		return result;
	}

	/**
	 * @brief 替换字符串中的字符。
	 * @param value 输入字符串。
	 * @param oldValue 旧字符。
	 * @param newValue 新字符。
	 * @return 替换后的字符串。
	 */
	static std::string Replace(const std::string &value, char oldValue, char newValue)
	{
		std::string result = value;
		std::replace(result.begin(), result.end(), oldValue, newValue);
		return result;
	}

	/**
	 * @brief 将字符替换为字符串。
	 * @param value 输入字符串。
	 * @param oldValue 旧字符。
	 * @param newValue 新字符串。
	 * @return 替换后的字符串。
	 */
	static std::string Replace(const std::string &value, char oldValue, const std::string &newValue)
	{
		std::string result = value;
		size_t pos = 0;
		while ((pos = result.find(oldValue, pos)) != std::string::npos)
		{
			result.replace(pos, 1, newValue);
			pos += newValue.size();
		}
		return result;
	}

	/**
	 * @brief 将字符串替换为字符。
	 * @param value 输入字符串。
	 * @param oldValue 旧字符串。
	 * @param newValue 新字符。
	 * @return 替换后的字符串。
	 */
	static std::string Replace(const std::string &value, const std::string &oldValue, char newValue)
	{
		if (oldValue.empty())
			return value;

		std::string result = value;
		size_t pos = 0;
		while ((pos = result.find(oldValue, pos)) != std::string::npos)
		{
			result.replace(pos, oldValue.size(), std::string(1, newValue));
			pos += 1;
		}
		return result;
	}

	/**
	 * @brief 按分隔符拆分字符串。
	 * @param value 输入字符串。
	 * @param delimiter 分隔符。
	 * @param removeEmpty 是否移除空项。
	 * @return 拆分后的字符串数组。
	 */
	static std::vector<std::string> Split(const std::string &value, const std::string &delimiter, bool removeEmpty = false)
	{
		std::vector<std::string> result;
		if (delimiter.empty())
		{
			if (!value.empty() || !removeEmpty)
				result.push_back(value);
			return result;
		}

		size_t start = 0;
		while (start <= value.size())
		{
			size_t pos = value.find(delimiter, start);
			std::string token = value.substr(start, pos == std::string::npos ? std::string::npos : pos - start);
			if (!token.empty() || !removeEmpty)
				result.push_back(token);
			if (pos == std::string::npos)
				break;
			start = pos + delimiter.size();
		}
		return result;
	}

	/**
	 * @brief 按单个字符拆分字符串。
	 * @param value 输入字符串。
	 * @param delimiter 分隔字符。
	 * @return 拆分后的字符串数组。
	 */
	static std::vector<std::string> Split(const std::string &value, char delimiter)
	{
		std::vector<std::string> result;
		std::stringstream stream(value);
		std::string item;
		while (std::getline(stream, item, delimiter))
		{
			if (!item.empty())
				result.push_back(item);
		}
		return result;
	}

	/**
	 * @brief 按任意分隔字符拆分字符串。
	 * @param value 输入字符串。
	 * @param delimiter 分隔字符集合。
	 * @return 拆分后的字符串数组。
	 */
	static std::vector<std::string> Split(const std::string &value, const char *delimiter)
	{
		std::vector<std::string> result;
		if (delimiter == nullptr || *delimiter == '\0')
		{
			result.push_back(value);
			return result;
		}

		size_t start = 0;
		size_t end = value.find_first_of(delimiter);
		while (end != std::string::npos)
		{
			result.push_back(value.substr(start, end - start));
			start = end + 1;
			end = value.find_first_of(delimiter, start);
		}
		result.push_back(value.substr(start));
		return result;
	}

	/**
	 * @brief 按单个字符拆分字符串。
	 * @param value 输入字符串。
	 * @param delimiter 分隔字符。
	 * @param removeEmpty 是否移除空项。
	 * @return 拆分后的字符串数组。
	 */
	static std::vector<std::string> Split(const std::string &value, char delimiter, bool removeEmpty = false)
	{
		return Split(value, std::string(1, delimiter), removeEmpty);
	}

	/**
	 * @brief 使用分隔符拼接字符串数组。
	 * @param values 待拼接字符串数组。
	 * @param delimiter 分隔符。
	 * @return 拼接后的字符串。
	 */
	static std::string Join(const std::vector<std::string> &values, const std::string &delimiter)
	{
		if (values.empty())
			return {};

		std::ostringstream stream;
		for (size_t i = 0; i < values.size(); ++i)
		{
			if (i > 0)
				stream << delimiter;
			stream << values[i];
		}
		return stream.str();
	}

	/**
	 * @brief 连接多个字符串片段。
	 * @param values 待拼接字符串数组。
	 * @return 拼接后的字符串。
	 */
	static std::string Concat(const std::vector<std::string> &values)
	{
		std::ostringstream stream;
		for (const auto &value : values)
			stream << value;
		return stream.str();
	}

	/**
	 * @brief 连接多个字符串片段。
	 * @param values 初始化列表。
	 * @return 拼接后的字符串。
	 */
	static std::string Concat(std::initializer_list<std::string> values)
	{
		std::ostringstream stream;
		for (const auto &value : values)
			stream << value;
		return stream.str();
	}

	/**
	 * @brief 生成重复字符串。
	 * @param value 重复内容。
	 * @param count 重复次数。
	 * @return 重复后的字符串。
	 */
	static std::string Repeat(const std::string &value, size_t count)
	{
		std::string result;
		result.reserve(value.size() * count);
		for (size_t i = 0; i < count; ++i)
			result += value;
		return result;
	}

	/**
	 * @brief 将字符串转换为指定标量类型。
	 * @tparam T 目标类型。
	 * @param value 输入字符串。
	 * @return 转换后的结果。
	 */
	template <typename T>
	static T StringTo(const std::string &value)
	{
		const std::string text = Trim(value);

		if constexpr (std::is_same_v<T, std::string>)
		{
			return text;
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			return StringToBool(text);
		}
		else if constexpr (std::is_integral_v<T>)
		{
			size_t consumed = 0;
			if constexpr (std::is_signed_v<T>)
			{
				long long parsed = std::stoll(text, &consumed, 0);
				if (consumed != text.size())
					throw std::invalid_argument("无法转换为整数: " + text);
				return static_cast<T>(parsed);
			}
			else
			{
				unsigned long long parsed = std::stoull(text, &consumed, 0);
				if (consumed != text.size())
					throw std::invalid_argument("无法转换为整数: " + text);
				return static_cast<T>(parsed);
			}
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			size_t consumed = 0;
			long double parsed = std::stold(text, &consumed);
			if (consumed != text.size())
				throw std::invalid_argument("无法转换为浮点数: " + text);
			return static_cast<T>(parsed);
		}
		else
		{
			static_assert(!std::is_same_v<T, T>, "ZString::StringTo 仅支持字符串、布尔值、整数和浮点类型。");
		}
	}

	/**
	 * @brief 将字符串转换为布尔值。
	 * @param value 输入字符串。
	 * @return 非零返回 true。
	 */
	static bool StringToBool(const std::string &value)
	{
		return std::stoi(value) != 0;
	}
	/**
	 * @brief 将字符串转换为 int。
	 */
	static int StringToInt(const std::string &value)
	{
		return StringTo<int>(value);
	}

	/**
	 * @brief 将字符串转换为 long long。
	 */
	static long long StringToLongLong(const std::string &value)
	{
		return StringTo<long long>(value);
	}

	/**
	 * @brief 将字符串转换为 float。
	 */
	static float StringToFloat(const std::string &value)
	{
		return StringTo<float>(value);
	}

	/**
	 * @brief 将字符串转换为 double。
	 */
	static double StringToDouble(const std::string &value)
	{
		return StringTo<double>(value);
	}

	/**
	 * @brief 将字符串转换为 long double。
	 */
	static long double StringToLongDouble(const std::string &value)
	{
		return StringTo<long double>(value);
	}

	/**
	 * @brief 将字符串转换为指定枚举类型。
	 * @tparam E 目标枚举类型。
	 * @param value 输入字符串。
	 * @param ignoreCase 是否忽略大小写。
	 * @return 转换后的枚举值。
	 */
	template <typename E>
	static E StringToEnum(const std::string &value, bool ignoreCase = true)
	{
		static_assert(std::is_enum_v<E>, "ZString::StringToEnum 仅支持枚举类型。");

		const std::string text = Trim(value);
		auto parsed = ignoreCase ? magic_enum::enum_cast<E>(text, magic_enum::case_insensitive)
								 : magic_enum::enum_cast<E>(text);
		if (!parsed)
			throw std::invalid_argument("无法转换为枚举: " + text);

		return *parsed;
	}

	/**
	 * @brief 将整数转换为指定枚举类型。
	 * @tparam E 目标枚举类型。
	 * @param value 输入整数。
	 * @param ignoreCase 是否忽略大小写（无效参数，仅为保持接口一致性）。
	 * @return 转换后的枚举值。
	 */
	template <typename E>
	static E StringToEnum(const int &value)
	{
		static_assert(std::is_enum_v<E>, "ZString::StringToEnum 仅支持枚举类型。");

		auto parsed = magic_enum::enum_cast<E>(value);
		if (!parsed)
			throw std::invalid_argument("无法转换为枚举: " + std::to_string(value));
		return *parsed;
	}

	/**
	 * @brief 将字符串转换为 Eigen 向量。
	 * @tparam Derived 目标 Eigen 类型。
	 * @param value 输入字符串。
	 * @param rowDelimiter 行分隔符，默认 ';'，同时也识别换行。
	 * @return 转换后的 Eigen 对象。
	 */
	template <typename Derived>
	static Derived StringToEigen(const std::string &value, char rowDelimiter = ';')
	{
		const std::string text = Trim(value);
		if (text.empty())
			throw std::invalid_argument("无法将空字符串转换为 Eigen 对象。");

		std::vector<std::string> rows;
		std::string current;
		auto flushRow = [&]()
		{
			std::string row = Trim(current);
			if (!row.empty())
				rows.push_back(row);
			current.clear();
		};

		for (char ch : text)
		{
			if (ch == '\r')
				continue;

			if (ch == '\n' || ch == rowDelimiter)
				flushRow();
			else
				current.push_back(ch);
		}
		flushRow();

		auto tokenizeRow = [](const std::string &row)
		{
			std::string normalized = row;
			std::replace(normalized.begin(), normalized.end(), ',', ' ');
			std::replace(normalized.begin(), normalized.end(), '\t', ' ');

			std::istringstream stream(normalized);
			std::vector<std::string> tokens;
			std::string token;
			while (stream >> token)
				tokens.push_back(token);
			return tokens;
		};

		std::vector<std::vector<std::string>> tokenRows;
		tokenRows.reserve(rows.size());
		size_t maxCols = 0;
		size_t totalScalars = 0;
		for (const auto &row : rows)
		{
			auto tokens = tokenizeRow(row);
			if (tokens.empty())
				continue;

			maxCols = std::max(maxCols, tokens.size());
			totalScalars += tokens.size();
			tokenRows.push_back(std::move(tokens));
		}

		if (tokenRows.empty())
			throw std::invalid_argument("字符串中没有可转换的 Eigen 数据: " + text);

		using Scalar = typename Derived::Scalar;
		constexpr bool isColumnVector = (Derived::ColsAtCompileTime == 1 && Derived::RowsAtCompileTime != 1);
		constexpr bool isRowVector = (Derived::RowsAtCompileTime == 1 && Derived::ColsAtCompileTime != 1);
		constexpr bool isVectorLike = isColumnVector || isRowVector;

		if constexpr (!isVectorLike)
		{
			throw std::invalid_argument("请使用 StringToEigenMatrix(value, rowCount) 解析矩阵。");
		}

		const size_t length = totalScalars;
		if constexpr (Derived::RowsAtCompileTime != Eigen::Dynamic && isColumnVector)
		{
			if (static_cast<int>(length) != Derived::RowsAtCompileTime)
				throw std::invalid_argument("Eigen 向量长度不匹配: " + text);
		}
		if constexpr (Derived::ColsAtCompileTime != Eigen::Dynamic && isRowVector)
		{
			if (static_cast<int>(length) != Derived::ColsAtCompileTime)
				throw std::invalid_argument("Eigen 向量长度不匹配: " + text);
		}

		Derived result;
		if constexpr (Derived::RowsAtCompileTime == Eigen::Dynamic || Derived::ColsAtCompileTime == Eigen::Dynamic)
		{
			if constexpr (isColumnVector)
				result.resize(static_cast<int>(length), 1);
			else
				result.resize(1, static_cast<int>(length));
		}

		size_t index = 0;
		for (const auto &row : tokenRows)
		{
			for (const auto &token : row)
			{
				if constexpr (isColumnVector)
					result(static_cast<int>(index), 0) = StringTo<Scalar>(token);
				else
					result(0, static_cast<int>(index)) = StringTo<Scalar>(token);
				++index;
			}
		}
		return result;
	}

	/**
	 * @brief 将字符串转换为 Eigen 列向量。
	 * @tparam Scalar 向量标量类型。
	 * @param value 输入字符串。
	 * @param rowDelimiter 行分隔符，默认 ';'，同时也识别换行。
	 * @return 转换后的 Eigen 列向量。
	 */
	template <typename Scalar>
	static Eigen::Matrix<Scalar, Eigen::Dynamic, 1> StringToEigenVector(const std::string &value, char rowDelimiter = ';')
	{
		return StringToEigen<Eigen::Matrix<Scalar, Eigen::Dynamic, 1>>(value, rowDelimiter);
	}

	/**
	 * @brief 将字符串转换为 Eigen 行向量。
	 * @tparam Scalar 向量标量类型。
	 * @param value 输入字符串。
	 * @param rowDelimiter 行分隔符，默认 ';'，同时也识别换行。
	 * @return 转换后的 Eigen 行向量。
	 */
	template <typename Scalar>
	static Eigen::Matrix<Scalar, 1, Eigen::Dynamic> StringToEigenRowVector(const std::string &value, char rowDelimiter = ';')
	{
		return StringToEigen<Eigen::Matrix<Scalar, 1, Eigen::Dynamic>>(value, rowDelimiter);
	}

	/**
	 * @brief 将字符串转换为 Eigen 矩阵。
	 * @tparam Derived 目标 Eigen 类型。
	 * @param value 输入字符串。
	 * @param rowCount 矩阵行数。
	 * @param rowDelimiter 行分隔符，默认 ';'，同时也识别换行。
	 * @return 转换后的 Eigen 矩阵。
	 */
	template <typename Derived>
	static Derived StringToEigenMatrix(const std::string &value, size_t rowCount, char rowDelimiter = ';')
	{
		const std::string text = Trim(value);
		if (text.empty())
			throw std::invalid_argument("无法将空字符串转换为 Eigen 矩阵。");
		if (rowCount == 0)
			throw std::invalid_argument("转换 Eigen 矩阵时必须指定行数。");

		std::vector<std::string> rows;
		std::string current;
		auto flushRow = [&]()
		{
			std::string row = Trim(current);
			if (!row.empty())
				rows.push_back(row);
			current.clear();
		};

		for (char ch : text)
		{
			if (ch == '\r')
				continue;

			if (ch == '\n' || ch == rowDelimiter)
				flushRow();
			else
				current.push_back(ch);
		}
		flushRow();

		auto tokenizeRow = [](const std::string &row)
		{
			std::string normalized = row;
			std::replace(normalized.begin(), normalized.end(), ',', ' ');
			std::replace(normalized.begin(), normalized.end(), '\t', ' ');

			std::istringstream stream(normalized);
			std::vector<std::string> tokens;
			std::string token;
			while (stream >> token)
				tokens.push_back(token);
			return tokens;
		};

		std::vector<std::vector<std::string>> tokenRows;
		tokenRows.reserve(rows.size());
		for (const auto &row : rows)
		{
			auto tokens = tokenizeRow(row);
			if (!tokens.empty())
				tokenRows.push_back(std::move(tokens));
		}

		if (tokenRows.empty())
			throw std::invalid_argument("字符串中没有可转换的 Eigen 矩阵数据: " + text);

		using Scalar = typename Derived::Scalar;
		if constexpr (Derived::RowsAtCompileTime != Eigen::Dynamic)
		{
			if (static_cast<int>(rowCount) != Derived::RowsAtCompileTime)
				throw std::invalid_argument("Eigen 矩阵行数不匹配: " + text);
		}

		const bool hasExplicitRows = tokenRows.size() > 1;
		size_t colsCount = 0;

		if (hasExplicitRows)
		{
			if (tokenRows.size() != rowCount)
				throw std::invalid_argument("Eigen 矩阵行数不匹配: " + text);

			colsCount = tokenRows.front().size();
			if (colsCount == 0)
				throw std::invalid_argument("Eigen 矩阵列数不能为空: " + text);

			for (const auto &row : tokenRows)
			{
				if (row.size() != colsCount)
					throw std::invalid_argument("Eigen 矩阵必须是规则矩阵: " + text);
			}
		}
		else
		{
			const size_t totalScalars = tokenRows.front().size();
			if (totalScalars == 0)
				throw std::invalid_argument("Eigen 矩阵列数不能为空: " + text);
			if (totalScalars % rowCount != 0)
				throw std::invalid_argument("Eigen 矩阵元素数量无法按指定行数均分: " + text);
			colsCount = totalScalars / rowCount;
		}

		if constexpr (Derived::ColsAtCompileTime != Eigen::Dynamic)
		{
			if (static_cast<int>(colsCount) != Derived::ColsAtCompileTime)
				throw std::invalid_argument("Eigen 矩阵列数不匹配: " + text);
		}

		Derived result;
		if constexpr (Derived::RowsAtCompileTime == Eigen::Dynamic || Derived::ColsAtCompileTime == Eigen::Dynamic)
			result.resize(static_cast<int>(rowCount), static_cast<int>(colsCount));

		if (hasExplicitRows)
		{
			for (size_t r = 0; r < rowCount; ++r)
			{
				for (size_t c = 0; c < colsCount; ++c)
					result(static_cast<int>(r), static_cast<int>(c)) = StringTo<Scalar>(tokenRows[r][c]);
			}
		}
		else
		{
			size_t index = 0;
			for (size_t r = 0; r < rowCount; ++r)
			{
				for (size_t c = 0; c < colsCount; ++c)
					result(static_cast<int>(r), static_cast<int>(c)) = StringTo<Scalar>(tokenRows.front()[index++]);
			}
		}

		return result;
	}

	/**
	 * @brief 从指定文件路径读取所有行。
	 * @param path 文件路径。
	 * @return 每一行组成的字符串数组。
	 */
	static std::vector<std::string> ReadAllLines(const std::string &path)
	{
		std::vector<std::string> lines;
		const std::string absPath = ZPath::GetABSPath(path);
		std::ifstream file(absPath);
		if (!file.is_open())
			throw std::runtime_error("无法打开文件: " + absPath);

		std::string line;
		while (std::getline(file, line))
			lines.push_back(line);
		return lines;
	}

	/**
	 * @brief 将字符串数组写入指定文件路径，每个元素占一行。
	 * @param path 文件路径。
	 * @param lines 要写入的字符串数组。
	 */
	static void WriteAllLines(const std::string &path, const std::vector<std::string> &lines)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath);
		if (!file.is_open())
			return;

		for (const auto &line : lines)
			file << line << '\n';
	}

	/**
	 * @brief 移除 UTF-8 BOM。
	 * @param value 输入字符串。
	 * @return 移除 BOM 后的字符串。
	 */
	static std::string RemoveUtf8Bom(std::string value)
	{
		if (value.size() >= 3 &&
			static_cast<unsigned char>(value[0]) == 0xEF &&
			static_cast<unsigned char>(value[1]) == 0xBB &&
			static_cast<unsigned char>(value[2]) == 0xBF)
		{
			value.erase(0, 3);
		}
		return value;
	}

	/**
	 * 在字符串数组中查找所有包含给定子串的元素索引
	 * @param strs   字符串数组
	 * @param target 待查找的子串
	 * @return       所有匹配的索引（0-based）；若无匹配返回 [-1]
	 */
	std::vector<int> findSubstringIndices(const std::vector<std::string> &strs,
										  const std::string &target)
	{
		std::vector<int> indices;
		for (size_t i = 0; i < strs.size(); ++i)
		{
			if (strs[i].find(target) != std::string::npos)
			{
				indices.push_back(static_cast<int>(i));
			}
		}
		if (indices.empty())
		{
			indices.push_back(-1);
		}
		return indices;
	}
};
