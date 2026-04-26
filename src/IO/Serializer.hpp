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
// 该文件提供一个面向文本、YAML 和二进制文件的通用序列化基类，支持多种持久化格式和
// 模式，适合配置文件和数据交换使用。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <Eigen/Dense>

#include "IO/ZPath.hpp"
#include "IO/LogHelper.h"
#include "IO/ZString.hpp"

/**
 * @brief 面向文本、YAML 和二进制文件的通用序列化基类。
 *
 * Serializer 通过统一的 ReadOrWrite() 接口，把派生类成员映射到具体的存储格式，
 * 并支持以下场景：
 * - 关键字输入格式：保留原始行、注释与布局，适合人工编辑的配置文件。
 * - YAML：按层级键值方式读写，便于与外部工具互操作。
 * - 二进制：以带版本标识的紧凑格式保存结构化数据。
 *
 * 派生类只需要重写 SerializeFields()，并在其中对需要持久化的成员调用 ReadOrWrite()。
 *
 * \code
 * class DemoSerializer : public Serializer
 * {
 * public:
 *     int version = 0;
 *     std::string name;
 *
 *     void SerializeFields() override
 *     {
 *         ReadOrWrite("version", version);
 *         ReadOrWrite("name", name);
 *     }
 * };
 *
 * DemoSerializer serializer;
 * serializer.ReadTextFile("demo.qwd");
 * serializer.WriteYamlFile("demo.yaml");
 * \endcode
 */
class Serializer
{
public:
#pragma region 定义基本的类型
    /**
     * @brief 输入/输出模式。
     */
    enum class Mode
    {
        /// @brief 读取模式，表示正在从文件加载数据。
        Read,
        /// @brief 写入模式，表示正在准备数据以保存到文件。
        Write
    };

    /**
     * @brief 当前使用的持久化格式。
     */
    enum class Format
    {
        /// @brief 文本格式，保留原始行、注释与布局，适合人工编辑的配置文件。
        KeywordInput,
        /// @brief YAML 格式，按层级键值方式读写，便于与外部工具互操作。
        Yaml,
    };

    /// @brief 获取当前模式。
    /// @return 当前模式。
    Mode GetMode() const
    {
        return mode;
    }

protected:
    /**
     * @brief 切换内部模式和格式。
     * @param nextMode 目标模式。
     * @param nextFormat 目标格式。
     */
    void SetMode(Mode nextMode, Format nextFormat)
    {
        mode = nextMode;
        format = nextFormat;
    }

    /// @brief 判断当前是否处于读取模式。
    /// @return 如果当前是读取模式则返回 true，否则返回 false。
    bool IsReadMode() const
    {
        return mode == Mode::Read;
    }

    /// @brief 获取当前格式。
    /// @return 当前持久化格式。
    Format GetFormat() const
    {
        return format;
    }

    /**
     * @brief 关键字输入文件中的原始行信息。用来在序列化时进行精确的行级读写，保留注释和布局。
     *
     * 该结构会保留原始文本、解析出的键值、注释以及参数标记，便于写回时保持
     * 原始文件的布局和注释风格。
     */
    struct InputLine
    {
        /// @brief 原始整行文本。
        std::string raw;
        /// @brief 解析出的值 token。
        std::string valueToken;
        /// @brief 参数名。
        std::string key;
        /// @brief 行尾注释。
        std::string comment;
        /// @brief 是否是有效参数行。
        bool isParameter = false;
        /// @brief 读取时该值是否带引号。
        bool wasQuoted = false;
        /// @brief 原始引号字符。
        char quote = '"';

        /// 对于一些高级的格式，例如多行矩阵或者多行向量其一般文本格式非常复杂，
        /// 甚至可能包含多行，因此在解析时会将这些行合并成一个 InputLine 来处理，以便在写回时保持原始格式。
    };

private:

    /**
     * @brief 文本分词结果。
     * @note 用于解析关键字输入文件并保留 token 的原始跨度。
     */
    struct Token
    {
        /// @brief token 文本。
        std::string text;
        /// @brief token 起始位置。
        std::size_t begin = 0;
        /// @brief token 结束位置。
        std::size_t end = 0;
        /// @brief token 是否被引号包裹。
        bool quoted = false;
        /// @brief 引号字符。
        char quote = '"';
    };

#pragma endregion 定义基本的类型

#pragma region 类当中的局部变量

private:
    /// @brief 当前运行模式。
    Mode mode = Mode::Read;
    /// @brief 当前序列化格式。
    Format format = Format::KeywordInput;
    /// @brief 关键字输入文件的解析结果。
    std::vector<InputLine> inputLines;
    /// @brief 关键字到输入行索引的映射。
    std::unordered_map<std::string, std::size_t> inputIndex;
    /// @brief YAML 键值缓存，键为完整层级路径。
    std::unordered_map<std::string, std::string> yamlValues;
    /// @brief YAML 输出顺序。
    std::vector<std::string> yamlOrder;
    /// @brief 二进制输出顺序。
    std::vector<std::string> binaryOrder;

#pragma endregion 类当中的局部变量

#pragma region 定义构造和析构函数

