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
// 该文件提供一个 YAML 解析和生成工具，支持常用数据类型的转换和格式化，适合配置文件和
// 数据交换使用。
// ──────────────────────────────────────────────────────────────────────────────



#pragma once

#include <Eigen/Dense>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "magic_enum.hpp"

#include "LocaleString.hpp"

/** @namespace yml_detail
 * @brief YAML 解析内部工具命名空间，提供字符串处理、数值解析和格式化等基础函数。
 */
namespace yml_detail
{
	/** @brief 去除字符串左侧空白字符。
	 *  @param value 原始字符串。
	 *  @return 去除左侧空白后的新字符串。
	 */
	inline std::string trimStart(const std::string &value)
	{
		auto it = std::find_if_not(value.begin(), value.end(), [](unsigned char ch)
								   { return std::isspace(ch) != 0; });
		return std::string(it, value.end());
	}

	/** @brief 去除字符串右侧空白字符。
	 *  @param value 原始字符串。
	 *  @return 去除右侧空白后的新字符串。
	 */
	inline std::string trimEnd(const std::string &value)
	{
		auto it = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch)
								   { return std::isspace(ch) != 0; });
		return std::string(value.begin(), it.base());
	}

	/** @brief 去除字符串首尾空白字符。
	 *  @param value 原始字符串。
	 *  @return 去除首尾空白后的新字符串。
	 */
	inline std::string trim(const std::string &value)
	{
		return trimEnd(trimStart(value));
	}

	/** @brief 将字符串转换为小写。
	 *  @param value 原始字符串（按值传递）。
	 *  @return 全小写的新字符串。
	 */
	inline std::string toLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
					   { return static_cast<char>(std::tolower(ch)); });
		return value;
	}

	/** @brief 判断字符串是否以指定前缀开头。
	 *  @param value 待检查的字符串。
	 *  @param prefix 前缀。
	 *  @return 若以 prefix 开头则返回 true。
	 */
	inline bool startsWith(const std::string &value, const std::string &prefix)
	{
		return value.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), value.begin());
	}

	/** @brief 统计字符串前导空格数。
	 *  @param value 待检测的字符串。
	 *  @return 前导空格数量。
	 */
	inline int leadingSpaces(const std::string &value)
	{
		int count = 0;
		for (char ch : value)
		{
			if (ch != ' ')
				break;
			++count;
		}
		return count;
	}

	/** @brief 按分隔符分割字符串。
	 *  @param value 原始字符串。
	 *  @param delimiter 分隔符。
	 *  @param removeEmpty 是否移除空片段，默认为 true。
	 *  @return 分割后的字符串向量。
	 */
	inline std::vector<std::string> split(const std::string &value, char delimiter, bool removeEmpty = true)
	{
		std::vector<std::string> result;
		std::string current;
		std::istringstream stream(value);
		while (std::getline(stream, current, delimiter))
		{
			if (!removeEmpty || !current.empty())
				result.push_back(current);
		}
		if (!removeEmpty && !value.empty() && value.back() == delimiter)
			result.emplace_back();
		return result;
	}

	/** @brief 按 '.' 分割键路径字符串。
	 *  @param key 键路径（如 "parent.child.grandchild"）。
	 *  @return 各级键名的向量，已去除空白和空片段。
	 */
	inline std::vector<std::string> splitKey(const std::string &key)
	{
		std::vector<std::string> result;
		for (const auto &part : split(key, '.', true))
		{
			std::string text = trim(part);
			if (!text.empty())
				result.push_back(text);
		}
		return result;
	}

	/** @brief 将键名向量按 '.' 拼接为键路径字符串。
	 *  @param keys 键名向量。
	 *  @param endExclusive 拼接的结束位置（不含）。
	 *  @return 拼接后的键路径，如 "parent.child"。
	 */
	inline std::string joinKey(const std::vector<std::string> &keys, size_t endExclusive)
	{
		std::ostringstream stream;
		for (size_t i = 0; i < endExclusive; ++i)
		{
			if (i != 0)
				stream << '.';
			stream << keys[i];
		}
		return stream.str();
	}

	/** @brief 将多行文本拆分为行向量。
	 *  @param text 多行文本。
	 *  @return 各行字符串的向量，自动处理 \\r\\n 换行符。
	 */
	inline std::vector<std::string> splitLines(const std::string &text)
	{
		std::vector<std::string> lines;
		std::istringstream stream(text);
		std::string line;
		while (std::getline(stream, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			lines.push_back(line);
		}
		if (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
			lines.emplace_back();
		return lines;
	}

	/** @brief 拆分 YAML 标量列表（如 "[a, b, c]" 或 "a, b, c"）。
	 *  @param value 包含列表的字符串。
	 *  @return 各标量值的字符串向量。
	 *  @note 自动去除方括号，支持空格和制表符分隔。
	 */
	inline std::string unquoteScalar(const std::string &value)
	{
		const std::string text = trim(value);
		if (text.size() < 2)
			return text;
		const char quote = text.front();
		if ((quote != '"' && quote != '\'') || text.back() != quote)
			return text;

		std::string out;
		out.reserve(text.size() - 2);
		for (size_t i = 1; i + 1 < text.size(); ++i)
		{
			const char ch = text[i];
			if (quote == '"' && ch == '\\' && i + 1 < text.size() - 1)
			{
				const char next = text[i + 1];
				if (next == '"' || next == '\\')
				{
					out.push_back(next);
					++i;
					continue;
				}
			}
			if (quote == '\'' && ch == '\'' && i + 1 < text.size() - 1 && text[i + 1] == '\'')
			{
				out.push_back('\'');
				++i;
				continue;
			}
			out.push_back(ch);
		}
		return out;
	}

	inline std::vector<std::string> splitScalarList(const std::string &value)
	{
		std::vector<std::string> result;
		std::string token;
		bool inQuote = false;
		char quote = '"';
		bool bracketed = false;
		bool hasComma = false;

		for (size_t i = 0; i < value.size(); ++i)
		{
			const char ch = value[i];
			if (inQuote)
			{
				if (ch == '\\' && quote == '"' && i + 1 < value.size())
					++i;
				else if (ch == quote)
					inQuote = false;
				continue;
			}
			if (ch == '"' || ch == '\'')
			{
				inQuote = true;
				quote = ch;
				continue;
			}
			if (ch == ',')
			{
				hasComma = true;
				break;
			}
		}

		inQuote = false;
		quote = '"';

		auto flush = [&]() {
			std::string text = trim(token);
			if (!text.empty())
				result.push_back(unquoteScalar(text));
			token.clear();
		};

		for (size_t i = 0; i < value.size(); ++i)
		{
			const char ch = value[i];
			if (ch == '[' || ch == ']')
			{
				bracketed = true;
				if (!inQuote)
					continue;
			}

			if (inQuote)
			{
				token.push_back(ch);
				if (ch == '\\' && quote == '"' && i + 1 < value.size())
					token.push_back(value[++i]);
				else if (ch == quote)
					inQuote = false;
				continue;
			}

			if (ch == '"' || ch == '\'')
			{
				inQuote = true;
				quote = ch;
				token.push_back(ch);
				continue;
			}

			if (ch == ',' || ((!bracketed || !hasComma) && std::isspace(static_cast<unsigned char>(ch))))
			{
				flush();
				continue;
			}
			token.push_back(ch == '\t' ? ' ' : ch);
		}
		flush();
		return result;
	}

	/** @brief 拆分 YAML 矩阵行数据。
	 *  @param value 包含矩阵数据的字符串。
	 *  @return 各行数据的字符串向量。
	 *  @note 优先按行解析，若为空则按分号 ';' 拆分。
	 */
	inline std::vector<std::string> splitMatrixRows(const std::string &value)
	{
		std::vector<std::string> rows;
		for (const auto &line : splitLines(value))
		{
			std::string row = trim(line);
			if (row.empty())
				continue;
			if (row.front() == '-')
			{
				row.erase(row.begin());
				row = trim(row);
			}
			if (!row.empty())
				rows.push_back(row);
		}

		if (!rows.empty())
			return rows;

		for (const auto &row : split(value, ';', true))
		{
			std::string text = trim(row);
			if (!text.empty())
				rows.push_back(text);
		}
		return rows;
	}

	/** @brief 将字符串解析为数值类型。
	 *  @tparam T 目标数值类型。
	 *  @param value 待解析的字符串。
	 *  @param typeName 类型名称（用于错误消息）。
	 *  @return 解析后的数值。
	 *  @throws std::invalid_argument 解析失败时抛出。
	 */
	template <typename T>
	inline T parseNumber(const std::string &value, const std::string &typeName)
	{
		const std::string text = trim(value);
		if (text.empty())
			return T{};

		std::istringstream stream(text);
		stream.imbue(std::locale::classic());
		T result{};
		stream >> result;
		if (!stream || !stream.eof())
			throw std::invalid_argument("Cannot convert yaml value to " + typeName + ": " + value);
		return result;
	}

	/** @brief 将字符串解析为布尔值。
	 *  @param value 待解析的字符串。
	 *  @param defaultValue 无法识别时的默认值。
	 *  @return 解析后的布尔值，支持 "true"/"false"/"1"/"0"（不区分大小写）。
	 */
	inline bool parseBool(const std::string &value, bool defaultValue = false)
	{
		const std::string text = toLower(trim(value));
		if (text == "true")
			return true;
		if (text == "false")
			return false;
		if (text == "1")
			return true;
		if (text == "0")
			return false;
		return defaultValue;
	}

	/** @brief 将标量值转换为字符串表示。
	 *  @tparam T 标量类型，支持 bool、enum、整数、浮点数等。
	 *  @param value 待转换的标量值。
	 *  @return 格式化后的字符串。
	 *  @note 枚举类型使用 magic_enum 获取名称，浮点数使用最大精度输出。
	 */
	inline std::string quoteScalarString(const std::string &value)
	{
		std::ostringstream stream;
		stream << '"';
		for (const char ch : value)
		{
			if (ch == '"' || ch == '\\')
				stream << '\\';
			stream << ch;
		}
		stream << '"';
		return stream.str();
	}

	template <typename T>
	inline std::string scalarToString(T value)
	{
		if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
		{
			return quoteScalarString(value);
		}
		else if constexpr (std::is_same_v<std::decay_t<T>, bool>)
		{
			return value ? "True" : "False";
		}
		else if constexpr (std::is_enum_v<std::decay_t<T>>)
		{
			auto name = magic_enum::enum_name(value);
			return name.empty() ? std::to_string(static_cast<std::underlying_type_t<std::decay_t<T>>>(value))
								: std::string(name);
		}
		else if constexpr (std::is_integral_v<std::decay_t<T>>)
		{
			return std::to_string(value);
		}
		else if constexpr (std::is_floating_point_v<std::decay_t<T>>)
		{
			std::ostringstream stream;
			stream.imbue(std::locale::classic());
			stream << std::setprecision(std::numeric_limits<std::decay_t<T>>::max_digits10) << value;
			return stream.str();
		}
		else
		{
			std::ostringstream stream;
			stream << value;
			return stream.str();
		}
	}

	/** @brief 将 vector 转换为 YAML 数组字符串 "[ e1 , e2 , ... ]"。
	 *  @tparam T 元素类型。
	 *  @param values 元素向量。
	 *  @return 格式化后的数组字符串。
	 */
	template <typename T>
	inline std::string arrayToString(const std::vector<T> &values)
	{
		std::ostringstream stream;
		stream << "[ ";
		for (size_t i = 0; i < values.size(); ++i)
		{
			if (i != 0)
				stream << " , ";
			stream << scalarToString(values[i]);
		}
		stream << " ]";
		return stream.str();
	}

	/** @brief 将 std::array 转换为 YAML 数组字符串。
	 *  @tparam T 元素类型。
	 *  @tparam N 数组大小。
	 *  @param values 元素数组。
	 *  @return 格式化后的数组字符串。
	 */
	template <typename T, size_t N>
	inline std::string arrayToString(const std::array<T, N> &values)
	{
		std::ostringstream stream;
		stream << "[ ";
		for (size_t i = 0; i < values.size(); ++i)
		{
			if (i != 0)
				stream << " , ";
			stream << scalarToString(values[i]);
		}
		stream << " ]";
		return stream.str();
	}

	/** @brief 获取当前时间的格式化字符串。
	 *  @return "YYYY-MM-DD HH:MM:SS" 格式的当前时间字符串。
	 */
	inline std::string nowString()
	{
		const auto now = std::chrono::system_clock::now();
		const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
		std::tm tm{};
#if defined(_WIN32)
		localtime_s(&tm, &nowTime);
#else
		localtime_r(&nowTime, &tm);
#endif
		std::ostringstream stream;
		stream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
		return stream.str();
	}
}

/** @class YMLConvertToObjectiveExtensions
 * @brief YAML 值类型转换扩展类，提供将 YAML 字符串转换为各种 C++/Eigen 类型的静态方法。
 *
 * 该类同时维护 @c ConvertValueType 和 @c readlist 两个静态向量，用于记录类型转换和读取历史。
 */
class YMLConvertToObjectiveExtensions
{
public:
	/** @brief 已转换类型名称列表，每次转换追加一条记录。 */
	inline static std::vector<std::string> ConvertValueType{};
	/** @brief 已读取的键名列表，记录读取历史。 */
	inline static std::vector<std::string> readlist{};

	/** @brief 将 YAML 字符串转换为 bool。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数，未使用）。
	 *  @param moren 默认值，无法识别时返回此值。
	 *  @return 转换后的布尔值。
	 *  @code bool b = YMLConvertToObjectiveExtensions::YmlToBool("true"); @endcode
	 */
	static bool YmlToBool(const std::string &yml, const std::string &name = "", bool moren = false)
	{
		(void)name;
		const bool result = yml_detail::parseBool(yml, moren);
		ConvertValueType.emplace_back("bool");
		return result;
	}

	/** @brief 将 YAML 字符串转换为 bool 数组。
	 *  @param yml YAML 值字符串（如 "[true, false, true]"）。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 转换后的 std::vector\<bool\>。
	 *  @code auto arr = YMLConvertToObjectiveExtensions::YmlToBoolArray("[true, false]"); @endcode
	 */
	static std::vector<bool> YmlToBoolArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		std::vector<bool> result;
		for (const auto &token : yml_detail::splitScalarList(yml))
			result.push_back(yml_detail::parseBool(token));
		ConvertValueType.emplace_back("std::vector<bool>");
		return result;
	}

	/** @brief 将 YAML 字符串转换为 int。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 转换后的整数值。
	 *  @throws std::invalid_argument 解析失败时抛出。
	 *  @code int i = YMLConvertToObjectiveExtensions::YmlToInt("42"); @endcode
	 */
	static int YmlToInt(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		const int result = yml_detail::parseNumber<int>(yml, "int");
		ConvertValueType.emplace_back("int");
		return result;
	}

	/** @brief 将 YAML 字符串转换为 int 数组。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 转换后的 std::vector\<int\>。
	 *  @code auto arr = YMLConvertToObjectiveExtensions::YmlToIntArray("[1, 2, 3]"); @endcode
	 */
	static std::vector<int> YmlToIntArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		std::vector<int> result;
		for (const auto &token : yml_detail::splitScalarList(yml))
			result.push_back(yml_detail::parseNumber<int>(token, "int"));
		ConvertValueType.emplace_back("std::vector<int>");
		return result;
	}

	/** @brief 将 YAML 字符串转换为 double。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 转换后的双精度浮点数。
	 *  @throws std::invalid_argument 解析失败时抛出。
	 *  @code double d = YMLConvertToObjectiveExtensions::YmlToDouble("3.14"); @endcode
	 */
	static double YmlToDouble(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		const double result = yml_detail::parseNumber<double>(yml, "double");
		ConvertValueType.emplace_back("double");
		return result;
	}

	/** @brief 将 YAML 字符串转换为 double 数组。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 转换后的 std::vector\<double\>。
	 *  @code auto arr = YMLConvertToObjectiveExtensions::YmlToDoubleArray("[1.0, 2.0]"); @endcode
	 */
	static std::vector<double> YmlToDoubleArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		std::vector<double> result;
		for (const auto &token : yml_detail::splitScalarList(yml))
			result.push_back(yml_detail::parseNumber<double>(token, "double"));
		ConvertValueType.emplace_back("std::vector<double>");
		return result;
	}

	/** @brief 将 YAML 字符串转换为 float。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 转换后的单精度浮点数（内部以 double 计算）。
	 *  @throws std::invalid_argument 解析失败时抛出。
	 *  @code float f = YMLConvertToObjectiveExtensions::YmlToFloat("1.5"); @endcode
	 */
	static double YmlToFloat(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		const float result = yml_detail::parseNumber<float>(yml, "float");
		ConvertValueType.emplace_back("float");
		return result;
	}

	/** @brief 将 YAML 字符串转换为 float 数组。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 转换后的 std::vector\<float\>。
	 *  @code auto arr = YMLConvertToObjectiveExtensions::YmlToFloatArray("[1.0f, 2.0f]"); @endcode
	 */
	static std::vector<float> YmlToFloatArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		std::vector<float> result;
		for (const auto &token : yml_detail::splitScalarList(yml))
			result.push_back(yml_detail::parseNumber<float>(token, "float"));
		ConvertValueType.emplace_back("std::vector<float>");
		return result;
	}

	/** @brief 将 YAML 字符串转换为 Eigen::VectorXd。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 转换后的 Eigen 列向量。
	 *  @code Eigen::VectorXd v = YMLConvertToObjectiveExtensions::YmlToVector("[1.0, 2.0, 3.0]"); @endcode
	 */
	static Eigen::VectorXd YmlToVector(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		const auto values = YmlToDoubleArray(yml);
		Eigen::VectorXd result(static_cast<Eigen::Index>(values.size()));
		for (Eigen::Index i = 0; i < result.size(); ++i)
			result(i) = values[static_cast<size_t>(i)];
		ConvertValueType.emplace_back("Eigen::VectorXd");
		return result;
	}

	/** @brief 将 YAML 字符串原样返回（无转换）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 原始字符串，空字符串记录为 "Null"。
	 *  @code std::string s = YMLConvertToObjectiveExtensions::YmlToString("hello"); @endcode
	 */
	static std::string YmlToString(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		ConvertValueType.emplace_back(yml.empty() ? "Null" : "std::string");
		return yml;
	}

	/** @brief 将 YAML 字符串转换为字符串数组。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 各标量字符串的向量。
	 *  @code auto arr = YMLConvertToObjectiveExtensions::YmlToStringArray("[a, b, c]"); @endcode
	 */
	static std::vector<std::string> YmlToStringArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		std::vector<std::string> result;
		for (const auto &token : yml_detail::splitScalarList(yml))
			result.push_back(token);
		ConvertValueType.emplace_back("std::vector<std::string>");
		return result;
	}

	/** @struct DoubleArray2D
	 *  @brief 二维 double 数组的中间结果，包含数据和行列数。
	 */
	struct DoubleArray2D
	{
		std::vector<std::vector<double>> data;
		int RowCount = 0;
		int ColumnCount = 0;
	};

	/** @brief 将 YAML 字符串转换为二维 double 数组。
	 *  @param yml YAML 值字符串（支持多行矩阵格式）。
	 *  @param name 键名（保留参数，未使用）。
	 *  @return 包含二维数组及行列信息的 DoubleArray2D 结构体。
	 *  @throws std::invalid_argument 各行列数不一致时抛出。
	 *  @code auto mat2d = YMLConvertToObjectiveExtensions::YmlTo2DDoubleArray("- [1, 2]\n- [3, 4]"); @endcode
	 */
	static DoubleArray2D YmlTo2DDoubleArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		DoubleArray2D result;
		const auto rows = yml_detail::splitMatrixRows(yml);
		result.RowCount = static_cast<int>(rows.size());
		if (rows.empty())
		{
			ConvertValueType.emplace_back("std::vector<std::vector<double>>");
			return result;
		}

		for (const auto &rowText : rows)
		{
			std::vector<double> row;
			for (const auto &token : yml_detail::splitScalarList(rowText))
				row.push_back(yml_detail::parseNumber<double>(token, "double"));

			if (result.ColumnCount == 0)
				result.ColumnCount = static_cast<int>(row.size());
			else if (result.ColumnCount != static_cast<int>(row.size()))
				throw std::invalid_argument("Yaml matrix rows must have equal column counts.");

			result.data.push_back(std::move(row));
		}

		ConvertValueType.emplace_back("std::vector<std::vector<double>>");
		return result;
	}

	/** @struct MatrixResult
	 *  @brief Eigen 矩阵的中间结果，包含 MatrixXd 数据和行列数。
	 */
	struct MatrixResult
	{
		Eigen::MatrixXd data;
		int RowCount = 0;
		int ColumnCount = 0;
	};

	/** @brief 将 YAML 字符串转换为 Eigen::MatrixXd。
	 *  @param yml YAML 值字符串（支持多行矩阵格式）。
	 *  @param name 键名（保留参数，未使用）。
	 *  @param add 是否向 ConvertValueType 追加记录，默认为 true。
	 *  @return 包含 Eigen 矩阵及行列信息的 MatrixResult 结构体。
	 *  @code auto mat = YMLConvertToObjectiveExtensions::YmlToMatrix("- [1, 2]\n- [3, 4]"); @endcode
	 */
	static MatrixResult YmlToMatrix(const std::string &yml, const std::string &name = "", bool add = true)
	{
		(void)name;
		const auto array = YmlTo2DDoubleArray(yml);
		MatrixResult result;
		result.RowCount = array.RowCount;
		result.ColumnCount = array.ColumnCount;
		result.data.resize(result.RowCount, result.ColumnCount);
		for (int r = 0; r < result.RowCount; ++r)
		{
			for (int c = 0; c < result.ColumnCount; ++c)
				result.data(r, c) = array.data[static_cast<size_t>(r)][static_cast<size_t>(c)];
		}
		if (add)
			ConvertValueType.emplace_back("Eigen::MatrixXd");
		return result;
	}

	/** @brief 将 YAML 字符串转换为枚举值。
	 *  @tparam E 目标枚举类型。
	 *  @param yml YAML 值字符串。
	 *  @param ignoreCase 是否忽略大小写，默认为 true。
	 *  @return 转换后的枚举值。
	 *  @throws std::invalid_argument 无法匹配时抛出。
	 *  @code MyEnum e = YMLConvertToObjectiveExtensions::YmlToEnum<MyEnum>("Value1"); @endcode
	 */
	template <typename E>
	static E YmlToEnum(const std::string &yml, bool ignoreCase = true)
	{
		static_assert(std::is_enum_v<E>, "YmlToEnum requires an enum type.");
		const std::string text = yml_detail::trim(yml);
		auto parsed = ignoreCase ? magic_enum::enum_cast<E>(text, magic_enum::case_insensitive)
								 : magic_enum::enum_cast<E>(text);
		if (!parsed)
			throw std::invalid_argument("Cannot convert yaml value to enum: " + yml);
		ConvertValueType.emplace_back("enum");
		return *parsed;
	}
};

