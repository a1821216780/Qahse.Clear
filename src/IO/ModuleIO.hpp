#pragma once

#include <Eigen/Dense>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "IO/Serializer.hpp"
#include "IO/ZFile.hpp"
#include "IO/ZPath.hpp"
#include "IO/ZString.hpp"

struct NodeOutputConfig
{
	int count = 0;
	std::vector<int> nodes;
};

struct OutputConfig
{
	bool sumPrint = true;
	bool afSpanput = false;
	std::vector<int> bldOutSig{0};
	std::string sumPath;
	NodeOutputConfig blade;
	NodeOutputConfig tower;
	std::vector<std::string> outList;

	double dtOut = 0.0;
	double tStart = 0.0;
	int outType = 0;
};

namespace module_io
{
inline std::vector<std::string> ReadYamlStringArray(const std::string &path,
                                                    const std::string &root,
                                                    const std::string &key);
inline std::vector<int> ReadYamlIntArray(const std::string &path,
                                         const std::string &root,
                                         const std::string &key);
inline std::vector<double> ReadYamlDoubleArray(const std::string &path,
                                               const std::string &root,
                                               const std::string &key);
inline std::vector<std::vector<std::string>> ReadYamlRows(const std::string &path,
                                                          const std::string &root,
                                                          const std::string &key);
inline void ResolveOutputPath(const std::string &baseFile, OutputConfig &output);

inline std::string TrimQuotes(std::string text)
{
	text = ZString::Trim(text);
	if (text.size() >= 2 &&
	    ((text.front() == '"' && text.back() == '"') ||
	     (text.front() == '\'' && text.back() == '\'')))
	{
		return text.substr(1, text.size() - 2);
	}
	return text;
}

inline std::string StripInlineComment(const std::string &line)
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
			return line.substr(0, i);
		if (ch == '/' && i + 1 < line.size() && line[i + 1] == '/')
			return line.substr(0, i);
		if (ch == '-' && i + 1 < line.size() &&
		    i > 0 &&
		    std::isspace(static_cast<unsigned char>(line[i - 1])) &&
		    std::isspace(static_cast<unsigned char>(line[i + 1])))
			return line.substr(0, i);
	}
	return line;
}

inline bool IsCommentOrEmpty(const std::string &line)
{
	const std::string text = ZString::Trim(line);
	return text.empty() ||
	       text[0] == '#' ||
	       text[0] == '!' ||
	       (text.size() >= 2 && text[0] == '-' && text[1] == '-');
}

inline bool IsSectionBoundary(const std::string &line)
{
	const std::string text = ZString::Trim(line);
	if (text.empty())
		return false;
	const auto upper = ZString::ToUpper(text);
	if (upper == "END" || ZString::StartsWith(upper, "END "))
		return true;
	if (text.find("======") != std::string::npos)
		return true;
	if (text.find("------") != std::string::npos)
		return true;
	return false;
}

inline std::vector<std::string> TokenizeLoose(const std::string &line)
{
	std::vector<std::string> tokens;
	std::string token;
	bool inQuote = false;
	char quote = '"';

	auto flush = [&]() {
		token = ZString::Trim(token);
		if (!token.empty())
			tokens.push_back(TrimQuotes(token));
		token.clear();
	};

	for (char ch : StripInlineComment(line))
	{
		if (inQuote)
		{
			if (ch == quote)
			{
				inQuote = false;
				flush();
			}
			else
			{
				token.push_back(ch);
			}
			continue;
		}

		if (ch == '"' || ch == '\'')
		{
			inQuote = true;
			quote = ch;
			flush();
			continue;
		}
		if (ch == ',' || std::isspace(static_cast<unsigned char>(ch)))
		{
			flush();
			continue;
		}
		token.push_back(ch);
	}
	flush();
	return tokens;
}

inline std::vector<int> ParseIntList(const std::string &line)
{
	std::vector<int> values;
	for (const auto &token : TokenizeLoose(line))
	{
		try
		{
			values.push_back(std::stoi(token));
		}
		catch (...)
		{
		}
	}
	return values;
}

inline std::vector<double> ParseDoubleList(const std::string &line)
{
	std::vector<double> values;
	for (const auto &token : TokenizeLoose(line))
	{
		try
		{
			values.push_back(std::stod(token));
		}
		catch (...)
		{
		}
	}
	return values;
}

