#pragma once

#include <Eigen/Dense>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "IO/Yaml.hpp"
#include "IO/ZFile.hpp"
#include "IO/ZString.hpp"

class Serializer
{
public:
	enum class Mode { Read, Write };
	enum class Format { KeywordInput, Yaml };

	struct LineRef
	{
		int index = -1;
		std::string key;

		bool Found() const { return index >= 0; }
		LineRef Add(int offset) const { return {Found() ? index + offset : -1, key}; }
		LineRef operator+(int offset) const { return Add(offset); }
	};

	template <typename T>
	struct FieldRef
	{
		const char *key = "";
		T &value;
	};

	template <typename T>
	static FieldRef<T> Field(const char *key, T &value)
	{
		return {key, value};
	}

	Serializer() = default;

	explicit Serializer(const std::string &path, Format format)
	{
		if (format == Format::Yaml)
			ReadYamlFile(path);
		else
			ReadTextFile(path);
	}

	explicit Serializer(const std::string &path, std::string yamlRoot, bool valueFirst = true)
	{
		SetYamlRoot(std::move(yamlRoot));
		SetValueFirst(valueFirst);
		ReadFile(path);
	}

	static Serializer OpenReader(const std::string &path, std::string yamlRoot = {}, bool valueFirst = true)
	{
		Serializer reader;
		reader.SetYamlRoot(std::move(yamlRoot));
		reader.SetValueFirst(valueFirst);
		reader.LoadFile(path);
		return reader;
	}

	static Serializer OpenYamlWriter(std::string yamlRoot = {})
	{
		Serializer writer;
		writer.SetYamlRoot(std::move(yamlRoot));
		return writer;
	}

	void SetValueFirst(bool enabled) { valueFirst_ = enabled; }
	void SetYamlRoot(std::string root) { yamlRoot_ = std::move(root); }

	Mode GetMode() const { return mode_; }
	Format GetFormat() const { return format_; }
	bool IsReadMode() const { return mode_ == Mode::Read; }
	bool IsYaml() const { return format_ == Format::Yaml; }

	static bool IsYamlPath(const std::string &path)
	{
		const auto ext = ZString::ToUpper(std::filesystem::path(path).extension().string());
		return ext == ".YML" || ext == ".YAML" || ext == ".SIM";
	}

	void ReadFile(const std::string &path)
	{
		if (IsYamlPath(path))
			ReadYamlFile(path);
		else
			ReadTextFile(path);
	}

	void WriteFile(const std::string &path)
	{
		if (IsYamlPath(path))
			WriteYamlFile(path);
		else
			WriteTextFile(path);
	}

	void LoadFile(const std::string &path)
	{
		if (IsYamlPath(path))
			LoadYamlFile(path);
		else
			LoadTextFile(path);
	}

	void ReadTextFile(const std::string &path)
	{
		LoadTextFile(path);
		SerializeFields();
	}

	void LoadTextFile(const std::string &path)
	{
		filePath_ = path;
		SetMode(Mode::Read, Format::KeywordInput);
		if (!ZFile::Exists(path))
		{
			inputLines_.clear();
			inputIndex_.clear();
			return;
		}

		auto lines = ZFile::ReadAllLines(path);
		if (!lines.empty())
			lines[0] = ZString::RemoveUtf8Bom(lines[0]);
		ParseKeywordLines(lines);
	}

	void ReadTextLines(std::vector<std::string> lines)
	{
		filePath_.clear();
		SetMode(Mode::Read, Format::KeywordInput);
		if (!lines.empty())
			lines[0] = ZString::RemoveUtf8Bom(lines[0]);
		ParseKeywordLines(lines);
	}

	void WriteTextFile(const std::string &path)
	{
		SetMode(Mode::Write, Format::KeywordInput);
		SerializeFields();

		std::vector<std::string> lines;
		lines.reserve(inputLines_.size());
		for (const auto &line : inputLines_)
			lines.push_back(line.raw);
		ZFile::WriteAllLines(path, lines);
	}

