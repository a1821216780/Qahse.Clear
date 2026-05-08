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
// 该软件按"原样"提供，不提供任何明示或暗示的保证，包括但不限于适销性、特定用途的适用性、所有权和非侵权。在任何情况下，版权持有人或任何分
// 发软件的人都不对任何索赔、损害或其他责任负责，无论是在合同诉讼、侵权诉讼或其他诉讼中，还是在软件使用或其他交易中产生的。
//
//**********************************************************************************************************************************

// ───────────────────────────────── File Info ─────────────────────────────────
//
// 该文件定义了序列化器基类 Serializer，支持关键字文本格式和 YAML 格式的读写操作，
// 提供标量、向量、矩阵、枚举等多种类型的序列化/反序列化接口。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <Eigen/Dense>

#include "IO/Yaml.hpp"
#include "IO/ZFile.hpp"
#include "IO/ZString.hpp"

/**
 * @class Serializer
 * @brief 序列化器基类，支持关键字文本格式和 YAML 格式的读写操作，提供标量、向量、矩阵、枚举等多种类型的序列化/反序列化接口。
 *
 * 子类通过重写 SerializeFields() 方法来定义需要序列化的成员字段，并在其中调用 ReadOrWrite() 模版函数。
 * 支持两种文件格式：
 * - Format::KeywordInput：每行 "Key = Value" 或 "Value \t Key" 格式的文本文件。
 * - Format::Yaml：标准 YAML 文件。
 */
class Serializer
{
public:

	/**
	 * @brief 序列化器工作模式枚举。
	 */
	enum class Mode
	{
		Read,   /**< 读取模式，从文件反序列化到成员变量 */
		Write   /**< 写入模式，将成员变量序列化到文件 */
	};

	/**
	 * @brief 序列化器文件格式枚举。
	 */
	enum class Format
	{
		KeywordInput,   /**< 关键字输入文本格式（Key = Value 或 Value Tab Key） */
		Yaml            /**< YAML 格式 */
	};

	/**
	 * @brief 默认构造函数，初始化为读取模式、关键字格式。
	 */
	Serializer() = default;

	/**
	 * @brief 构造并立即从文件读取序列化数据。
	 * @param filePath 文件路径。
	 * @param fmt 文件格式（KeywordInput 或 Yaml）。
	 */
	explicit Serializer(const std::string &filePath, Format fmt)
	{
		if (fmt == Format::KeywordInput)
			ReadTextFile(filePath);
		else
			ReadYamlFile(filePath);
	}

	/**
	 * @brief 获取当前工作模式。
	 * @return Mode::Read 或 Mode::Write。
	 */
	Mode GetMode() const
	{
		return mode_;
	}

	/**
	 * @brief 设置值优先模式（Value Tab Key 格式）。
	 * @param enabled true 表示值在前、键在后；false 表示键在前、值在后。
	 */
	void SetValueFirst(bool enabled)
	{
		valueFirst_ = enabled;
	}

	/**
	 * @brief 获取当前是否为值优先模式。
	 * @return 值优先模式返回 true。
	 */
	bool IsValueFirst() const
	{
		return valueFirst_;
	}

	/**
	 * @brief 从关键字文本文件读取并反序列化。
	 * @param path 文件路径。
	 */
	void ReadTextFile(const std::string &path)
	{
		SetMode(Mode::Read, Format::KeywordInput);
		inputLines_.clear();
		inputIndex_.clear();

		if (ZFile::Exists(path))
		{
			auto lines = ZFile::ReadAllLines(path);
			if (!lines.empty())
				lines[0] = ZString::RemoveUtf8Bom(lines[0]);
			ParseKeywordLines(lines);
		}

		SerializeFields();
	}

	/**
	 * @brief 序列化并写入关键字文本文件。
	 * @param path 文件路径。
	 */
	void WriteTextFile(const std::string &path)
	{
		SetMode(Mode::Write, Format::KeywordInput);
		SerializeFields();

		std::vector<std::string> outputLines;
		outputLines.reserve(inputLines_.size());
		for (const auto &line : inputLines_)
			outputLines.push_back(line.raw);
		ZFile::WriteAllLines(path, outputLines);
	}