/** @class YML
 * @brief YAML 解析与生成类，支持层级节点管理、类型转换及文件读写。
 *
 * 提供从文件或字符串解析 YAML、增删改查节点、类型转换（标量/数组/矩阵/枚举）、
 * 以及格式化保存等功能。
 */
class YML
{
public:
	/** @brief YML 版本号常量。 */
	inline static constexpr const char *ymlversion = "2.0.016";

	/** @struct Node
	 *  @brief YAML 树节点，包含名称、可选值、父节点指针及缩进层级信息。
	 */
	struct Node
	{
		/** @brief 节点名称（键名）。 */
		std::string name;
		/** @brief 节点值（可选，叶子节点可能有值，中间节点通常为空）。 */
		std::optional<std::string> value;
		/** @brief 父节点智能指针，根节点为空。 */
		std::shared_ptr<Node> parent;
		/** @brief 缩进空格数。 */
		int space = 0;
		/** @brief 层级深度（从根节点计数为 0）。 */
		int tier = 0;

		/** @brief 深拷贝当前节点及其父节点链。
		 *  @return 克隆后的 Node 对象。
		 *  @note 仅递归克隆 parent 链，不涉及子节点。
		 */
		Node Clone() const
		{
			Node node;
			node.name = name;
			node.value = value;
			node.space = space;
			node.tier = tier;
			if (parent)
				node.parent = std::make_shared<Node>(parent->Clone());
			return node;
		}
	};