	void ReadYamlFile(const std::string &path)
	{
		LoadYamlFile(path);
		SerializeFields();
	}

	void LoadYamlFile(const std::string &path)
	{
		filePath_ = path;
		SetMode(Mode::Read, Format::Yaml);
		yamlValues_.clear();
		yamlOrder_.clear();

		if (!ZFile::Exists(path))
			return;

		YML yaml(path);
		for (const auto &node : yaml.nodeList)
		{
			const std::string key = yaml.GetNodeKey(node);
			yamlValues_[key] = node->value.value_or("");
			yamlOrder_.push_back(key);
		}
	}

	void WriteYamlFile(const std::string &path)
	{
		SetMode(Mode::Write, Format::Yaml);
		yamlValues_.clear();
		yamlOrder_.clear();
		SerializeFields();
		SaveYamlFile(path);
	}

	void SaveYamlFile(const std::string &path) const
	{
		YML yaml;
		for (const auto &key : yamlOrder_)
			yaml.AddNode(key, yamlValues_.at(key));
		if (!yamlOrder_.empty())
			yaml.save(path);
	}

	std::vector<std::string> RawLines() const
	{
		std::vector<std::string> lines;
		lines.reserve(inputLines_.size());
		for (const auto &line : inputLines_)
			lines.push_back(line.raw);
		return lines;
	}

	static std::vector<int> GetMatchingLineIndexes(const std::vector<std::string> &input,
	                                               const std::string &searchTerm,
	                                               const std::string &path,
	                                               bool error = true,
	                                               bool show = true)
	{
		(void)path;
		(void)error;
		(void)show;
		std::vector<int> indexes;
		for (std::size_t i = 0; i < input.size(); ++i)
			if (input[i].find(searchTerm) != std::string::npos)
				indexes.push_back(static_cast<int>(i));
		if (indexes.empty())
			indexes.push_back(-1);
		return indexes;
	}

	LineRef fd(const std::string &key, bool error = true, bool show = true) const
	{
		if (IsYaml())
			return FindLine(key, error);
		const auto indexes = GetMatchingLineIndexes(RawLines(), key, filePath_, error, show);
		return {indexes.empty() ? -1 : indexes.front(), key};
	}

	LineRef FindLine(const std::string &key, bool required = false) const
	{
		if (IsYaml())
		{
			const auto yamlKey = QualifyYamlKey(key);
			const bool found = yamlValues_.find(yamlKey) != yamlValues_.end();
			if (!found && required)
				throw std::runtime_error("Cannot find key: " + key);
			return {found ? 0 : -1, yamlKey};
		}

		const auto it = inputIndex_.find(NormalizeKey(key));
		if (it == inputIndex_.end())
		{
			if (required)
				throw std::runtime_error("Cannot find key: " + key);
			return {-1, key};
		}
		return {static_cast<int>(it->second), key};
	}

	template <typename T>
	T Read(const std::string &key, T defaultValue) const
	{
		T value = defaultValue;
		TryParseLine(key, value);
		return value;
	}

	template <typename T>
	T ReadAny(std::initializer_list<std::string> keys, T defaultValue) const
	{
		for (const auto &key : keys)
		{
			T value = defaultValue;
			if (TryParseLine(key, value))
				return value;
		}
		return defaultValue;
	}

	std::string read(const std::string &key) const
	{
		return ReadValue(key);
	}

	template <typename T>
	T read(const std::string &key, T defaultValue) const
	{
		return Read<T>(key, defaultValue);
	}

	template <typename T>
	bool TryParseLine(const std::string &key, T &value) const
	{
		return ParseValue(ReadValue(key), value);
	}

	template <typename T>
	bool TryParseLine(LineRef line, T &value) const
	{
		if (IsYaml())
			return ParseValue(ReadYamlValue(line.key), value);
		if (!line.Found() || line.index >= static_cast<int>(inputLines_.size()))
			return false;
		return ParseValue(inputLines_[static_cast<std::size_t>(line.index)].valueToken, value);
	}

