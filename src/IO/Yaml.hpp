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

namespace yml_detail
{
	inline std::string trimStart(const std::string &value)
	{
		auto it = std::find_if_not(value.begin(), value.end(), [](unsigned char ch)
								   { return std::isspace(ch) != 0; });
		return std::string(it, value.end());
	}

	inline std::string trimEnd(const std::string &value)
	{
		auto it = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch)
								   { return std::isspace(ch) != 0; });
		return std::string(value.begin(), it.base());
	}

	inline std::string trim(const std::string &value)
	{
		return trimEnd(trimStart(value));
	}

	inline std::string toLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
					   { return static_cast<char>(std::tolower(ch)); });
		return value;
	}

	inline bool startsWith(const std::string &value, const std::string &prefix)
	{
		return value.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), value.begin());
	}

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

	inline std::vector<std::string> splitScalarList(const std::string &value)
	{
		std::string normalized;
		normalized.reserve(value.size());
		for (char ch : value)
		{
			if (ch == '[' || ch == ']')
				continue;
			normalized.push_back(ch == '\t' ? ' ' : ch);
		}

		std::vector<std::string> result;
		for (const auto &part : split(normalized, ',', true))
		{
			std::string token = trim(part);
			if (token.empty())
				continue;

			std::istringstream whitespaceStream(token);
			std::vector<std::string> whitespaceTokens;
			std::string whitespaceToken;
			while (whitespaceStream >> whitespaceToken)
				whitespaceTokens.push_back(whitespaceToken);

			if (whitespaceTokens.size() > 1)
			{
				result.insert(result.end(), whitespaceTokens.begin(), whitespaceTokens.end());
			}
			else
			{
				result.push_back(token);
			}
		}
		return result;
	}

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

	template <typename T>
	inline std::string scalarToString(T value)
	{
		if constexpr (std::is_same_v<std::decay_t<T>, bool>)
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

class YMLConvertToObjectiveExtensions
{
public:
	inline static std::vector<std::string> ConvertValueType{};
	inline static std::vector<std::string> readlist{};

	static bool YmlToBool(const std::string &yml, const std::string &name = "", bool moren = false)
	{
		(void)name;
		const bool result = yml_detail::parseBool(yml, moren);
		ConvertValueType.emplace_back("bool");
		return result;
	}

	static std::vector<bool> YmlToBoolArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		std::vector<bool> result;
		for (const auto &token : yml_detail::splitScalarList(yml))
			result.push_back(yml_detail::parseBool(token));
		ConvertValueType.emplace_back("std::vector<bool>");
		return result;
	}

	static int YmlToInt(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		const int result = yml_detail::parseNumber<int>(yml, "int");
		ConvertValueType.emplace_back("int");
		return result;
	}

	static std::vector<int> YmlToIntArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		std::vector<int> result;
		for (const auto &token : yml_detail::splitScalarList(yml))
			result.push_back(yml_detail::parseNumber<int>(token, "int"));
		ConvertValueType.emplace_back("std::vector<int>");
		return result;
	}

	static double YmlToDouble(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		const double result = yml_detail::parseNumber<double>(yml, "double");
		ConvertValueType.emplace_back("double");
		return result;
	}

	static std::vector<double> YmlToDoubleArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		std::vector<double> result;
		for (const auto &token : yml_detail::splitScalarList(yml))
			result.push_back(yml_detail::parseNumber<double>(token, "double"));
		ConvertValueType.emplace_back("std::vector<double>");
		return result;
	}

	static double YmlToFloat(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		const float result = yml_detail::parseNumber<float>(yml, "float");
		ConvertValueType.emplace_back("float");
		return result;
	}

	static std::vector<float> YmlToFloatArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		std::vector<float> result;
		for (const auto &token : yml_detail::splitScalarList(yml))
			result.push_back(yml_detail::parseNumber<float>(token, "float"));
		ConvertValueType.emplace_back("std::vector<float>");
		return result;
	}

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

	static std::string YmlToString(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		ConvertValueType.emplace_back(yml.empty() ? "Null" : "std::string");
		return yml;
	}

	static std::vector<std::string> YmlToStringArray(const std::string &yml, const std::string &name = "")
	{
		(void)name;
		std::vector<std::string> result;
		for (const auto &token : yml_detail::splitScalarList(yml))
			result.push_back(token);
		ConvertValueType.emplace_back("std::vector<std::string>");
		return result;
	}

	struct DoubleArray2D
	{
		std::vector<std::vector<double>> data;
		int RowCount = 0;
		int ColumnCount = 0;
	};

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

	struct MatrixResult
	{
		Eigen::MatrixXd data;
		int RowCount = 0;
		int ColumnCount = 0;
	};

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