	/**
	 * @brief 从 YAML 文件读取并反序列化。
	 * @param path 文件路径。
	 */
	void ReadYamlFile(const std::string &path)
	{
		SetMode(Mode::Read, Format::Yaml);
		yamlValues_.clear();
		yamlOrder_.clear();

		if (ZFile::Exists(path))
		{
			YML yaml(path);
			for (const auto &node : yaml.nodeList)
			{
				const std::string key = yaml.GetNodeKey(node);
				const std::string value = node->value.value_or("");
				yamlValues_[key] = value;
				yamlOrder_.push_back(key);
			}
		}

		SerializeFields();
	}

	/**
	 * @brief 序列化并写入 YAML 文件。
	 * @param path 文件路径。
	 */
	void WriteYamlFile(const std::string &path)
	{
		SetMode(Mode::Write, Format::Yaml);
		yamlValues_.clear();
		yamlOrder_.clear();

		SerializeFields();

		YML yaml;
		for (const auto &key : yamlOrder_)
			yaml.AddNode(key, yamlValues_.at(key));
		if (!yamlOrder_.empty())
			yaml.save(path);
	}

	/**
	 * @brief 尝试将字符串标记解析为枚举值。
	 * @tparam E 目标枚举类型。
	 * @param token 输入字符串标记。
	 * @param value 输出解析后的枚举值。
	 * @return 解析成功返回 true，否则返回 false。
	 */
	template <typename E>
	bool TryParseEnumToken(const std::string &token, E &value)
	{
		static_assert(std::is_enum_v<E>, "TryParseEnumToken requires enum type.");

		try
		{
			value = ZString::StringToEnum<E>(token, true);
			return true;
		}
		catch (...) {}

		try
		{
			const auto numeric = ZString::StringTo<std::underlying_type_t<E>>(token);
			auto parsed = magic_enum::enum_cast<E>(numeric);
			if (parsed)
			{
				value = *parsed;
				return true;
			}
		}
		catch (...) {}

		return false;
	}

protected:
	/**
	 * @brief 设置当前工作模式和文件格式。
	 * @param mode 工作模式（Read 或 Write）。
	 * @param format 文件格式（KeywordInput 或 Yaml）。
	 */
	void SetMode(Mode mode, Format format)
	{
		mode_ = mode;
		format_ = format;
	}

	/**
	 * @brief 判断当前是否为读取模式。
	 * @return 读取模式返回 true，写入模式返回 false。
	 */
	bool IsReadMode() const
	{
		return mode_ == Mode::Read;
	}

	/**
	 * @brief 获取当前文件格式。
	 * @return 当前 Format 枚举值。
	 */
	Format GetFormat() const
	{
		return format_;
	}

	/**
	 * @brief 读取或写入字符串值。
	 * @warning 禁止使用本方法读取/写入被引用标记包围的值！
	 * @param key 键名。
	 * @param value 字符串值引用。
	 */
	void ReadOrWrite(const std::string &key, std::string &value)
	{
		if (IsReadMode())
		{
			const std::string token = ReadValue(key);
			if (!token.empty() && !IsDefaultToken(token))
				value = token;
		}
		else
		{
			WriteValue(key, value);
		}
	}

	/**
	 * @brief 读取或写入 bool 值。
	 * @param key 键名。
	 * @param value bool 值引用。
	 */
	void ReadOrWrite(const std::string &key, bool &value)
	{
		if (IsReadMode())
		{
			const std::string token = ReadValue(key);
			if (!token.empty() && !IsDefaultToken(token))
				value = ParseBool(token);
		}
		else
		{
			WriteValue(key, FormatBool(value));
		}
	}

	/**
	 * @brief 读取或写入算术类型值（int、float、double 等，bool 除外）。
	 * @tparam T 算术类型。
	 * @param key 键名。
	 * @param value 算术值引用。
	 */
	template <typename T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, int> = 0>
	void ReadOrWrite(const std::string &key, T &value)
	{
		ReadOrWriteArithmetic(key, value);
	}