	template <typename T>
	T ParseLine(LineRef line,
	            T defaultValue,
	            int num = 0,
	            char fg = ' ',
	            char fg1 = '\t',
	            int station = 0,
	            const std::vector<std::string> *namelist = nullptr,
	            bool row = false,
	            const std::string *TiltleLine = nullptr,
	            bool warning = true,
	            const std::string &errorinf = "") const
	{
		(void)num;
		(void)namelist;
		(void)row;
		(void)TiltleLine;
		(void)warning;
		(void)errorinf;

		T value = defaultValue;
		if (IsYaml())
		{
			ParseValue(ReadYamlValue(line.key), value);
			return value;
		}
		if (!line.Found() || line.index >= static_cast<int>(inputLines_.size()))
			return value;

		const std::string token = station == 0
			? inputLines_[static_cast<std::size_t>(line.index)].valueToken
			: TokenAtStation(inputLines_[static_cast<std::size_t>(line.index)].raw, fg, fg1, station);
		ParseValue(token, value);
		return value;
	}

	template <typename T>
	T ParseLine(const std::string &key, T defaultValue) const
	{
		return Read<T>(key, defaultValue);
	}

	template <typename T>
	T ParseRequiredLine(const std::string &key) const
	{
		T value{};
		if (!TryParseLine(key, value))
			throw std::runtime_error("Cannot parse required key: " + key);
		return value;
	}

	template <typename T>
	static T ParseLine(const std::vector<std::string> &lines,
	                   const std::string &filename,
	                   LineRef pp,
	                   std::optional<T> moren = std::nullopt,
	                   int num = 0,
	                   char fg = ' ',
	                   char fg1 = '\t',
	                   int station = 0,
	                   const std::vector<std::string> *namelist = nullptr,
	                   bool row = false,
	                   const std::string *TiltleLine = nullptr,
	                   bool warning = true,
	                   const std::string &errorinf = "")
	{
		Serializer reader;
		reader.SetValueFirst(true);
		reader.ReadTextLines(lines);
		reader.filePath_ = filename;
		if (moren.has_value())
			return reader.ParseLine<T>(pp, *moren, num, fg, fg1, station, namelist, row, TiltleLine, warning, errorinf);

		T value{};
		if (!reader.TryParseLine(pp, value))
			throw std::runtime_error("Cannot parse required key: " + pp.key);
		return value;
	}

	Eigen::MatrixXd ReadMatrix(const std::string &key) const
	{
		return IsYaml() ? ParseYamlMatrix(key) : ParseMatrixAt(FindLine(key), 0, 0);
	}

	Eigen::MatrixXd ReadMatrixAfter(const std::string &key, int rows, int cols, int rowOffset = 1) const
	{
		return ParseMatrixAt(FindLine(key).Add(rowOffset), rows, cols);
	}

	Eigen::MatrixXd ReadMatrixAfter(const std::string &yamlKey,
	                                const std::string &textKey,
	                                int rows,
	                                int cols,
	                                int rowOffset = 1) const
	{
		return IsYaml() ? ParseYamlMatrix(yamlKey) : ParseMatrixAt(fd(textKey).Add(rowOffset), rows, cols);
	}

	Eigen::MatrixXd ParseMatrixAt(LineRef start, int rows, int cols) const
	{
		return ParseMatrixAtImpl(start, rows, cols);
	}

	std::vector<std::vector<double>> ReadTableRows(const std::string &yamlKey,
	                                               std::size_t textCols,
	                                               std::size_t textRows = 0) const
	{
		return IsYaml()
			? RowsFromMatrix(ReadMatrix(yamlKey))
			: ReadRowsAfterBegin(RawLines(), textCols, textRows);
	}

	void AddNode(const std::string &key, const std::string &value)
	{
		WriteYamlValue(key, value);
	}

	template <typename T>
	void AddNode(const std::string &key, const T &value, int level = 0)
	{
		WriteYamlValue(key, YML::ToYmlValueString(value, level));
	}