	using NodePtr = std::shared_ptr<Node>;

	std::vector<NodePtr> nodeList;

	/** @brief 默认构造函数，创建空的 YML 对象。 */
	YML() = default;

	/** @brief 从文件路径构造 YML 对象并加载解析。
	 *  @param filePath YAML 文件路径。
	 *  @param addMetadata 是否自动添加元数据节点，默认为 true。
	 */
	explicit YML(const std::string &filePath, bool addMetadata = true)
	{
		Load(filePath, addMetadata);
	}

	/** @brief 深拷贝当前 YML 对象。
	 *  @return 克隆后的 YML 对象，包含完整节点树的独立副本。
	 */
	YML Clone() const
	{
		YML copy;
		copy.path = path;
		copy.lines = lines;
		copy.tier = tier;

		std::unordered_map<const Node *, NodePtr> mapping;
		copy.nodeList.reserve(nodeList.size());
		for (const auto &node : nodeList)
		{
			auto cloned = std::make_shared<Node>();
			cloned->name = node->name;
			cloned->value = node->value;
			cloned->space = node->space;
			cloned->tier = node->tier;
			mapping[node.get()] = cloned;
			copy.nodeList.push_back(cloned);
		}

		for (size_t i = 0; i < nodeList.size(); ++i)
		{
			if (nodeList[i]->parent)
				copy.nodeList[i]->parent = mapping[nodeList[i]->parent.get()];
		}
		return copy;
	}