	/**
	 * @brief 读取或写入枚举值。
	 * @tparam E 枚举类型。
	 * @param key 键名。
	 * @param value 枚举值引用。
	 */
	template <typename E, std::enable_if_t<std::is_enum_v<E>, int> = 0>
	void ReadOrWrite(const std::string &key, E &value)
	{
		if (IsReadMode())
		{
			const std::string token = ReadValue(key);
			if (!token.empty() && !IsDefaultToken(token))
			{
				E parsed{};
				if (TryParseEnumToken(token, parsed))
					value = parsed;
			}
		}
		else
		{
			auto name = magic_enum::enum_name(value);
			WriteValue(key, name.empty() ? std::to_string(static_cast<int>(value)) : std::string(name));
		}
	}

	/**
	 * @brief 读取或写入 std::vector 容器。
	 * @tparam T 容器元素类型。
	 * @param key 键名。
	 * @param value 向量引用。
	 */
	template <typename T>
	void ReadOrWrite(const std::string &key, std::vector<T> &value)
	{
		if (IsReadMode())
		{
			const std::string token = ReadValue(key);
			if (token.empty() || IsDefaultToken(token))
				return;

			if (GetFormat() == Format::Yaml)
				ReadYamlVector(key, value);
			else
				ReadKeywordVector(token, value);
		}
		else
		{
			if (GetFormat() == Format::Yaml)
				WriteValue(key, YML::ToYmlValueString(value));
			else
				WriteValue(key, VectorToKeywordString(value));
		}
	}

	/**
	 * @brief 读取或写入 Eigen 矩阵/向量。
	 * @tparam Scalar 标量类型。
	 * @tparam Rows 行数（编译期）。
	 * @tparam Cols 列数（编译期）。
	 * @tparam Options 存储选项。
	 * @tparam MaxRows 最大行数。
	 * @tparam MaxCols 最大列数。
	 * @param key 键名。
	 * @param value Eigen 对象引用。
	 */
	template <typename Scalar, int Rows, int Cols, int Options, int MaxRows, int MaxCols>
	void ReadOrWrite(const std::string &key, Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols> &value)
	{
		using MatrixType = Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols>;

		if (IsReadMode())
		{
			const std::string token = ReadValue(key);
			if (token.empty() || IsDefaultToken(token))
				return;

			if (GetFormat() == Format::Yaml)
				ReadYamlEigen(key, value);
			else
				ReadKeywordEigen(token, value);
		}
		else
		{
			if (GetFormat() == Format::Yaml)
				WriteValue(key, YML::ToYmlValueString(value));
			else
				WriteValue(key, EigenToKeywordString(value));
		}
	}

	/**
	 * @brief 纯虚函数，子类重写以定义需要序列化的成员字段。
	 * @details 子类应在其中调用 ReadOrWrite() 来注册每个字段的序列化行为。
	 */
	virtual void SerializeFields()
	{
	}

private:
	/**
	 * @brief 表示已解析的输入行，包含键、值、注释等信息。
	 */
	struct InputLine
	{
		std::string raw;            /**< 原始文本行 */
		std::string valueToken;     /**< 值标记文本 */
		std::string key;            /**< 关键字（键名） */
		std::string comment;        /**< 注释内容 */
		bool isParameter = false;   /**< 该行是否包含有效参数 */
		bool wasQuoted = false;     /**< 值是否被引号包围 */
		char quote = '"';           /**< 引号字符类型 */
	};

	/**
	 * @brief 表示行解析过程中的单个标记（token）。
	 */
	struct Token
	{
		std::string text;       /**< 标记文本内容 */
		std::size_t begin = 0;  /**< 标记在行中的起始位置 */
		std::size_t end = 0;    /**< 标记在行中的结束位置 */
		bool quoted = false;    /**< 标记是否被引号包围 */
		char quote = '"';       /**< 引号字符类型 */
	};

	Mode mode_ = Mode::Read;
	Format format_ = Format::KeywordInput;
	bool valueFirst_ = false;
	std::vector<InputLine> inputLines_;
	std::unordered_map<std::string, std::size_t> inputIndex_;
	std::unordered_map<std::string, std::string> yamlValues_;
	std::vector<std::string> yamlOrder_;