	static std::string JoinNumbers(const std::vector<double> &values)
	{
		std::ostringstream out;
		out << std::setprecision(std::numeric_limits<double>::max_digits10);
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			if (i != 0)
				out << ' ';
			out << values[i];
		}
		return out.str();
	}

	static std::vector<std::vector<double>> RowsFromMatrix(const Eigen::MatrixXd &matrix)
	{
		std::vector<std::vector<double>> rows;
		rows.reserve(static_cast<std::size_t>(matrix.rows()));
		for (Eigen::Index r = 0; r < matrix.rows(); ++r)
		{
			std::vector<double> row;
			row.reserve(static_cast<std::size_t>(matrix.cols()));
			for (Eigen::Index c = 0; c < matrix.cols(); ++c)
				row.push_back(matrix(r, c));
			rows.push_back(std::move(row));
		}
		return rows;
	}

	static bool TryParseNumberRow(const std::string &line, std::vector<double> &row)
	{
		row.clear();
		std::istringstream stream(ZString::Trim(line));
		std::string token;
		while (stream >> token)
		{
			token = ZString::Trim(token);
			if (token.empty())
				continue;
			while (!token.empty() && token.back() == ',')
				token.pop_back();

			char *end = nullptr;
			const double value = std::strtod(token.c_str(), &end);
			if (end != token.c_str() && *end == '\0')
			{
				row.push_back(value);
				continue;
			}
			if (!row.empty())
				break;
			return false;
		}
		return !row.empty();
	}

	static std::vector<std::vector<double>> ReadRowsAfterBegin(const std::vector<std::string> &lines,
	                                                           std::size_t expectedCols,
	                                                           std::size_t maxRows = 0)
	{
		std::vector<std::vector<double>> rows;
		bool inBlock = false;
		for (const auto &line : lines)
		{
			if (!inBlock)
			{
				if (line.find("!Begin") != std::string::npos)
					inBlock = true;
				continue;
			}

			std::vector<double> row;
			if (!TryParseNumberRow(line, row))
				continue;
			if (expectedCols != 0 && row.size() < expectedCols)
				continue;
			if (expectedCols != 0)
				row.resize(expectedCols);
			rows.push_back(std::move(row));
			if (maxRows != 0 && rows.size() >= maxRows)
				break;
		}
		return rows;
	}

	template <typename... Columns>
	static std::size_t MinColumnSize(const Columns &...columns)
	{
		return std::min({columns.size()...});
	}

	template <typename... Columns>
	static std::vector<std::vector<double>> ColumnsToRows(const Columns &...columns)
	{
		const std::size_t rowCount = MinColumnSize(columns...);
		std::vector<std::vector<double>> rows;
		rows.reserve(rowCount);
		for (std::size_t i = 0; i < rowCount; ++i)
			rows.push_back({columns[i]...});
		return rows;
	}

	template <typename... Columns>
	static void LoadColumns(const std::vector<std::vector<double>> &rows, Columns &...columns)
	{
		constexpr std::size_t count = sizeof...(Columns);
		((columns.clear()), ...);
		for (const auto &row : rows)
		{
			if (row.size() < count)
				continue;
			std::size_t index = 0;
			((columns.push_back(row[index++])), ...);
		}
	}

	static void AddRows(std::vector<std::string> &lines, const std::vector<std::vector<double>> &rows)
	{
		for (const auto &row : rows)
			lines.push_back(JoinNumbers(row));
	}