inline std::string ReadAnyRaw(const Serializer &reader, std::initializer_list<std::string> keys)
{
	for (const auto &key : keys)
	{
		const std::string value = reader.read(key);
		if (!ZString::Trim(value).empty())
			return value;
	}
	return {};
}

inline std::vector<int> ReadIntList(const Serializer &reader,
                                    const std::string &path,
                                    const std::string &root,
                                    std::initializer_list<std::string> keys)
{
	if (reader.IsYaml())
	{
		for (const auto &key : keys)
		{
			auto values = ReadYamlIntArray(path, root, key);
			if (!values.empty())
				return values;
		}
	}
	return ParseIntList(ReadAnyRaw(reader, keys));
}

inline std::vector<double> ReadDoubleList(const Serializer &reader,
                                          const std::string &path,
                                          const std::string &root,
                                          std::initializer_list<std::string> keys)
{
	if (reader.IsYaml())
	{
		for (const auto &key : keys)
		{
			auto values = ReadYamlDoubleArray(path, root, key);
			if (!values.empty())
				return values;
		}
	}
	return ParseDoubleList(ReadAnyRaw(reader, keys));
}

inline std::string JoinIntList(const std::vector<int> &values)
{
	std::ostringstream out;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		if (i != 0)
			out << ",";
		out << values[i];
	}
	return out.str();
}

inline std::string JoinStringRow(const std::vector<std::string> &values)
{
	std::ostringstream out;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		if (i != 0)
			out << "\t";
		out << values[i];
	}
	return out.str();
}

inline std::vector<std::string> JoinRows(const std::vector<std::vector<std::string>> &rows)
{
	std::vector<std::string> result;
	result.reserve(rows.size());
	for (const auto &row : rows)
		result.push_back(JoinStringRow(row));
	return result;
}

inline int FindLineContaining(const std::vector<std::string> &lines, const std::string &key)
{
	const std::string needle = ZString::ToUpper(key);
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		if (ZString::ToUpper(lines[i]).find(needle) != std::string::npos)
			return static_cast<int>(i);
	}
	return -1;
}

inline std::string FirstTokenAfterKey(const Serializer &reader, const std::string &key)
{
	const auto lines = reader.RawLines();
	const int index = FindLineContaining(lines, key);
	if (index < 0)
		return {};
	const auto tokens = TokenizeLoose(lines[static_cast<std::size_t>(index)]);
	if (tokens.empty())
		return {};
	return tokens.front();
}

inline std::vector<std::string> ReadOutputListAfter(const Serializer &reader,
                                                    const std::string &key = "OutList")
{
	const auto lines = reader.RawLines();
	const int index = FindLineContaining(lines, key);
	if (index < 0)
		return {};

	std::vector<std::string> out;
	for (std::size_t i = static_cast<std::size_t>(index + 1); i < lines.size(); ++i)
	{
		const std::string text = ZString::Trim(lines[i]);
		if (IsSectionBoundary(text))
			break;
		if (IsCommentOrEmpty(text))
			continue;
		for (auto token : TokenizeLoose(text))
		{
			token = ZString::Trim(token);
			if (!token.empty())
				out.push_back(token);
		}
	}
	return out;
}

inline std::vector<std::string> ReadOutputList(const Serializer &reader,
                                               const std::string &path,
                                               const std::string &root)
{
	if (reader.IsYaml())
	{
		auto values = ReadYamlStringArray(path, root, "OutList");
		if (!values.empty())
			return values;
		values = ReadYamlStringArray(path, root, "Outputs_OutList");
		if (!values.empty())
			return values;
	}
	return ReadOutputListAfter(reader, "OutList");
}

inline std::vector<std::vector<std::string>> ReadTableAfter(const Serializer &reader,
                                                            const std::string &key,
                                                            std::size_t minColumns = 1)
{
	const auto lines = reader.RawLines();
	const int index = FindLineContaining(lines, key);
	if (index < 0)
		return {};

	std::vector<std::vector<std::string>> rows;
	for (std::size_t i = static_cast<std::size_t>(index + 1); i < lines.size(); ++i)
	{
		const std::string text = ZString::Trim(lines[i]);
		if (IsSectionBoundary(text))
			break;
		if (IsCommentOrEmpty(text))
			continue;

		auto tokens = TokenizeLoose(text);
		if (tokens.size() < minColumns)
			continue;
		if (!tokens.empty())
		{
			try
			{
				(void)std::stod(tokens.front());
			}
			catch (...)
			{
				continue;
			}
		}
		rows.push_back(std::move(tokens));
	}
	return rows;
}