	/** @brief 获取当前 YAML 文件的路径。
	 *  @return 文件路径的常量引用。
	 */
	const std::string &Path() const
	{
		return path;
	}

	/** @brief 从文件加载并解析 YAML。
	 *  @param filePath YAML 文件路径。
	 *  @param addMetadata 是否添加元数据节点，默认为 true。
	 *  @throws std::runtime_error 文件无法打开时抛出。
	 */
	void Load(const std::string &filePath, bool addMetadata = true)
	{
		path = filePath;
		lines.clear();

		if (std::filesystem::is_regular_file(std::filesystem::path(filePath)))
		{
			std::ifstream file(filePath);
			if (!file.is_open())
				throw std::runtime_error(std::string(L_YAML_CannotOpen) + filePath);

			std::string line;
			while (std::getline(file, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				lines.push_back(line);
			}
		}
		else
		{
			lines.push_back("# Yaml file will be created by YML.");
		}

		parseLines(lines);
		if (addMetadata)
			ensureMetadata();
		formatting();
	}

	/** @brief 从字符串解析 YAML。
	 *  @param text YAML 文本内容。
	 *  @param addMetadata 是否添加元数据节点，默认为 false。
	 *  @return 解析后的 YML 对象。
	 */
	static YML Parse(const std::string &text, bool addMetadata = false)
	{
		YML yml;
		yml.lines = yml_detail::splitLines(text);
		yml.parseLines(yml.lines);
		if (addMetadata)
			yml.ensureMetadata();
		yml.formatting();
		return yml;
	}

	/** @brief 修改指定键的值。
	 *  @param key 键路径（如 "parent.child"）。
	 *  @param value 新值字符串。
	 */
	void modify(const std::string &key, const std::string &value)
	{
		modifyOptional(key, std::optional<std::string>(value));
	}

	/** @brief 将指定键的值设为空（std::nullopt）。
	 *  @param key 键路径。
	 */
	void modify(const std::string &key, std::nullopt_t)
	{
		modifyOptional(key, std::nullopt);
	}

	/** @brief 修改指定键的可选值（底层实现）。
	 *  @param key 键路径。
	 *  @param value 新值（std::optional\<std::string\>），为 std::nullopt 表示清空。
	 */
	void modifyOptional(const std::string &key, std::optional<std::string> value)
	{
		Node *node = findNodeByKey(key);
		if (node != nullptr)
			node->value = std::move(value);
	}

	/** @brief 读取指定键的值（找不到或值为空时返回空字符串）。
	 *  @param key 键路径。
	 *  @return 节点的值字符串，若节点不存在或无值则返回空字符串。
	 */
	std::string read(const std::string &key) const
	{
		YMLConvertToObjectiveExtensions::readlist.push_back(key);
		const Node *node = findNodeByKey(key);
		if (node == nullptr || !node->value)
			return {};
		return *node->value;
	}

	/** @brief 读取指定键的可选值。
	 *  @param key 键路径。
	 *  @return 节点的 std::optional\<std::string\> 值，节点不存在返回 std::nullopt。
	 */
	std::optional<std::string> readOptional(const std::string &key) const
	{
		YMLConvertToObjectiveExtensions::readlist.push_back(key);
		const Node *node = findNodeByKey(key);
		if (node == nullptr)
			return std::nullopt;
		return node->value;
	}

	/** @brief 按键路径查找节点（可变版本）。
	 *  @param key 键路径。
	 *  @return 节点指针，找不到返回 nullptr。
	 */
	Node *findNodeByKey(const std::string &key)
	{
		return findNodePtrByKey(key).get();
	}

	/** @brief 按键路径查找节点（const 版本）。
	 *  @param key 键路径。
	 *  @return 常量节点指针，找不到返回 nullptr。
	 */
	const Node *findNodeByKey(const std::string &key) const
	{
		return findNodePtrByKey(key).get();
	}

	/** @brief 检查指定键的节点是否存在。
	 *  @param key 键路径。
	 *  @return 节点存在时返回 true。
	 */
	bool ChickfindNodeByKey(const std::string &key) const
	{
		return findNodeByKey(key) != nullptr;
	}

	/** @brief 检查指定键的节点是否存在（CheckFindNodeByKey 拼写别名）。
	 *  @param key 键路径。
	 *  @return 节点存在时返回 true。
	 */
	bool CheckFindNodeByKey(const std::string &key) const
	{
		return ChickfindNodeByKey(key);
	}

	/** @brief 获取节点的完整键路径（递归到根节点）。
	 *  @param node 节点裸指针，为 nullptr 返回空字符串。
	 *  @return 如 "parent.child.grandchild" 格式的键路径。
	 */
	std::string GetNodeKey(const Node *node) const
	{
		if (node == nullptr)
			return {};
		if (!node->parent)
			return node->name;
		return GetNodeKey(node->parent.get()) + "." + node->name;
	}

	/** @brief 获取节点的完整键路径（智能指针重载）。
	 *  @param node 节点智能指针。
	 *  @return 如 "parent.child" 格式的键路径。
	 */
	std::string GetNodeKey(const NodePtr &node) const
	{
		return GetNodeKey(node.get());
	}

	/** @brief 合并另一个 YML 对象的节点到当前对象。
	 *  @param yaml 源 YML 对象。
	 *  @note 已存在的键将被覆盖，不存在的键将新建。
	 */
	void AddYAML(const YML &yaml)
	{
		for (const auto &node : yaml.nodeList)
		{
			const std::string key = yaml.GetNodeKey(node);
			if (ChickfindNodeByKey(key))
			{
				modifyOptional(key, node->value);
			}
			else if (node->parent)
			{
				const std::string parentKey = yaml.GetNodeKey(node->parent);
				if (ChickfindNodeByKey(parentKey))
					addNodeUnderParent(parentKey, node->name, node->value);
				else
					addNodeOptional(key, node->value);
			}
			else
			{
				addNodeUnderParent({}, node->name, node->value);
			}
		}
		formatting();
	}

	/** @brief 通过键路径添加或覆盖节点。
	 *  @param key 键路径，中间节点不存在时自动创建。
	 *  @param value 节点值。
	 */
	void AddNode(const std::string &key, const std::string &value)
	{
		addNodeOptional(key, std::optional<std::string>(value));
	}

	/** @brief 通过键路径添加空值节点。
	 *  @param key 键路径。
	 */
	void AddNode(const std::string &key, std::nullopt_t)
	{
		addNodeOptional(key, std::nullopt);
	}

	/** @brief 在指定父节点下添加子节点。
	 *  @param parentKey 父节点键路径。
	 *  @param nodeName 子节点名称。
	 *  @param value 子节点值。
	 */
	void AddNode(const std::string &parentKey, const std::string &nodeName, const std::string &value)
	{
		addNodeUnderParent(parentKey, nodeName, std::optional<std::string>(value));
	}

	/** @brief 在指定父节点下添加空值子节点。
	 *  @param parentKey 父节点键路径。
	 *  @param nodeName 子节点名称。
	 */
	void AddNode(const std::string &parentKey, const std::string &nodeName, std::nullopt_t)
	{
		addNodeUnderParent(parentKey, nodeName, std::nullopt);
	}

private:
	/** @brief 通过键路径添加节点（内部实现）。
	 *  @param key 键路径，中间节点缺失时自动创建。
	 *  @param value 节点值（可选）。
	 */
	void addNodeOptional(const std::string &key, std::optional<std::string> value)
	{
		auto keys = yml_detail::splitKey(key);
		if (keys.empty())
			return;

		if (ChickfindNodeByKey(key))
		{
			modifyOptional(key, std::move(value));
			return;
		}

		for (size_t i = 0; i < keys.size(); ++i)
		{
			const std::string currentKey = yml_detail::joinKey(keys, i + 1);
			if (ChickfindNodeByKey(currentKey))
				continue;

			const std::string parentKey = i == 0 ? std::string{} : yml_detail::joinKey(keys, i);
			const bool isLeaf = i + 1 == keys.size();
			addNodeUnderParent(parentKey, keys[i], isLeaf ? value : std::optional<std::string>{});
		}
	}

	/** @brief 在指定父节点下添加子节点（内部实现）。
	 *  @param parentKey 父节点键路径（空表示根节点）。
	 *  @param nodeName 子节点名称。
	 *  @param value 子节点值（可选）。
	 *  @throws std::invalid_argument 父节点不存在时抛出。
	 */
	void addNodeUnderParent(const std::string &parentKey, const std::string &nodeName, std::optional<std::string> value)
	{
		if (yml_detail::trim(nodeName).empty())
			return;

		if (yml_detail::trim(parentKey).empty())
		{
			auto node = std::make_shared<Node>();
			node->name = yml_detail::trim(nodeName);
			node->value = std::move(value);
			node->space = 0;
			node->tier = 0;
			nodeList.push_back(std::move(node));
			return;
		}

		auto parentNode = findNodePtrByKey(parentKey);
		if (!parentNode)
			throw std::invalid_argument("Cannot add yaml node because parent was not found: " + parentKey);

		auto node = std::make_shared<Node>();
		node->name = yml_detail::trim(nodeName);
		node->value = std::move(value);
		node->space = parentNode->space + 2;
		node->tier = parentNode->tier + 1;
		node->parent = parentNode;
		nodeList.push_back(std::move(node));
	}

public:
	/** @brief 删除指定节点及其所有子孙节点。
	 *  @param key 要删除的节点键路径，不存在则无操作。
	 */
	void DeleteNode(const std::string &key)
	{
		auto node = findNodePtrByKey(key);
		if (!node)
			return;

		std::unordered_set<Node *> removeSet;
		collectChildren(node, removeSet);
		removeSet.insert(node.get());

		nodeList.erase(std::remove_if(nodeList.begin(), nodeList.end(), [&](const NodePtr &item)
									  { return removeSet.count(item.get()) != 0; }),
					   nodeList.end());
		formatting();
	}

	/** @brief 查找指定节点的所有直接子节点（可变版本）。
	 *  @param key 父节点键路径。
	 *  @return 子节点指针向量。
	 */
	std::vector<Node *> FindChildren(const std::string &key)
	{
		std::vector<Node *> result;
		auto parentNode = findNodePtrByKey(key);
		if (!parentNode)
			return result;

		for (const auto &node : nodeList)
		{
			if (node->parent == parentNode)
				result.push_back(node.get());
		}
		return result;
	}

	/** @brief 查找指定节点的所有直接子节点（const 版本）。
	 *  @param key 父节点键路径。
	 *  @return 常量子节点指针向量。
	 */
	std::vector<const Node *> FindChildren(const std::string &key) const
	{
		std::vector<const Node *> result;
		auto parentNode = findNodePtrByKey(key);
		if (!parentNode)
			return result;

		for (const auto &node : nodeList)
		{
			if (node->parent == parentNode)
				result.push_back(node.get());
		}
		return result;
	}

	/** @brief 保存 YAML 到文件。
	 *  @param savepath 目标路径，为空则使用加载时的路径。
	 *  @param format 是否在保存前执行格式化，默认为 true。
	 *  @throws std::runtime_error 路径为空或文件无法写入时抛出。
	 */
	void save(const std::string &savepath = std::string{}, bool format = true)
	{
		std::string target = savepath.empty() ? path : savepath;
		if (target.empty())
			throw std::runtime_error(L_YAML_CannotSaveEmpty);

		ensureMetadata();
		if (format)
			formatting();

		const std::filesystem::path filePath(target);
		const auto parentPath = filePath.parent_path();
		if (!parentPath.empty())
			std::filesystem::create_directories(parentPath);

		std::ofstream file(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file.is_open())
			throw std::runtime_error(std::string(L_YAML_CannotWrite) + target);

		for (const auto &node : nodeList)
		{
			file << std::string(static_cast<size_t>(node->tier) * 2, ' ') << node->name << ": ";
			if (node->value)
				file << *node->value;
			file << '\n';
		}
	}

	/** @brief 将 YAML 字符串转换为 bool（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @param moren 默认值。
	 *  @return 转换后的布尔值。
	 *  @code bool b = YML::YmlToBool("true"); @endcode
	 */
	static bool YmlToBool(const std::string &yml, const std::string &name = "", bool moren = false)
	{
		return YMLConvertToObjectiveExtensions::YmlToBool(yml, name, moren);
	}

	/** @brief 将 YAML 字符串转换为 bool 数组（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @return 转换后的 std::vector\<bool\>。
	 *  @code auto arr = YML::YmlToBoolArray("[true, false]"); @endcode
	 */
	static std::vector<bool> YmlToBoolArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToBoolArray(yml, name);
	}