	/**
	 * @brief 根据当前格式读取键对应的值。
	 * @param key 键名。
	 * @return 值字符串，未找到时返回空字符串。
	 */
	std::string ReadValue(const std::string &key) const
	{
		return format_ == Format::KeywordInput ? ReadKeywordToken(key) : ReadYamlValue(key);
	}

	/**
	 * @brief 根据当前格式写入键值对。
	 * @param key 键名。
	 * @param value 值字符串。
	 */
	void WriteValue(const std::string &key, const std::string &value)
	{
		if (format_ == Format::KeywordInput)
			WriteKeywordToken(key, value);
		else
			WriteYamlValue(key, value);
	}

	/**
	 * @brief 解析关键字文本文件的行列表。
	 * @details 按行进行标记化，提取 key、valueToken、comment 等信息，并建立键名到行索引的映射。
	 * @param lines 文件中的原始行向量。
	 */
	void ParseKeywordLines(const std::vector<std::string> &lines)
	{
		inputLines_.clear();
		inputIndex_.clear();

		for (const auto &raw : lines)
		{
			InputLine line;
			line.raw = raw;

			const auto tokens = TokenizeLine(raw);
			if (tokens.empty())
			{
				inputLines_.push_back(line);
				continue;
			}

			if (valueFirst_)
			{
				if (tokens.size() >= 2)
				{
					line.valueToken = tokens[0].text;
					line.key = tokens[1].text;
					line.wasQuoted = tokens[0].quoted;
					line.quote = tokens[0].quote;
				}
				else
				{
					line.key = tokens[0].text;
				}
			}
			else
			{
				line.key = tokens[0].text;
				if (tokens.size() >= 2)
				{
					line.valueToken = tokens[1].text;
					line.wasQuoted = tokens[1].quoted;
					line.quote = tokens[1].quote;
				}
			}

			line.isParameter = !line.key.empty();
			const std::size_t commentPos = FindCommentStart(raw);
			if (commentPos != std::string::npos)
				line.comment = ZString::Trim(raw.substr(commentPos));

			if (line.isParameter)
				inputIndex_[ZString::ToUpper(std::string(line.key))] = inputLines_.size();
			inputLines_.push_back(line);
		}
	}

	/**
	 * @brief 查找行中注释的起始位置。
	 * @details 支持 #、! 注释符和 - 值优先模式注释。引号内的注释符会被忽略。
	 * @param line 原始行字符串。
	 * @return 注释起始位置索引，未找到时返回 std::string::npos。
	 */
	std::size_t FindCommentStart(const std::string &line) const
	{
		bool inQuote = false;
		char quote = '"';
		for (std::size_t i = 0; i < line.size(); ++i)
		{
			const char ch = line[i];
			if (inQuote)
			{
				if (ch == quote)
					inQuote = false;
				continue;
			}

			if (ch == '"' || ch == '\'')
			{
				inQuote = true;
				quote = ch;
			}
			else if (ch == '#' || ch == '!')
			{
				return i;
			}
			else if (valueFirst_ && ch == '-' && i > 0 && std::isspace(static_cast<unsigned char>(line[i - 1])))
			{
				return i;
			}
		}
		return std::string::npos;
	}

	/**
	 * @brief 将一行文本解析为标记列表。
	 * @details 识别引号包围的标记和普通标记，自动跳过空白和注释行。
	 * @param line 原始行字符串。
	 * @return 标记向量。
	 */
	std::vector<Token> TokenizeLine(const std::string &line) const
	{
		std::vector<Token> tokens;
		std::size_t i = 0;
		const std::size_t n = line.size();

		while (i < n && std::isspace(static_cast<unsigned char>(line[i])))
			++i;
		if (i >= n)
			return tokens;
		if (line[i] == '#' || line[i] == '!')
			return tokens;
		if (i + 1 < n && line[i] == '-' && line[i + 1] == '-')
			return tokens;

		while (i < n)
		{
			while (i < n && (std::isspace(static_cast<unsigned char>(line[i])) || line[i] == '=' || line[i] == ':'))
				++i;
			if (i >= n || line[i] == '#' || line[i] == '!')
				break;
			if (line[i] == '-' && tokens.size() >= 2)
				break;

			Token token;
			token.begin = i;
			if (line[i] == '"' || line[i] == '\'')
			{
				token.quoted = true;
				token.quote = line[i++];
				token.begin = i;
				while (i < n && line[i] != token.quote)
					token.text.push_back(line[i++]);
				token.end = i;
				if (i < n)
					++i;
			}
			else
			{
				while (i < n && !std::isspace(static_cast<unsigned char>(line[i])) &&
				       line[i] != '=' && line[i] != ':' && line[i] != '#' && line[i] != '!')
				{
					if (line[i] == '-' && tokens.size() >= 2)
						break;
					token.text.push_back(line[i++]);
				}
				token.end = i;
			}

			if (!token.text.empty())
				tokens.push_back(token);
			if (tokens.size() >= 2)
				break;
		}

		return tokens;
	}