protected:
	void SetMode(Mode mode, Format format)
	{
		mode_ = mode;
		format_ = format;
	}

	void ReadOrWrite(const std::string &key, std::string &value) { Transfer(key, value); }
	void ReadOrWrite(const std::string &key, bool &value) { Transfer(key, value); }

	template <typename T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, int> = 0>
	void ReadOrWrite(const std::string &key, T &value)
	{
		Transfer(key, value);
	}

	template <typename E, std::enable_if_t<std::is_enum_v<E>, int> = 0>
	void ReadOrWrite(const std::string &key, E &value)
	{
		Transfer(key, value);
	}

	template <typename T>
	void ReadOrWriteAny(std::initializer_list<std::string> keys, T &value)
	{
		if (IsReadMode())
		{
			for (const auto &key : keys)
			{
				T candidate = value;
				if (TryParseLine(key, candidate))
				{
					value = std::move(candidate);
					return;
				}
			}
			return;
		}
		if (keys.size() > 0)
			ReadOrWrite(*keys.begin(), value);
	}

	template <typename T>
	void One(FieldRef<T> field)
	{
		ReadOrWrite(field.key, field.value);
	}

	template <typename... FieldsT>
	void Fields(FieldsT... fields)
	{
		(One(fields), ...);
	}

	virtual void SerializeFields() {}