	/** @brief 将 YAML 字符串转换为 int（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @return 转换后的整数值。
	 *  @code int i = YML::YmlToInt("42"); @endcode
	 */
	static int YmlToInt(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToInt(yml, name);
	}

	/** @brief 将 YAML 字符串转换为 int 数组（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @return 转换后的 std::vector\<int\>。
	 *  @code auto arr = YML::YmlToIntArray("[1, 2, 3]"); @endcode
	 */
	static std::vector<int> YmlToIntArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToIntArray(yml, name);
	}

	/** @brief 将 YAML 字符串转换为 double（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @return 转换后的双精度浮点数。
	 *  @code double d = YML::YmlToDouble("3.14"); @endcode
	 */
	static double YmlToDouble(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToDouble(yml, name);
	}

	/** @brief 将 YAML 字符串转换为 double 数组（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @return 转换后的 std::vector\<double\>。
	 *  @code auto arr = YML::YmlToDoubleArray("[1.0, 2.0]"); @endcode
	 */
	static std::vector<double> YmlToDoubleArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToDoubleArray(yml, name);
	}

	/** @brief 将 YAML 字符串转换为 float（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @return 转换后的单精度浮点数。
	 *  @code float f = YML::YmlToFloat("1.5"); @endcode
	 */
	static double YmlToFloat(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToFloat(yml, name);
	}

	/** @brief 将 YAML 字符串转换为 float 数组（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @return 转换后的 std::vector\<float\>。
	 *  @code auto arr = YML::YmlToFloatArray("[1.0, 2.0]"); @endcode
	 */
	static std::vector<float> YmlToFloatArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToFloatArray(yml, name);
	}

	/** @brief 将 YAML 字符串转换为 Eigen::VectorXd（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @return 转换后的 Eigen 列向量。
	 *  @code Eigen::VectorXd v = YML::YmlToVector("[1.0, 2.0]"); @endcode
	 */
	static Eigen::VectorXd YmlToVector(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToVector(yml, name);
	}

	/** @brief 将 YAML 字符串原样返回（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @return 原始字符串。
	 *  @code std::string s = YML::YmlToString("hello"); @endcode
	 */
	static std::string YmlToString(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToString(yml, name);
	}

	/** @brief 将 YAML 字符串转换为字符串数组（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @return 转换后的 std::vector\<std::string\>。
	 *  @code auto arr = YML::YmlToStringArray("[a, b]"); @endcode
	 */
	static std::vector<std::string> YmlToStringArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToStringArray(yml, name);
	}

	/** @brief 将 YAML 字符串转换为二维 double 数组（静态便捷方法）。
	 *  @param yml YAML 值字符串（支持多行矩阵格式）。
	 *  @param name 键名（保留参数）。
	 *  @return 包含二维数组及行列信息的 DoubleArray2D 结构体。
	 *  @code auto mat2d = YML::YmlTo2DDoubleArray("- [1, 2]\n- [3, 4]"); @endcode
	 */
	static YMLConvertToObjectiveExtensions::DoubleArray2D YmlTo2DDoubleArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlTo2DDoubleArray(yml, name);
	}

	/** @brief 将 YAML 字符串转换为 Eigen::MatrixXd（静态便捷方法）。
	 *  @param yml YAML 值字符串。
	 *  @param name 键名（保留参数）。
	 *  @param add 是否向 ConvertValueType 追加记录，默认为 true。
	 *  @return 包含 Eigen 矩阵及行列信息的 MatrixResult 结构体。
	 *  @code auto mat = YML::YmlToMatrix("- [1, 2]\n- [3, 4]"); @endcode
	 */
	static YMLConvertToObjectiveExtensions::MatrixResult YmlToMatrix(const std::string &yml, const std::string &name = "", bool add = true)
	{
		return YMLConvertToObjectiveExtensions::YmlToMatrix(yml, name, add);
	}

	/** @brief 将 YAML 字符串转换为枚举值（模板静态便捷方法）。
	 *  @tparam E 目标枚举类型。
	 *  @param yml YAML 值字符串。
	 *  @param ignoreCase 是否忽略大小写，默认为 true。
	 *  @return 转换后的枚举值。
	 *  @throws std::invalid_argument 无法匹配时抛出。
	 *  @code MyEnum e = YML::YmlToEnum<MyEnum>("Value1"); @endcode
	 */
	template <typename E>
	static E YmlToEnum(const std::string &yml, bool ignoreCase = true)
	{
		return YMLConvertToObjectiveExtensions::YmlToEnum<E>(yml, ignoreCase);
	}

	/** @brief 将 std::string 转换为 YAML 值字符串。
	 *  @param value 字符串值。
	 *  @param level 缩进层级（保留参数，未使用）。
	 *  @return 去除首尾空白后的字符串。
	 */
	static std::string ToYmlValueString(const std::string &value, int level = 0)
	{
		(void)level;
		return yml_detail::trim(value);
	}

	/** @brief 将 C 字符串转换为 YAML 值字符串。
	 *  @param value C 字符串指针，可为 nullptr。
	 *  @param level 缩进层级（保留参数）。
	 *  @return 去除空白后的字符串，nullptr 返回空字符串。
	 */
	static std::string ToYmlValueString(const char *value, int level = 0)
	{
		(void)level;
		return value == nullptr ? std::string{} : yml_detail::trim(value);
	}

	/** @brief 将 bool 转换为 YAML 值字符串（"True"/"False"）。
	 *  @param value 布尔值。
	 *  @param level 缩进层级（保留参数）。
	 *  @return "True" 或 "False"。
	 */
	static std::string ToYmlValueString(bool value, int level = 0)
	{
		(void)level;
		return yml_detail::scalarToString(value);
	}

	/** @brief 将算术类型（非 bool）转换为 YAML 值字符串。
	 *  @tparam T 算术类型（int, float, double 等，排除 bool）。
	 *  @param value 数值。
	 *  @param level 缩进层级（保留参数）。
	 *  @return 格式化后的数值字符串。
	 */
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
	static std::string ToYmlValueString(T value, int level = 0)
	{
		(void)level;
		return yml_detail::scalarToString(value);
	}

	/** @brief 将枚举类型转换为 YAML 值字符串（使用 magic_enum）。
	 *  @tparam E 枚举类型。
	 *  @param value 枚举值。
	 *  @param level 缩进层级（保留参数）。
	 *  @return 枚举名称字符串或数值字符串。
	 */
	template <typename E, typename = std::enable_if_t<std::is_enum_v<E>>, typename = void>
	static std::string ToYmlValueString(E value, int level = 0)
	{
		(void)level;
		return yml_detail::scalarToString(value);
	}

	/** @brief 将 std::vector 转换为 YAML 数组字符串。
	 *  @tparam T 元素类型。
	 *  @param values 元素向量。
	 *  @param level 缩进层级（保留参数）。
	 *  @return "[ e1 , e2 , ... ]" 格式的字符串。
	 */
	template <typename T>
	static std::string ToYmlValueString(const std::vector<T> &values, int level = 0)
	{
		(void)level;
		return yml_detail::arrayToString(values);
	}

	/** @brief 将 std::array 转换为 YAML 数组字符串。
	 *  @tparam T 元素类型。
	 *  @tparam N 数组大小。
	 *  @param values 元素数组。
	 *  @param level 缩进层级（保留参数）。
	 *  @return "[ e1 , e2 , ... ]" 格式的字符串。
	 */
	template <typename T, size_t N>
	static std::string ToYmlValueString(const std::array<T, N> &values, int level = 0)
	{
		(void)level;
		return yml_detail::arrayToString(values);
	}

	/** @brief 将 std::initializer_list 转换为 YAML 数组字符串。
	 *  @tparam T 元素类型。
	 *  @param values 初始化列表。
	 *  @param level 缩进层级（保留参数）。
	 *  @return "[ e1 , e2 , ... ]" 格式的字符串。
	 */
	template <typename T>
	static std::string ToYmlValueString(std::initializer_list<T> values, int level = 0)
	{
		(void)level;
		return yml_detail::arrayToString(std::vector<T>(values));
	}

	/** @brief 将二维 vector 转换为多行 YAML 矩阵字符串。
	 *  @tparam T 元素类型。
	 *  @param data 二维向量。
	 *  @param level 缩进层级，控制前导空格数。
	 *  @return 带换行和缩进的多行矩阵字符串，每行以 "-  [ ... ]" 开头。
	 */
	template <typename T>
	static std::string ToYmlValueString(const std::vector<std::vector<T>> &data, int level = 0)
	{
		std::ostringstream stream;
		stream << '\n';
		const std::string indent(static_cast<size_t>(std::max(level, 0)) * 2, ' ');
		for (size_t r = 0; r < data.size(); ++r)
		{
			stream << indent << "-  [ ";
			for (size_t c = 0; c < data[r].size(); ++c)
			{
				if (c != 0)
					stream << " , ";
				stream << yml_detail::scalarToString(data[r][c]);
			}
			stream << " ]";
			if (r + 1 != data.size())
				stream << '\n';
		}
		return stream.str();
	}

	/** @brief 将 Eigen 矩阵/向量转换为 YAML 字符串。
	 *  @tparam Derived Eigen 派生类型。
	 *  @param data Eigen 矩阵或向量。
	 *  @param level 缩进层级，控制多行矩阵的前导空格。
	 *  @return 一维向量返回 "[...]" 格式，二维矩阵返回多行 "-  [ ... ]" 格式。
	 */
	template <typename Derived>
	static std::string ToYmlValueString(const Eigen::MatrixBase<Derived> &data, int level = 0)
	{
		if (data.rows() == 1 || data.cols() == 1)
		{
			std::vector<typename Derived::Scalar> values;
			values.reserve(static_cast<size_t>(data.size()));
			for (Eigen::Index i = 0; i < data.size(); ++i)
				values.push_back(data.derived().coeff(i));
			return yml_detail::arrayToString(values);
		}

		std::ostringstream stream;
		stream << '\n';
		const std::string indent(static_cast<size_t>(std::max(level, 0)) * 2, ' ');
		for (Eigen::Index r = 0; r < data.rows(); ++r)
		{
			stream << indent << "-  [ ";
			for (Eigen::Index c = 0; c < data.cols(); ++c)
			{
				if (c != 0)
					stream << " , ";
				stream << yml_detail::scalarToString(data.derived().coeff(r, c));
			}
			stream << " ]";
			if (r + 1 != data.rows())
				stream << '\n';
		}
		return stream.str();
	}