	/**
	 * @brief 从关键字文本中读取指定键的值标记。
	 * @param key 键名（不区分大小写）。
	 * @return 值标记文本，未找到时返回空字符串。
	 */
	std::string ReadKeywordToken(const std::string &key) const
	{
		const auto it = inputIndex_.find(ZString::ToUpper(key));
		return it == inputIndex_.end() ? std::string{} : inputLines_[it->second].valueToken;
	}

	/**
	 * @brief 向关键字文本中写入指定键的值。
	 * @details 若键已存在则更新该行的值和原始文本；若不存在则追加新行。
	 * @param key 键名。
	 * @param value 值字符串。
	 */
	void WriteKeywordToken(const std::string &key, const std::string &value)
	{
		const std::string upperKey = ZString::ToUpper(key);
		const auto it = inputIndex_.find(upperKey);
		if (it != inputIndex_.end())
		{
			auto &line = inputLines_[it->second];
			const std::string formatted = FormatKeywordValue(value);
			std::ostringstream out;
			if (valueFirst_)
				out << formatted << "\t" << line.key;
			else
				out << line.key << " = " << formatted;
			if (!line.comment.empty())
				out << "  " << line.comment;
			line.raw = out.str();
			line.valueToken = value;
			return;
		}

		InputLine line;
		line.key = key;
		line.valueToken = value;
		line.isParameter = true;

		std::ostringstream out;
		if (valueFirst_)
			out << FormatKeywordValue(value) << "\t" << key;
		else
			out << key << " = " << FormatKeywordValue(value);
		line.raw = out.str();

		inputIndex_[upperKey] = inputLines_.size();
		inputLines_.push_back(line);
	}

	/**
	 * @brief 对值进行格式化，必要时添加引号。
	 * @details 若值为空或包含空格、制表符、=、:、#、! 等特殊字符，则用双引号包围。
	 * @param value 原始值字符串。
	 * @return 格式化后的字符串。
	 */
	static std::string FormatKeywordValue(const std::string &value)
	{
		const bool quote = value.empty() || value.find_first_of(" \t:=#!") != std::string::npos;
		return quote ? ("\"" + value + "\"") : value;
	}

	/**
	 * @brief 从 YAML 键值映射中读取值。
	 * @param key 键名。
	 * @return 值字符串，未找到时返回空字符串。
	 */
	std::string ReadYamlValue(const std::string &key) const
	{
		const auto it = yamlValues_.find(key);
		return it == yamlValues_.end() ? std::string{} : it->second;
	}

	/**
	 * @brief 向 YAML 键值映射中写入值。
	 * @param key 键名。
	 * @param value 值字符串。
	 */
	void WriteYamlValue(const std::string &key, const std::string &value)
	{
		if (yamlValues_.find(key) == yamlValues_.end())
			yamlOrder_.push_back(key);
		yamlValues_[key] = value;
	}