inline Eigen::MatrixXd ReadNumericTableAfter(const Serializer &reader,
                                             const std::string &key,
                                             std::size_t minColumns = 1)
{
	const auto stringRows = ReadTableAfter(reader, key, minColumns);
	if (stringRows.empty())
		return {};

	std::size_t cols = 0;
	for (const auto &row : stringRows)
		cols = std::max(cols, row.size());
	Eigen::MatrixXd matrix(static_cast<Eigen::Index>(stringRows.size()),
	                       static_cast<Eigen::Index>(cols));
	matrix.setZero();
	for (Eigen::Index r = 0; r < matrix.rows(); ++r)
	{
		const auto &row = stringRows[static_cast<std::size_t>(r)];
		for (Eigen::Index c = 0; c < matrix.cols() && static_cast<std::size_t>(c) < row.size(); ++c)
		{
			try
			{
				matrix(r, c) = std::stod(row[static_cast<std::size_t>(c)]);
			}
			catch (...)
			{
				matrix(r, c) = 0.0;
			}
		}
	}
	return matrix;
}

inline std::vector<std::vector<std::string>> ReadRows(const Serializer &reader,
                                                      const std::string &path,
                                                      const std::string &root,
                                                      const std::string &key,
                                                      std::size_t minColumns = 1)
{
	if (reader.IsYaml())
		return ReadYamlRows(path, root, key);
	return ReadTableAfter(reader, key, minColumns);
}

inline Eigen::MatrixXd ReadMatrixBlock(const Serializer &reader,
                                       const std::string &path,
                                       const std::string &root,
                                       const std::string &key,
                                       std::size_t minColumns = 1)
{
	if (reader.IsYaml())
		return reader.ReadMatrix(key);
	return ReadNumericTableAfter(reader, key, minColumns);
}

inline std::string ResolvePath(const std::string &baseFile, const std::string &path)
{
	if (path.empty())
		return {};
	return ZPath::ResolvePath(baseFile, path);
}

inline void ResolveIfSet(const std::string &baseFile, std::string &path)
{
	if (!path.empty())
		path = ResolvePath(baseFile, path);
}

inline void ResolveIfSetWithExtension(const std::string &baseFile,
                                      std::string &path,
                                      const std::string &extension)
{
	if (path.empty())
		return;
	auto resolved = std::filesystem::path(ResolvePath(baseFile, path));
	if (!extension.empty() && !resolved.has_extension())
	{
		auto withExt = resolved;
		withExt += extension;
		if (std::filesystem::exists(withExt))
			resolved = withExt;
	}
	path = resolved.lexically_normal().string();
}

inline void TrimNodeOutput(NodeOutputConfig &config)
{
	if (config.count <= 0)
	{
		config.count = 0;
		config.nodes.clear();
		return;
	}
	if (config.nodes.size() > static_cast<std::size_t>(config.count))
		config.nodes.resize(static_cast<std::size_t>(config.count));
	if (config.nodes.size() < static_cast<std::size_t>(config.count))
		throw std::runtime_error("Output node count is larger than the node list length");
}

inline void FinalizeOutputConfig(OutputConfig &output)
{
	TrimNodeOutput(output.blade);
	TrimNodeOutput(output.tower);
	if (output.bldOutSig.empty())
		output.bldOutSig.push_back(0);
}