    /// @brief 构造一个默认以关键字输入格式工作的序列化器。
    Serializer() = delete; // default;

    /// @brief 直接定义一个构造函数，允许在创建时指定文件路径和格式
    /// @param filePath 输入或输出文件路径；如果不为空，则会根据 fmt 自动调用对应的加载或保存方法。
    /// @param fmt 指定的输出格式；根据该格式会调用对应的加载或保存方法。
    Serializer(const std::string &filePath, const Format &fmt)
    {

        this->mode = Mode::Read; // 先设置为读取模式以便调用加载方法
        this->format = fmt;      // 设置格式后调用对应的加载方法
    }

#pragma endregion 定义构造和析构函数


    /**
     * @brief 查找关键字在输入文件中的行号。
     * @param key 要查找的字段名。
     * @return 1-based 行号；如果未找到则返回 -1。
     */
    int FindKeywordLine(const std::string &key) const
    {
        const auto it = inputIndex.find(key);
        if (it == inputIndex.end())
            return -1;
        return static_cast<int>(it->second + 1);
    }

    /**
     * @brief 判断输入文件中是否存在指定关键字。
     * @param key 要查找的字段名。
     * @return 如果存在则返回 true，否则返回 false。
     */
    bool HasKeyword(const std::string &key) const
    {
        return inputIndex.find(key) != inputIndex.end();
    }

    /**
     * @brief 获取当前解析得到的关键字输入行。
     * @return 解析后的输入行只读引用。
     */
    const std::vector<InputLine> &KeywordLines() const
    {
        return inputLines;
    }