	/**
	 * @brief 从 YAML 值解析为 std::vector。
	 * @tparam T 向量元素类型。
	 * @param key 键名。
	 * @param value 输出向量引用。
	 */
	template <typename T>
	void ReadYamlVector(const std::string &key, std::vector<T> &value)
	{
		const std::string yamlValue = ReadYamlValue(key);
		if (yamlValue.empty())
			return;

		try
		{
			if constexpr (std::is_same_v<T, bool>)
				value = YML::YmlToBoolArray(yamlValue);
			else if constexpr (std::is_same_v<T, int>)
				value = YML::YmlToIntArray(yamlValue);
			else if constexpr (std::is_same_v<T, double>)
				value = YML::YmlToDoubleArray(yamlValue);
			else if constexpr (std::is_same_v<T, float>)
				value = YML::YmlToFloatArray(yamlValue);
			else if constexpr (std::is_same_v<T, std::string>)
				value = YML::YmlToStringArray(yamlValue);
			else
			{
				auto tokens = yml_detail::splitScalarList(yamlValue);
				value.clear();
				value.reserve(tokens.size());
				for (const auto &token : tokens)
					value.push_back(ZString::StringTo<T>(token));
			}
		}
		catch (...) {}
	}

	/**
	 * @brief 从关键字标记解析为 std::vector。
	 * @tparam T 向量元素类型。
	 * @param token 关键字值标记（"[a, b, c]" 格式）。
	 * @param value 输出向量引用。
	 */
	template <typename T>
	static void ReadKeywordVector(const std::string &token, std::vector<T> &value)
	{
		value.clear();
		std::string cleaned = token;
		if (!cleaned.empty() && cleaned.front() == '[')
			cleaned.erase(0, 1);
		if (!cleaned.empty() && cleaned.back() == ']')
			cleaned.pop_back();

		auto tokens = ZString::Split(cleaned, ',');
		for (auto &part : tokens)
		{
			part = ZString::Trim(part);
			if (part.empty())
				continue;
			try { value.push_back(ZString::StringTo<T>(part)); }
			catch (...) {}
		}
	}

