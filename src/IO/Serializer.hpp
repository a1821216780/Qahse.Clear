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

class Serializer
{
public:

	enum class Mode
	{
		Read,
		Write
	};

	enum class Format
	{
		KeywordInput,
		Yaml
	};

	Serializer() = default;

	explicit Serializer(const std::string &filePath, Format fmt)
	{
		if (fmt == Format::KeywordInput)
			ReadTextFile(filePath);
		else
			ReadYamlFile(filePath);
	}

	Mode GetMode() const
	{
		return mode_;
	}

	void SetValueFirst(bool enabled)
	{
		valueFirst_ = enabled;
	}

	bool IsValueFirst() const
	{
		return valueFirst_;
	}

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
	void SetMode(Mode mode, Format format)
	{
		mode_ = mode;
		format_ = format;
	}

	bool IsReadMode() const
	{
		return mode_ == Mode::Read;
	}

	Format GetFormat() const
	{
		return format_;
	}

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

	template <typename T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, int> = 0>
	void ReadOrWrite(const std::string &key, T &value)
	{
		ReadOrWriteArithmetic(key, value);
	}

	template <typename E, std::enable_if_t<std::is_enum_v<E>, int> = 0>
	void ReadOrWrite(const std::string &key, E &value)
	{
		if (IsReadMode())
		{
			const std::string token = ReadValue(key);
			if (!token.empty() && !IsDefaultToken(token))
			{
				E parsed{};
				if ( TryParseEnumToken(token, parsed))
					value = parsed;
			}
		}
		else
		{
			auto name = magic_enum::enum_name(value);
			WriteValue(key, name.empty() ? std::to_string(static_cast<int>(value)) : std::string(name));
		}
	}

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

	virtual void SerializeFields()
	{
	}

private:
	struct InputLine
	{
		std::string raw;
		std::string valueToken;
		std::string key;
		std::string comment;
		bool isParameter = false;
		bool wasQuoted = false;
		char quote = '"';
	};

	struct Token
	{
		std::string text;
		std::size_t begin = 0;
		std::size_t end = 0;
		bool quoted = false;
		char quote = '"';
	};

	Mode mode_ = Mode::Read;
	Format format_ = Format::KeywordInput;
	bool valueFirst_ = false;
	std::vector<InputLine> inputLines_;
	std::unordered_map<std::string, std::size_t> inputIndex_;
	std::unordered_map<std::string, std::string> yamlValues_;
	std::vector<std::string> yamlOrder_;

	std::string ReadValue(const std::string &key) const
	{
		return format_ == Format::KeywordInput ? ReadKeywordToken(key) : ReadYamlValue(key);
	}

	void WriteValue(const std::string &key, const std::string &value)
	{
		if (format_ == Format::KeywordInput)
			WriteKeywordToken(key, value);
		else
			WriteYamlValue(key, value);
	}

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
				inputIndex_[Upper(line.key)] = inputLines_.size();
			inputLines_.push_back(line);
		}
	}

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

	std::string ReadKeywordToken(const std::string &key) const
	{
		const auto it = inputIndex_.find(Upper(key));
		return it == inputIndex_.end() ? std::string{} : inputLines_[it->second].valueToken;
	}

	void WriteKeywordToken(const std::string &key, const std::string &value)
	{
		const std::string upperKey = Upper(key);
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

	static std::string FormatKeywordValue(const std::string &value)
	{
		const bool quote = value.empty() || value.find_first_of(" \t:=#!") != std::string::npos;
		return quote ? ("\"" + value + "\"") : value;
	}

	std::string ReadYamlValue(const std::string &key) const
	{
		const auto it = yamlValues_.find(key);
		return it == yamlValues_.end() ? std::string{} : it->second;
	}

	void WriteYamlValue(const std::string &key, const std::string &value)
	{
		if (yamlValues_.find(key) == yamlValues_.end())
			yamlOrder_.push_back(key);
		yamlValues_[key] = value;
	}

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

	static bool IsDefaultToken(const std::string &token)
	{
		return Upper(token) == "DEFAULT";
	}

	static bool ParseBool(const std::string &token)
	{
		const std::string upper = Upper(ZString::Trim(token));
		if (upper == "TRUE" || upper == "TURE" || upper == "1" || upper == "YES" || upper == "ON")
			return true;
		if (upper == "FALSE" || upper == "0" || upper == "NO" || upper == "OFF")
			return false;
		try { return ZString::StringToBool(token); }
		catch (...) { return false; }
	}

	static std::string FormatBool(bool value)
	{
		return value ? "True" : "False";
	}

	static std::string Upper(const std::string &value)
	{
		return ZString::ToUpper(std::string(value));
	}

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