    /**
     * @brief 按当前格式读写一个字段。
     * @tparam T 字段类型，支持 string、bool、整型、浮点型和枚举。
     * @param key 字段名。
     * @param value 读取时接收结果，写入时提供当前值。
     */
    template <typename T>
    void ReadOrWrite(const std::string &key, T &value)
    {
        switch (format)
        {
        case Format::KeywordInput:
            ReadOrWriteKeywordValue(key, value);
            break;
        case Format::Yaml:
            ReadOrWriteYamlValue(key, value);
            break;
        case Format::Binary:
            ReadOrWriteBinaryValue(key, value);
            break;
        default:
            LogHelper::ErrorLog("Unsupported format in ReadOrWrite.", "", "", 20, "Serializer::ReadOrWrite");
        }
    }


private:
    static std::vector<Token> TokenizeWithSpans(const std::string &line)
    {
        std::vector<Token> tokens;
        std::size_t i = 0;
        while (i < line.size())
        {
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0)
                ++i;
            if (i >= line.size())
                break;

            Token token;
            token.begin = i;
            if (line[i] == '"' || line[i] == '\'')
            {
                token.quoted = true;
                token.quote = line[i];
                const char quote = line[i++];
                while (i < line.size() && line[i] != quote)
                    ++i;
                if (i >= line.size())
                    throw std::runtime_error("Unclosed quoted token in line: " + line);
                ++i;
                token.end = i;
                token.text = line.substr(token.begin, token.end - token.begin);
            }
            else
            {
                while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) == 0)
                    ++i;
                token.end = i;
                token.text = line.substr(token.begin, token.end - token.begin);
            }
            tokens.push_back(std::move(token));
        }
        return tokens;
    }

    static std::size_t FindInlineComment(const std::string &line)
    {
        std::size_t cut = std::string::npos;
        bool quoted = false;
        char quote = '\0';
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            const char ch = line[i];
            if (quoted)
            {
                if (ch == quote)
                    quoted = false;
                continue;
            }

            if (ch == '"' || ch == '\'')
            {
                quoted = true;
                quote = ch;
                continue;
            }

            if (ch == '#' || (ch == '-' && i + 1 < line.size() && line[i + 1] == '-'))
            {
                cut = i;
                break;
            }

            if (ch == '-' && i > 0 && i + 1 < line.size() &&
                std::isspace(static_cast<unsigned char>(line[i - 1])) != 0 &&
                std::isspace(static_cast<unsigned char>(line[i + 1])) != 0)
            {
                cut = i - 1;
                break;
            }
        }
        return cut;
    }

    static InputLine ParseInputLine(const std::string &rawLine)
    {
        InputLine parsed;
        parsed.raw = rawLine;

        const std::string line = ZString::RemoveUtf8Bom(rawLine);
        const std::string trimmed = ZString::Trim(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.rfind("--", 0) == 0)
            return parsed;

        const std::size_t cut = FindInlineComment(line);
        const std::string dataPart = cut == std::string::npos ? line : line.substr(0, cut);
        parsed.comment = cut == std::string::npos ? std::string{} : line.substr(cut);

        const auto tokens = TokenizeWithSpans(dataPart);
        if (tokens.size() < 2 || !ZString::IsIdentifier(tokens[1].text))
            return parsed;

        parsed.valueToken = tokens[0].text;
        parsed.key = tokens[1].text;
        parsed.isParameter = true;
        parsed.wasQuoted = tokens[0].quoted;
        parsed.quote = tokens[0].quote;
        return parsed;
    }

    void RebuildInputIndex()
    {
        inputIndex.clear();
        for (std::size_t i = 0; i < inputLines.size(); ++i)
        {
            if (inputLines[i].isParameter)
                inputIndex[inputLines[i].key] = i;
        }
    }

    void ClearKeywordInput()
    {
        inputLines.clear();
        inputIndex.clear();
    }

    void LoadKeywordInput(const std::string &filePath, const std::string &fallbackExtension)
    {
        const auto path = ZPath::ResolveExistingPath(filePath, fallbackExtension);
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("Cannot open keyword input file: " + path.string());

        inputLines.clear();
        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            inputLines.push_back(ParseInputLine(line));
        }
        RebuildInputIndex();
    }

    void SaveKeywordInput(const std::string &filePath) const
    {
        const std::filesystem::path path(filePath);
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
            throw std::runtime_error("Cannot write keyword input file: " + path.string());

        for (const auto &line : inputLines)
            file << line.raw << '\n';
    }

    /// @brief 去除引号并解码文本转义。
    static std::string Unquote(std::string value)
    {
        value = ZString::Trim(value);
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                  (value.front() == '\'' && value.back() == '\'')))
        {
            const char quote = value.front();
            value = value.substr(1, value.size() - 2);
            if (quote == '"')
            {
                std::string decoded;
                decoded.reserve(value.size());
                for (std::size_t i = 0; i < value.size(); ++i)
                {
                    if (value[i] != '\\' || i + 1 >= value.size())
                    {
                        decoded.push_back(value[i]);
                        continue;
                    }

                    const char escaped = value[++i];
                    switch (escaped)
                    {
                    case 'n':
                        decoded.push_back('\n');
                        break;
                    case 'r':
                        decoded.push_back('\r');
                        break;
                    case 't':
                        decoded.push_back('\t');
                        break;
                    case '"':
                    case '\\':
                        decoded.push_back(escaped);
                        break;
                    default:
                        decoded.push_back('\\');
                        decoded.push_back(escaped);
                        break;
                    }
                }
                value = std::move(decoded);
            }
        }
        return value;
    }

    /// @brief 将文本值包装为双引号字符串。
    static std::string QuoteTextString(const std::string &value)
    {
        return '"' + value + '"';
    }

    /// @brief 按 YAML 双引号字符串规则转义。
    static std::string EscapeYamlString(const std::string &value)
    {
        std::string result;
        result.reserve(value.size() + 2);
        result.push_back('"');
        for (char ch : value)
        {
            if (ch == '"' || ch == '\\')
                result.push_back('\\');
            result.push_back(ch);
        }
        result.push_back('"');
        return result;
    }

    template <typename T>
    /// @brief 将字符串转换为目标类型。
    static T ConvertFromString(const std::string &rawValue, const std::string &key)
    {
        const std::string value = Unquote(rawValue);
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
        {
            return value;
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, bool>)
        {
            const std::string text = ToLower(value);
            if (text == "true" || text == "ture" || text == "1" || text == "yes" || text == "y")
                return true;
            if (text == "false" || text == "0" || text == "no" || text == "n")
                return false;
            throw std::runtime_error("Invalid bool value for " + key + ": " + rawValue);
        }
        else if constexpr (std::is_enum_v<std::decay_t<T>>)
        {
            using Underlying = std::underlying_type_t<std::decay_t<T>>;
            return static_cast<T>(ConvertFromString<Underlying>(value, key));
        }
        else if constexpr (std::is_integral_v<std::decay_t<T>>)
        {
            try
            {
                std::size_t used = 0;
                const long long parsed = std::stoll(value, &used, 0);
                if (used != value.size())
                    throw std::invalid_argument("trailing data");
                return static_cast<T>(parsed);
            }
            catch (const std::exception &)
            {
                throw std::runtime_error("Invalid integer value for " + key + ": " + rawValue);
            }
        }
        else if constexpr (std::is_floating_point_v<std::decay_t<T>>)
        {
            try
            {
                std::size_t used = 0;
                const double parsed = std::stod(value, &used);
                if (used != value.size())
                    throw std::invalid_argument("trailing data");
                return static_cast<T>(parsed);
            }
            catch (const std::exception &)
            {
                throw std::runtime_error("Invalid floating-point value for " + key + ": " + rawValue);
            }
        }
        else
        {
            static_assert(std::is_same_v<T, void>, "Unsupported serializer value type");
        }
    }

    template <typename T>
    /// @brief 将值转换为可序列化字符串。
    static std::string ConvertToString(const T &value, bool quoteString = false, bool yamlStyle = false)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
        {
            if (quoteString || value.empty() || value.find_first_of(" \t:#") != std::string::npos)
                return yamlStyle ? EscapeYamlString(value) : QuoteTextString(value);
            return value;
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, bool>)
        {
            return value ? "true" : "false";
        }
        else if constexpr (std::is_enum_v<std::decay_t<T>>)
        {
            return ConvertToString(static_cast<std::underlying_type_t<std::decay_t<T>>>(value));
        }
        else if constexpr (std::is_integral_v<std::decay_t<T>>)
        {
            return std::to_string(value);
        }
        else if constexpr (std::is_floating_point_v<std::decay_t<T>>)
        {
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << std::setprecision(15) << value;
            return stream.str();
        }
        else
        {
            static_assert(std::is_same_v<T, void>, "Unsupported serializer value type");
        }
    }

    template <typename T>
    /// @brief 按关键字输入格式读写单个字段。
    void ReadOrWriteKeywordValue(const std::string &key, T &value)
    {
        if (mode == Mode::Read)
        {
            const auto it = inputIndex.find(key);
            if (it == inputIndex.end())
                throw std::runtime_error("Cannot find keyword in input file: " + key);
            value = ConvertFromString<T>(inputLines[it->second].valueToken, key);
            return;
        }

        const auto valueText = ConvertToString(value, FindExistingQuote(key), false);
        const auto it = inputIndex.find(key);
        if (it == inputIndex.end())
        {
            InputLine line;
            line.valueToken = valueText;
            line.key = key;
            line.isParameter = true;
            line.wasQuoted = valueText.size() >= 2 && valueText.front() == '"';
            line.raw = BuildKeywordLine(valueText, key, std::string{});
            inputIndex[key] = inputLines.size();
            inputLines.push_back(std::move(line));
            return;
        }

        InputLine &line = inputLines[it->second];
        line.valueToken = valueText;
        line.raw = BuildKeywordLine(valueText, key, line.comment);
    }

    /// @brief 查找关键字是否在原始输入中使用了引号。
    bool FindExistingQuote(const std::string &key) const
    {
        const auto it = inputIndex.find(key);
        if (it == inputIndex.end())
            return false;
        return inputLines[it->second].wasQuoted;
    }

    /// @brief 构造关键字输入格式的一整行文本。
    static std::string BuildKeywordLine(const std::string &valueText, const std::string &key, const std::string &comment)
    {
        std::ostringstream stream;
        stream << std::left << std::setw(static_cast<int>(std::max<std::size_t>(12, valueText.size() + 1))) << valueText
               << std::left << std::setw(static_cast<int>(std::max<std::size_t>(16, key.size() + 1))) << key;
        if (!comment.empty())
            stream << comment;
        return stream.str();
    }


#pragma region 虚函数,需要继承来实现
    /**
     * @brief 派生类重写的字段序列化入口。
     *
     * 在读取或写入文件时会调用该函数。派生类应在此集中调用 ReadOrWrite()，
     * 将所有需要持久化的成员映射到具体键名。
     */
    virtual void SerializeFields()
    {
    }

#pragma endregion 虚函数, 需要继承来实现
};