class YML
{
public:
	inline static constexpr const char *ymlversion = "2.0.016";

	struct Node
	{
		std::string name;
		std::optional<std::string> value;
		std::shared_ptr<Node> parent;
		int space = 0;
		int tier = 0;

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

	YML() = default;

	explicit YML(const std::string &filePath, bool addMetadata = true)
	{
		Load(filePath, addMetadata);
	}

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

	const std::string &Path() const
	{
		return path;
	}

	void Load(const std::string &filePath, bool addMetadata = true)
	{
		path = filePath;
		lines.clear();

		if (std::filesystem::is_regular_file(std::filesystem::path(filePath)))
		{
			std::ifstream file(filePath);
			if (!file.is_open())
				throw std::runtime_error("Cannot open yaml file: " + filePath);

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

	void modify(const std::string &key, const std::string &value)
	{
		modifyOptional(key, std::optional<std::string>(value));
	}

	void modify(const std::string &key, std::nullopt_t)
	{
		modifyOptional(key, std::nullopt);
	}

	void modifyOptional(const std::string &key, std::optional<std::string> value)
	{
		Node *node = findNodeByKey(key);
		if (node != nullptr)
			node->value = std::move(value);
	}

	std::string read(const std::string &key) const
	{
		YMLConvertToObjectiveExtensions::readlist.push_back(key);
		const Node *node = findNodeByKey(key);
		if (node == nullptr || !node->value)
			return {};
		return *node->value;
	}

	std::optional<std::string> readOptional(const std::string &key) const
	{
		YMLConvertToObjectiveExtensions::readlist.push_back(key);
		const Node *node = findNodeByKey(key);
		if (node == nullptr)
			return std::nullopt;
		return node->value;
	}

	Node *findNodeByKey(const std::string &key)
	{
		return findNodePtrByKey(key).get();
	}

	const Node *findNodeByKey(const std::string &key) const
	{
		return findNodePtrByKey(key).get();
	}

	bool ChickfindNodeByKey(const std::string &key) const
	{
		return findNodeByKey(key) != nullptr;
	}

	bool CheckFindNodeByKey(const std::string &key) const
	{
		return ChickfindNodeByKey(key);
	}

	std::string GetNodeKey(const Node *node) const
	{
		if (node == nullptr)
			return {};
		if (!node->parent)
			return node->name;
		return GetNodeKey(node->parent.get()) + "." + node->name;
	}

	std::string GetNodeKey(const NodePtr &node) const
	{
		return GetNodeKey(node.get());
	}

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

	void AddNode(const std::string &key, const std::string &value)
	{
		addNodeOptional(key, std::optional<std::string>(value));
	}

	void AddNode(const std::string &key, std::nullopt_t)
	{
		addNodeOptional(key, std::nullopt);
	}

	void AddNode(const std::string &parentKey, const std::string &nodeName, const std::string &value)
	{
		addNodeUnderParent(parentKey, nodeName, std::optional<std::string>(value));
	}

	void AddNode(const std::string &parentKey, const std::string &nodeName, std::nullopt_t)
	{
		addNodeUnderParent(parentKey, nodeName, std::nullopt);
	}

private:
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

	void save(const std::string &savepath = std::string{}, bool format = true)
	{
		std::string target = savepath.empty() ? path : savepath;
		if (target.empty())
			throw std::runtime_error("Cannot save yaml because both savepath and path are empty.");

		if (format)
			formatting();

		const std::filesystem::path filePath(target);
		const auto parentPath = filePath.parent_path();
		if (!parentPath.empty())
			std::filesystem::create_directories(parentPath);

		std::ofstream file(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file.is_open())
			throw std::runtime_error("Cannot write yaml file: " + target);

		for (const auto &node : nodeList)
		{
			file << std::string(static_cast<size_t>(node->tier) * 2, ' ') << node->name << ": ";
			if (node->value)
				file << *node->value;
			file << '\n';
		}
	}

	static bool YmlToBool(const std::string &yml, const std::string &name = "", bool moren = false)
	{
		return YMLConvertToObjectiveExtensions::YmlToBool(yml, name, moren);
	}

	static std::vector<bool> YmlToBoolArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToBoolArray(yml, name);
	}

	static int YmlToInt(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToInt(yml, name);
	}

	static std::vector<int> YmlToIntArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToIntArray(yml, name);
	}

	static double YmlToDouble(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToDouble(yml, name);
	}

