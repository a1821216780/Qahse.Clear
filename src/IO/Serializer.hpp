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