private:
	struct InputLine
	{
		std::string raw;
		std::string valueToken;
		std::string key;
		std::string comment;
		bool wasQuoted = false;
		char quote = '"';
	};

	struct Token
	{
		std::string text;
		bool quoted = false;
		char quote = '"';
	};

	Mode mode_ = Mode::Read;
	Format format_ = Format::KeywordInput;
	bool valueFirst_ = false;
	std::string filePath_;
	std::string yamlRoot_;
	std::vector<InputLine> inputLines_;
	std::unordered_map<std::string, std::size_t> inputIndex_;
	std::unordered_map<std::string, std::string> yamlValues_;
	std::vector<std::string> yamlOrder_;

	static std::string NormalizeKey(const std::string &key)
	{
		return ZString::ToUpper(ZString::Trim(key));
	}

	std::string QualifyYamlKey(const std::string &key) const
	{
		if (yamlRoot_.empty() || key.empty())
			return key;
		const auto prefix = yamlRoot_ + ".";
		if (key == yamlRoot_ || ZString::StartsWith(key, prefix))
			return key;
		return prefix + key;
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
			if (valueFirst_)
			{
				if (tokens.size() >= 1)
				{
					line.valueToken = tokens[0].text;
					line.wasQuoted = tokens[0].quoted;
					line.quote = tokens[0].quote;
				}
				if (tokens.size() >= 2)
					line.key = tokens[1].text;
			}
			else
			{
				if (tokens.size() >= 1)
					line.key = tokens[0].text;
				if (tokens.size() >= 2)
				{
					line.valueToken = tokens[1].text;
					line.wasQuoted = tokens[1].quoted;
					line.quote = tokens[1].quote;
				}
			}

			line.comment = ExtractComment(raw);
			if (!line.key.empty())
				inputIndex_[NormalizeKey(line.key)] = inputLines_.size();
			inputLines_.push_back(std::move(line));
		}
	}

	std::vector<Token> TokenizeLine(const std::string &line) const
	{
		std::vector<Token> tokens;
		std::size_t i = 0;
		while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
			++i;
		if (i >= line.size() || line[i] == '#' || line[i] == '!' || (i + 1 < line.size() && line[i] == '-' && line[i + 1] == '-'))
			return tokens;

		while (i < line.size() && tokens.size() < 2)
		{
			while (i < line.size() && (std::isspace(static_cast<unsigned char>(line[i])) || line[i] == '=' || line[i] == ':'))
				++i;
			if (valueFirst_ && tokens.size() == 1 && i < line.size() && line[i] == '-')
			{
				++i;
				continue;
			}
			if (i >= line.size() || line[i] == '#' || line[i] == '!')
				break;
			if (line[i] == '-' && tokens.size() >= 1)
				break;

			Token token;
			if (line[i] == '"' || line[i] == '\'')
			{
				token.quoted = true;
				token.quote = line[i++];
				while (i < line.size() && line[i] != token.quote)
					token.text.push_back(line[i++]);
				if (i < line.size())
					++i;
			}
			else
			{
				while (i < line.size() &&
				       !std::isspace(static_cast<unsigned char>(line[i])) &&
				       line[i] != '=' && line[i] != ':' && line[i] != '#' && line[i] != '!')
				{
					if (line[i] == '-' && tokens.size() >= 1)
						break;
					token.text.push_back(line[i++]);
				}
			}
			if (!token.text.empty())
				tokens.push_back(std::move(token));
		}
		return tokens;
	}

	std::string ExtractComment(const std::string &line) const
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
				continue;
			}
			if (ch == '#' || ch == '!')
				return ZString::Trim(line.substr(i));
			if (valueFirst_ && ch == '-' && i > 0 && std::isspace(static_cast<unsigned char>(line[i - 1])))
				return ZString::Trim(line.substr(i));
		}
		return {};
	}

	std::string ReadValue(const std::string &key) const
	{
		return IsYaml() ? ReadYamlValue(key) : ReadKeywordToken(key);
	}

	std::string ReadKeywordToken(const std::string &key) const
	{
		const auto it = inputIndex_.find(NormalizeKey(key));
		return it == inputIndex_.end() ? std::string{} : inputLines_[it->second].valueToken;
	}

	std::string ReadYamlValue(const std::string &key) const
	{
		const auto it = yamlValues_.find(QualifyYamlKey(key));
		return it == yamlValues_.end() ? std::string{} : it->second;
	}

	template <typename T>
	void Transfer(const std::string &key, T &value)
	{
		if (IsReadMode())
		{
			ParseValue(ReadValue(key), value);
			return;
		}
		WriteValue(key, ToText(value));
	}

	void WriteValue(const std::string &key, const std::string &value)
	{
		if (IsYaml())
			WriteYamlValue(key, value);
		else
			WriteKeywordToken(key, value);
	}

	void WriteYamlValue(const std::string &key, const std::string &value)
	{
		const auto yamlKey = QualifyYamlKey(key);
		if (yamlValues_.find(yamlKey) == yamlValues_.end())
			yamlOrder_.push_back(yamlKey);
		yamlValues_[yamlKey] = value;
	}

	void WriteKeywordToken(const std::string &key, const std::string &value)
	{
		const auto normalized = NormalizeKey(key);
		auto it = inputIndex_.find(normalized);
		if (it == inputIndex_.end())
		{
			InputLine line;
			line.key = key;
			line.valueToken = value;
			line.raw = FormatKeywordLine(key, value, {});
			inputIndex_[normalized] = inputLines_.size();
			inputLines_.push_back(std::move(line));
			return;
		}

		auto &line = inputLines_[it->second];
		line.valueToken = value;
		line.raw = FormatKeywordLine(line.key.empty() ? key : line.key, value, line.comment);
	}

	std::string FormatKeywordLine(const std::string &key, const std::string &value, const std::string &comment) const
	{
		std::ostringstream out;
		if (valueFirst_)
			out << FormatKeywordValue(value) << "\t" << key;
		else
			out << key << " = " << FormatKeywordValue(value);
		if (!comment.empty())
			out << "  " << comment;
		return out.str();
	}

	static std::string FormatKeywordValue(const std::string &value)
	{
		const bool quote = value.empty() || value.find_first_of(" \t:=#!") != std::string::npos;
		return quote ? "\"" + value + "\"" : value;
	}

	static bool IsDefaultToken(const std::string &token)
	{
		return ZString::ToUpper(ZString::Trim(token)) == "DEFAULT";
	}

	static bool ParseBool(const std::string &token)
	{
		const auto upper = ZString::ToUpper(ZString::Trim(token));
		return upper == "TRUE" || upper == "TURE" || upper == "1" || upper == "YES" || upper == "ON";
	}

	template <typename T>
	static bool ParseValue(const std::string &token, T &value)
	{
		const auto text = ZString::Trim(token);
		if (text.empty() || IsDefaultToken(text))
			return false;

		try
		{
			if constexpr (std::is_same_v<T, std::string>)
			{
				value = text;
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				value = ParseBool(text);
			}
			else if constexpr (std::is_enum_v<T>)
			{
				try { value = ZString::StringToEnum<T>(text, true); }
				catch (...)
				{
					const auto numeric = ZString::StringTo<std::underlying_type_t<T>>(text);
					const auto parsed = magic_enum::enum_cast<T>(numeric);
					if (!parsed)
						return false;
					value = *parsed;
				}
			}
			else if constexpr (std::is_arithmetic_v<T>)
			{
				value = ZString::StringTo<T>(text);
			}
			else
			{
				return false;
			}
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	template <typename T>
	static std::string ToText(const T &value)
	{
		if constexpr (std::is_same_v<T, std::string>)
			return value;
		else if constexpr (std::is_same_v<T, bool>)
			return value ? "True" : "False";
		else if constexpr (std::is_enum_v<T>)
		{
			const auto name = magic_enum::enum_name(value);
			return name.empty() ? std::to_string(static_cast<std::underlying_type_t<T>>(value)) : std::string(name);
		}
		else
			return YML::ToYmlValueString(value);
	}

	Eigen::MatrixXd ParseYamlMatrix(const std::string &key) const
	{
		const auto token = ReadYamlValue(key);
		if (ZString::Trim(token).empty())
			return {};

		const auto rows = YML::YmlTo2DDoubleArray(token).data;
		if (rows.empty())
			return {};

		Eigen::MatrixXd matrix(static_cast<Eigen::Index>(rows.size()),
		                       static_cast<Eigen::Index>(rows.front().size()));
		for (Eigen::Index r = 0; r < matrix.rows(); ++r)
			for (Eigen::Index c = 0; c < matrix.cols(); ++c)
				matrix(r, c) = rows[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
		return matrix;
	}

	Eigen::MatrixXd ParseMatrixAtImpl(LineRef start, int rows, int cols) const
	{
		if (IsYaml())
			return ParseYamlMatrix(start.key);
		if (!start.Found())
			return {};
		if (rows < 0)
			throw std::runtime_error("Matrix row count cannot be negative for key " + start.key);

		std::vector<std::vector<double>> parsed;
		const int maxRows = rows;
		int actualCols = cols;
		for (std::size_t i = static_cast<std::size_t>(start.index);
		     i < inputLines_.size() && (maxRows == 0 || static_cast<int>(parsed.size()) < maxRows);
		     ++i)
		{
			std::vector<double> row;
			if (!TryParseNumberRow(inputLines_[i].raw, row))
				continue;
			if (actualCols <= 0)
				actualCols = static_cast<int>(row.size());
			if (static_cast<int>(row.size()) < actualCols)
				throw std::runtime_error("Invalid matrix row after key " + start.key);
			row.resize(static_cast<std::size_t>(actualCols));
			parsed.push_back(std::move(row));
		}

		if (rows != 0 && static_cast<int>(parsed.size()) != rows)
			throw std::runtime_error("Matrix row count after key " + start.key + " is smaller than expected");
		if (parsed.empty() || actualCols <= 0)
			return {};

		Eigen::MatrixXd matrix(static_cast<Eigen::Index>(parsed.size()), actualCols);
		for (Eigen::Index r = 0; r < matrix.rows(); ++r)
			for (Eigen::Index c = 0; c < matrix.cols(); ++c)
				matrix(r, c) = parsed[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
		return matrix;
	}

	static std::string TokenAtStation(const std::string &line, char fg, char fg1, int station)
	{
		std::vector<std::string> tokens;
		std::string token;
		for (const char ch : line)
		{
			if (ch == fg || ch == fg1 || ch == ',')
			{
				if (!token.empty())
				{
					tokens.push_back(token);
					token.clear();
				}
			}
			else
			{
				token.push_back(ch);
			}
		}
		if (!token.empty())
			tokens.push_back(token);
		if (station < 0 || station >= static_cast<int>(tokens.size()))
			return {};
		return ZString::Trim(tokens[static_cast<std::size_t>(station)]);
	}
};