	/**
	 * @brief 将 std::vector 转换为关键字字符串格式。
	 * @tparam T 向量元素类型。
	 * @param value 输入向量。
	 * @return 格式化后的字符串（"[a, b, c]" 格式）。
	 */
	template <typename T>
	static std::string VectorToKeywordString(const std::vector<T> &value)
	{
		std::ostringstream out;
		out << "[";
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			if (i != 0)
				out << ", ";
			if constexpr (std::is_same_v<T, std::string>)
				out << "\"" << value[i] << "\"";
			else if constexpr (std::is_same_v<T, bool>)
				out << (value[i] ? "True" : "False");
			else
				out << value[i];
		}
		out << "]";
		return out.str();
	}

	/**
	 * @brief 从关键字标记解析为 Eigen 矩阵/向量。
	 * @tparam MatrixType Eigen 矩阵或向量类型。
	 * @param token 关键字值标记。
	 * @param value 输出 Eigen 对象引用。
	 */
	template <typename MatrixType>
	static void ReadKeywordEigen(const std::string &token, MatrixType &value)
	{
		using Scalar = typename MatrixType::Scalar;
		constexpr bool isColVector = MatrixType::ColsAtCompileTime == 1 && MatrixType::RowsAtCompileTime != 1;
		constexpr bool isRowVector = MatrixType::RowsAtCompileTime == 1 && MatrixType::ColsAtCompileTime != 1;
		try
		{
			if constexpr (isColVector || isRowVector)
			{
				auto vec = ZString::StringToEigen<Eigen::Matrix<Scalar, Eigen::Dynamic, 1>>(token);
				value = vec.template cast<Scalar>();
			}
			else
			{
				std::size_t rowCount = 1;
				for (char ch : token)
				{
					if (ch == ';' || ch == '\n')
						++rowCount;
				}
				if constexpr (MatrixType::RowsAtCompileTime != Eigen::Dynamic)
					rowCount = static_cast<std::size_t>(MatrixType::RowsAtCompileTime);
				auto mat = ZString::StringToEigenMatrix<Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>>(token, rowCount);
				value = mat;
			}
		}
		catch (...) {}
	}

	/**
	 * @brief 从 YAML 值解析为 Eigen 矩阵/向量。
	 * @tparam MatrixType Eigen 矩阵或向量类型。
	 * @param key 键名。
	 * @param value 输出 Eigen 对象引用。
	 */
	template <typename MatrixType>
	void ReadYamlEigen(const std::string &key, MatrixType &value)
	{
		const std::string yamlValue = ReadYamlValue(key);
		if (yamlValue.empty())
			return;

		using Scalar = typename MatrixType::Scalar;
		constexpr bool isColVector = MatrixType::ColsAtCompileTime == 1 && MatrixType::RowsAtCompileTime != 1;
		constexpr bool isRowVector = MatrixType::RowsAtCompileTime == 1 && MatrixType::ColsAtCompileTime != 1;
		try
		{
			if constexpr ((isColVector || isRowVector) && MatrixType::SizeAtCompileTime != Eigen::Dynamic)
			{
				auto vec = YML::YmlToVector(yamlValue);
				value.resize(vec.size(), 1);
				for (Eigen::Index i = 0; i < vec.size(); ++i)
					value(static_cast<int>(i)) = static_cast<Scalar>(vec(i));
			}
			else
			{
				auto matrix = YML::YmlToMatrix(yamlValue);
				value.resize(matrix.RowCount, matrix.ColumnCount);
				for (int r = 0; r < matrix.RowCount; ++r)
					for (int c = 0; c < matrix.ColumnCount; ++c)
						value(r, c) = static_cast<Scalar>(matrix.data(r, c));
			}
		}
		catch (...) {}
	}

	/**
	 * @brief 将 Eigen 矩阵/向量转换为关键字字符串格式。
	 * @tparam MatrixType Eigen 矩阵或向量类型。
	 * @param value 输入 Eigen 对象。
	 * @return 格式化后的字符串（向量用空格分隔，矩阵用 "; " 分隔行）。
	 */
	template <typename MatrixType>
	static std::string EigenToKeywordString(const MatrixType &value)
	{
		std::ostringstream out;
		if (value.rows() == 1 || value.cols() == 1)
		{
			for (Eigen::Index i = 0; i < value.size(); ++i)
			{
				if (i != 0)
					out << ' ';
				out << value(static_cast<int>(i));
			}
		}
		else
		{
			for (Eigen::Index r = 0; r < value.rows(); ++r)
			{
				if (r != 0)
					out << "; ";
				for (Eigen::Index c = 0; c < value.cols(); ++c)
				{
					if (c != 0)
						out << ' ';
					out << value(r, c);
				}
			}
		}
		return out.str();
	}

	/**
	 * @brief 判断值标记是否为默认标记（"DEFAULT"，不区分大小写）。
	 * @param token 值标记字符串。
	 * @return 是默认标记返回 true。
	 */
	static bool IsDefaultToken(const std::string &token)
	{
		return ZString::ToUpper(token) == "DEFAULT";
	}

	/**
	 * @brief 解析布尔字符串标记。
	 * @details 支持 "True"/"Ture"/"1"/"Yes"/"On" 为 true，"False"/"0"/"No"/"Off" 为 false。
	 * @param token 布尔字符串标记。
	 * @return 解析后的 bool 值。
	 */
	static bool ParseBool(const std::string &token)
	{
		const std::string upper = ZString::ToUpper(ZString::Trim(token));
		if (upper == "TRUE" || upper == "TURE" || upper == "1" || upper == "YES" || upper == "ON")
			return true;
		if (upper == "FALSE" || upper == "0" || upper == "NO" || upper == "OFF")
			return false;
		try { return ZString::StringToBool(token); }
		catch (...) { return false; }
	}

	/**
	 * @brief 将 bool 值格式化为字符串。
	 * @param value 布尔值。
	 * @return "True" 或 "False"。
	 */
	static std::string FormatBool(bool value)
	{
		return value ? "True" : "False";
	}

	/**
	 * @brief 读取或写入算术类型值（int、float、double 等，bool 除外）。
	 * @tparam T 算术类型。
	 * @param key 键名。
	 * @param value 算术值引用。
	 */
	template <typename T>
	void ReadOrWriteArithmetic(const std::string &key, T &value)
	{
		if (IsReadMode())
		{
			const std::string token = ReadValue(key);
			if (!token.empty() && !IsDefaultToken(token))
			{
				try { value = ZString::StringTo<T>(token); }
				catch (...) {}
			}
		}
		else
		{
			WriteValue(key, YML::ToYmlValueString(value));
		}
	}
};