private:
	std::vector<std::string> lines;
	int tier = 0;
	std::string path;

	/** @brief 解析 YAML 行数据，构建节点树。
	 *  @param sourceLines 原始 YAML 行向量。
	 */
	void parseLines(const std::vector<std::string> &sourceLines)
	{
		nodeList.clear();
		std::vector<NodePtr> parentStack;
		for (size_t i = 0; i < sourceLines.size(); ++i)
		{
			const std::string &line = sourceLines[i];
			const std::string trimmed = yml_detail::trim(line);
			if (trimmed.empty() || yml_detail::startsWith(trimmed, "#"))
				continue;

			const size_t colon = line.find(':');
			if (colon == std::string::npos)
				continue;

			auto node = std::make_shared<Node>();
			node->space = yml_detail::leadingSpaces(line);
			node->name = yml_detail::trim(line.substr(0, colon));

			const std::string afterColon = yml_detail::trim(line.substr(colon + 1));
			if (afterColon.empty())
			{
				std::string block;
				for (size_t j = i + 1; j < sourceLines.size(); ++j)
				{
					const std::string &nextLine = sourceLines[j];
					const std::string nextTrimmed = yml_detail::trim(nextLine);
					if (nextTrimmed.empty())
						continue;
					if (!yml_detail::startsWith(nextTrimmed, "-"))
						break;
					if (yml_detail::leadingSpaces(nextLine) <= node->space)
						break;

					block.push_back('\n');
					block += nextLine;
				}
				if (!block.empty())
					node->value = std::move(block);
			}
			else
			{
				node->value = afterColon;
			}

			while (!parentStack.empty() && parentStack.back()->space >= node->space)
				parentStack.pop_back();
			node->parent = parentStack.empty() ? nullptr : parentStack.back();
			if (node->parent)
				node->tier = node->parent->tier + 1;
			parentStack.push_back(node);
			nodeList.push_back(std::move(node));
		}
	}

	/** @brief 按键路径查找节点智能指针（可变版本，内部）。
	 *  @param key 键路径。
	 *  @return 节点智能指针，找不到返回 nullptr。
	 */
	NodePtr findNodePtrByKey(const std::string &key)
	{
		return constFindNodePtrByKey(key);
	}

	/** @brief 按键路径查找节点智能指针（const 版本，内部）。
	 *  @param key 键路径。
	 *  @return 节点智能指针，找不到返回 nullptr。
	 */
	NodePtr findNodePtrByKey(const std::string &key) const
	{
		return constFindNodePtrByKey(key);
	}

	/** @brief 按键路径查找节点智能指针（内部核心实现）。
	 *  @param key 键路径（以 '.' 分隔）。
	 *  @return 匹配的节点智能指针，找不到返回 nullptr。
	 *  @note 从叶子节点名开始匹配，逐级向上验证父节点名。
	 */
	NodePtr constFindNodePtrByKey(const std::string &key) const
	{
		const auto keys = yml_detail::splitKey(key);
		if (keys.empty())
			return nullptr;

		for (const auto &node : nodeList)
		{
			if (node->name != keys.back())
				continue;

			NodePtr current = node;
			bool matched = true;
			for (size_t reverseIndex = keys.size() - 1; reverseIndex > 0; --reverseIndex)
			{
				const std::string &expectedParent = keys[reverseIndex - 1];
				if (!current->parent || current->parent->name != expectedParent)
				{
					matched = false;
					break;
				}
				current = current->parent;
			}

			if (matched)
				return node;
		}
		return nullptr;
	}

	/** @brief 查找当前节点的父节点（基于缩进层级）。
	 *  @param space 当前节点的缩进空格数。
	 *  @return 父节点智能指针，根节点返回 nullptr。
	 */
	NodePtr findParent(int space) const
	{
		for (auto it = nodeList.rbegin(); it != nodeList.rend(); ++it)
		{
			if ((*it)->space < space)
				return *it;
		}
		return nullptr;
	}

	/** @brief 递归收集父节点的所有子孙节点到删除集合。
	 *  @param parent 父节点。
	 *  @param nodesToRemove 输出参数，收集到的待删除节点集合。
	 */
	void collectChildren(const NodePtr &parent, std::unordered_set<Node *> &nodesToRemove) const
	{
		for (const auto &node : nodeList)
		{
			if (node->parent == parent)
			{
				nodesToRemove.insert(node.get());
				collectChildren(node, nodesToRemove);
			}
		}
	}

	/** @brief 重新格式化节点列表，按层级排序并重新计算缩进和层级。
	 *  @note 将根节点置为 tier=0, space=0，子节点递归递增。
	 */
	void formatting()
	{
		std::unordered_map<Node *, std::vector<NodePtr>> children;
		std::vector<NodePtr> roots;
		for (const auto &node : nodeList)
		{
			if (!node->parent)
				roots.push_back(node);
			else
				children[node->parent.get()].push_back(node);
		}
		auto rootRank = [](const NodePtr &node)
		{
			if (node->name == "Information")
				return 0;
			if (node->name == "Qahse")
				return 1;
			return 2;
		};
		std::stable_sort(roots.begin(), roots.end(), [&](const NodePtr &lhs, const NodePtr &rhs)
		{
			return rootRank(lhs) < rootRank(rhs);
		});

		std::vector<NodePtr> formatted;
		formatted.reserve(nodeList.size());
		std::function<void(const NodePtr &)> append = [&](const NodePtr &node)
		{
			formatted.push_back(node);
			for (const auto &child : children[node.get()])
			{
				child->tier = node->tier + 1;
				child->space = node->space + 2;
				append(child);
			}
		};

		for (const auto &root : roots)
		{
			root->tier = 0;
			root->space = 0;
			append(root);
		}
		nodeList = std::move(formatted);
	}

	/** @brief 递归将父节点的子节点追加到格式化列表。
	 *  @param parent 父节点。
	 *  @param formatted 输出参数，格式化后的节点列表。
	 */
	void appendChildren(const NodePtr &parent, std::vector<NodePtr> &formatted)
	{
		for (const auto &node : nodeList)
		{
			if (node->parent == parent)
			{
				node->tier = parent->tier + 1;
				node->space = parent->space + 2;
				formatted.push_back(node);
				appendChildren(node, formatted);
			}
		}
	}

	/** @brief 确保元数据节点存在（版本号、作者、最后修改时间）。
	 *  @note 每次保存 YAML 时都会更新最后修改时间。
	 */
	void ensureMetadata()
	{
		AddNode("Information.YMLVersion", ymlversion);
		AddNode("Information.Author", "YML module by Zhao Zizhen @copyright");
		AddNode("Information.LastModifiedTime", yml_detail::nowString());
	}
};

using Yaml = YML;