	static std::vector<double> YmlToDoubleArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToDoubleArray(yml, name);
	}

	static double YmlToFloat(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToFloat(yml, name);
	}

	static std::vector<float> YmlToFloatArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToFloatArray(yml, name);
	}

	static Eigen::VectorXd YmlToVector(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToVector(yml, name);
	}

	static std::string YmlToString(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToString(yml, name);
	}

	static std::vector<std::string> YmlToStringArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlToStringArray(yml, name);
	}

	static YMLConvertToObjectiveExtensions::DoubleArray2D YmlTo2DDoubleArray(const std::string &yml, const std::string &name = "")
	{
		return YMLConvertToObjectiveExtensions::YmlTo2DDoubleArray(yml, name);
	}

	static YMLConvertToObjectiveExtensions::MatrixResult YmlToMatrix(const std::string &yml, const std::string &name = "", bool add = true)
	{
		return YMLConvertToObjectiveExtensions::YmlToMatrix(yml, name, add);
	}

	template <typename E>
	static E YmlToEnum(const std::string &yml, bool ignoreCase = true)
	{
		return YMLConvertToObjectiveExtensions::YmlToEnum<E>(yml, ignoreCase);
	}

	static std::string ToYmlValueString(const std::string &value, int level = 0)
	{
		(void)level;
		return yml_detail::trim(value);
	}

	static std::string ToYmlValueString(const char *value, int level = 0)
	{
		(void)level;
		return value == nullptr ? std::string{} : yml_detail::trim(value);
	}

	static std::string ToYmlValueString(bool value, int level = 0)
	{
		(void)level;
		return yml_detail::scalarToString(value);
	}

	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
	static std::string ToYmlValueString(T value, int level = 0)
	{
		(void)level;
		return yml_detail::scalarToString(value);
	}

	template <typename E, typename = std::enable_if_t<std::is_enum_v<E>>, typename = void>
	static std::string ToYmlValueString(E value, int level = 0)
	{
		(void)level;
		return yml_detail::scalarToString(value);
	}

	template <typename T>
	static std::string ToYmlValueString(const std::vector<T> &values, int level = 0)
	{
		(void)level;
		return yml_detail::arrayToString(values);
	}

	template <typename T, size_t N>
	static std::string ToYmlValueString(const std::array<T, N> &values, int level = 0)
	{
		(void)level;
		return yml_detail::arrayToString(values);
	}

	template <typename T>
	static std::string ToYmlValueString(std::initializer_list<T> values, int level = 0)
	{
		(void)level;
		return yml_detail::arrayToString(std::vector<T>(values));
	}

	template <typename T>
	static std::string ToYmlValueString(const std::vector<std::vector<T>> &data, int level = 0)
	{
		std::ostringstream stream;
		stream << '\n';
		const std::string indent(static_cast<size_t>(level) * 2 + 1, ' ');
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
		const std::string indent(static_cast<size_t>(level) * 2 + 1, ' ');
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

	void parseLines(const std::vector<std::string> &sourceLines)
	{
		nodeList.clear();
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

			node->parent = findParent(node->space);
			if (node->parent)
				node->tier = node->parent->tier + 1;
			nodeList.push_back(std::move(node));
		}
	}

	NodePtr findNodePtrByKey(const std::string &key)
	{
		return constFindNodePtrByKey(key);
	}

	NodePtr findNodePtrByKey(const std::string &key) const
	{
		return constFindNodePtrByKey(key);
	}

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

	NodePtr findParent(int space) const
	{
		for (auto it = nodeList.rbegin(); it != nodeList.rend(); ++it)
		{
			if ((*it)->space < space)
				return *it;
		}
		return nullptr;
	}

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

	void formatting()
	{
		std::vector<NodePtr> roots;
		for (const auto &node : nodeList)
		{
			if (!node->parent)
				roots.push_back(node);
		}

		std::vector<NodePtr> formatted;
		formatted.reserve(nodeList.size());
		for (const auto &root : roots)
		{
			root->tier = 0;
			root->space = 0;
			formatted.push_back(root);
			appendChildren(root, formatted);
		}
		nodeList = std::move(formatted);
	}

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

	void ensureMetadata()
	{
		if (!ChickfindNodeByKey("OpenWECD.Information.YMLVersion"))
			AddNode("OpenWECD.Information.YMLVersion", ymlversion);
		AddNode("OpenWECD.Information.Auther", "YML module by Zhao Zizhen @copyright");
		AddNode("OpenWECD.Information.LastModifiedTime", yml_detail::nowString());
	}
};

using Yaml = YML;