inline void ReadOutputConfig(Serializer &reader,
                             const std::string &path,
                             const std::string &root,
                             OutputConfig &output)
{
	output.sumPrint = reader.ReadAny<bool>({"SumPrint"}, output.sumPrint);
	output.afSpanput = reader.ReadAny<bool>({"AfSpanput", "AFSpanput", "AFSpanPut"}, output.afSpanput);
	output.sumPath = reader.ReadAny<std::string>({"SumPath"}, output.sumPath);
	output.dtOut = reader.ReadAny<double>({"DT_Out", "DtOut", "DTOut"}, output.dtOut);
	output.tStart = reader.ReadAny<double>({"TStart"}, output.tStart);
	output.outType = reader.ReadAny<int>({"OutType"}, output.outType);
	output.blade.count = reader.ReadAny<int>({"NBlOuts", "NumBladeOutNodes"}, output.blade.count);
	output.tower.count = reader.ReadAny<int>({"NTwOuts", "NumTowerOutNodes"}, output.tower.count);

	auto bldOutSig = ReadIntList(reader, path, root, {"BldOutSig"});
	if (!bldOutSig.empty())
		output.bldOutSig = std::move(bldOutSig);
	output.blade.nodes = ReadIntList(reader, path, root, {"BlOutNd", "BladeOutNodes"});
	output.tower.nodes = ReadIntList(reader, path, root, {"TwOutNd", "TowerOutNodes"});
	output.outList = ReadOutputList(reader, path, root);
	FinalizeOutputConfig(output);
	ResolveOutputPath(path, output);
}

inline void AddOutputYamlNodes(Serializer &writer, const OutputConfig &output)
{
	writer.AddNode("BldOutSig", output.bldOutSig);
	writer.AddNode("NBlOuts", output.blade.count);
	writer.AddNode("BlOutNd", output.blade.nodes);
	writer.AddNode("NTwOuts", output.tower.count);
	writer.AddNode("TwOutNd", output.tower.nodes);
	writer.AddNode("OutList", output.outList);
}

inline void ResolveOutputPath(const std::string &baseFile, OutputConfig &output)
{
	ResolveIfSet(baseFile, output.sumPath);
}

inline bool FileExistsOrEmpty(const std::string &path)
{
	return path.empty() || std::filesystem::exists(std::filesystem::path(path));
}

inline std::string QualifyYamlKey(const std::string &root, const std::string &key)
{
	if (root.empty() || key == root || ZString::StartsWith(key, root + "."))
		return key;
	return root + "." + key;
}

inline std::vector<std::string> ReadYamlStringArray(const std::string &path,
                                                    const std::string &root,
                                                    const std::string &key)
{
	if (!Serializer::IsYamlPath(path) || !ZFile::Exists(path))
		return {};
	YML yaml(path);
	return YML::YmlToStringArray(yaml.read(QualifyYamlKey(root, key)));
}

inline std::vector<int> ReadYamlIntArray(const std::string &path,
                                         const std::string &root,
                                         const std::string &key)
{
	if (!Serializer::IsYamlPath(path) || !ZFile::Exists(path))
		return {};
	YML yaml(path);
	return YML::YmlToIntArray(yaml.read(QualifyYamlKey(root, key)));
}

inline std::vector<double> ReadYamlDoubleArray(const std::string &path,
                                               const std::string &root,
                                               const std::string &key)
{
	if (!Serializer::IsYamlPath(path) || !ZFile::Exists(path))
		return {};
	YML yaml(path);
	return YML::YmlToDoubleArray(yaml.read(QualifyYamlKey(root, key)));
}

inline std::vector<std::vector<std::string>> ReadYamlRows(const std::string &path,
                                                          const std::string &root,
                                                          const std::string &key)
{
	std::vector<std::vector<std::string>> rows;
	if (!Serializer::IsYamlPath(path) || !ZFile::Exists(path))
		return rows;

	YML yaml(path);
	const std::string value = yaml.read(QualifyYamlKey(root, key));
	if (ZString::Trim(value).empty())
		return rows;

	for (const auto &rowText : yml_detail::splitMatrixRows(value))
	{
		std::vector<std::string> row;
		for (auto token : yml_detail::splitScalarList(rowText))
		{
			token = TrimQuotes(ZString::Trim(token));
			if (!token.empty())
				row.push_back(std::move(token));
		}
		if (!row.empty())
			rows.push_back(std::move(row));
	}

	if (!rows.empty())
		return rows;

	for (const auto &line : ReadYamlStringArray(path, root, key))
	{
		auto row = TokenizeLoose(line);
		if (!row.empty())
			rows.push_back(std::move(row));
	}
	return rows;
}
} // namespace module_io
