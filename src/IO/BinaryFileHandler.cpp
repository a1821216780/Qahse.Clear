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
// 该文件定义了 BinaryFile 类，提供对二进制文件的读写操作封装。支持多种数据类型的写入，
// 并通过智能指针管理文件流资源，确保安全和高效的文件操作。
//
// ──────────────────────────────────────────────────────────────────────────────


#pragma once

#include "BinaryFileHandler.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include "LocaleString.hpp"

/**
 * @brief 构造函数，根据指定模式打开二进制文件。
 * @param filePath 文件路径，包含文件名和扩展名。
 * @param mode 文件访问模式："read"/"r"(读取)、"write"/"w"(写入)、"append"/"a"(追加)。
 * @details 支持大小写不敏感的模式参数。内部根据模式初始化对应的文件流。
 * @code
 * // 创建用于写入的二进制文件
 * BinaryFile writer("output.bin", "write");
 * // 创建用于读取的二进制文件
 * BinaryFile reader("input.bin", "read");
 * // 创建用于追加的二进制文件
 * BinaryFile appender("log.bin", "append");
 * @endcode
 */
BinaryFile::BinaryFile(const std::string &filePath, const std::string &mode)
{
    this->filePath = filePath; ///< 保存文件路径
    this->isReading = false;   ///< 初始化读取标志
    this->isWriting = false;   ///< 初始化写入标志
    try
    {
        std::string lowerMode = mode;
        std::transform(lowerMode.begin(), lowerMode.end(), lowerMode.begin(), ::tolower); ///< 模式转小写
        // 根据不同模式初始化文件流
        if (lowerMode == "read" || lowerMode == "r")
        {
            reader = std::make_unique<std::ifstream>(filePath, std::ios::binary);
            if (!reader->good())
            {
                LogHelper::ErrorLog(std::string(L_BINFILE_OpenReadFail) + filePath, "", "", 20, "BinaryFile::BinaryFile");
            }
            isReading = true;
        }
        else if (lowerMode == "write" || lowerMode == "w")
        {
            writer = std::make_unique<std::ofstream>(filePath, std::ios::binary | std::ios::trunc);
            if (!writer->good())
            {
                LogHelper::ErrorLog(std::string(L_BINFILE_OpenWriteFail) + filePath, "", "", 20, "BinaryFile::BinaryFile");
            }
            isWriting = true;
        }
        else if (lowerMode == "append" || lowerMode == "a")
        {
            writer = std::make_unique<std::ofstream>(filePath, std::ios::binary | std::ios::app);
            if (!writer->good())
            {
                LogHelper::ErrorLog(std::string(L_BINFILE_OpenAppendFail) + filePath, "", "", 20, "BinaryFile::BinaryFile");
            }
            isWriting = true;
        }
        else
        {
            LogHelper::ErrorLog(std::string(L_BINFILE_ModeNotFound) + mode, "", "", 20, "BinaryFile::BinaryFile");
            writer = std::make_unique<std::ofstream>(filePath, std::ios::binary | std::ios::app);
            if (writer->good())
            {
                isWriting = true;
            }
        }
    }
    catch (const std::exception &ex)
    {
        LogHelper::ErrorLog("BinaryFile \n" + std::string(ex.what()), "", "", 20, "BinaryFile::BinaryFile");
    }
}

/**
 * @brief 析构函数，自动关闭文件流并释放资源。
 * @details 当对象生命周期结束时自动调用，确保文件流被正确关闭，防止资源泄漏。
 * @code
 * {
 *     BinaryFile file("temp.bin", "write");
 *     // 文件操作...
 * } // 此处析构函数被调用，自动关闭文件流
 * @endcode
 */
BinaryFile::~BinaryFile()
{
    Close(); ///< 调用Close确保资源释放
}

/**
 * @brief 手动关闭当前文件流，释放系统资源。
 * @details 建议在完成文件操作后主动调用此方法，而不是依赖析构函数。
 * @code
 * BinaryFile file("data.bin", "write");
 * file.WriteData(42);
 * file.Close(); // 主动关闭文件流
 * @endcode
 */
void BinaryFile::Close()
{
    // 关闭写入流
    if (writer && writer->is_open())
    {
        writer->close();
        writer.reset();
        isWriting = false;
    }
    // 关闭读取流
    if (reader && reader->is_open())
    {
        reader->close();
        reader.reset();
        isReading = false;
    }
}

// 基本类型写入特化
/**
 * @brief WriteData 模板特化：写入 short 类型数据
 * @param data 要写入的 short 数据
 */
template <>
void BinaryFile::WriteData<short>(const short &data)
{
    WriteBasicType(data); ///< 直接写入二进制数据
}

/**
 * @brief WriteData 模板特化：写入 int 类型数据
 */
template <>
void BinaryFile::WriteData<int>(const int &data)
{
    WriteBasicType(data);
}

/**
 * @brief WriteData 模板特化：写入 int64_t 类型数据
 */
template <>
void BinaryFile::WriteData<std::int64_t>(const std::int64_t &data)
{
    WriteBasicType(data);
}

/**
 * @brief WriteData 模板特化：写入 float 类型数据
 */
template <>
void BinaryFile::WriteData<float>(const float &data)
{
    WriteBasicType(data);
}

/**
 * @brief WriteData 模板特化：写入 double 类型数据
 */
template <>
void BinaryFile::WriteData<double>(const double &data)
{
    WriteBasicType(data);
}

/**
 * @brief WriteData 模板特化：写入 char 类型数据
 */
template <>
void BinaryFile::WriteData<char>(const char &data)
{
    WriteBasicType(data);
}

/**
 * @brief WriteData 模板特化：写入 bool 类型数据（转为 int 存储，1=true, 0=false）
 */
template <>
void BinaryFile::WriteData<bool>(const bool &data)
{
    int value = data ? 1 : 0;
    WriteBasicType(value);
}

/**
 * @brief WriteData 模板特化：写入 std::string 类型数据（先写长度再写内容）
 */
template <>
void BinaryFile::WriteData<std::string>(const std::string &data)
{
    WriteString(data);
}

// 数组类型写入特化
/**
 * @brief WriteData 模板特化：写入 std::vector<char> 数组（先写长度再写内容）
 */
template <>
void BinaryFile::WriteData<std::vector<char>>(const std::vector<char> &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    if (!data.empty())
    {
        writer->write(reinterpret_cast<const char *>(data.data()), data.size());
    }
}

/**
 * @brief WriteData 模板特化：写入 std::vector<std::string> 数组
 */
template <>
void BinaryFile::WriteData<std::vector<std::string>>(const std::vector<std::string> &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    for (const auto &str : data)
    {
        WriteString(str);
    }
}

/**
 * @brief WriteData 模板特化：写入 std::vector<float> 数组
 */
template <>
void BinaryFile::WriteData<std::vector<float>>(const std::vector<float> &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    for (const auto &value : data)
    {
        WriteBasicType(value);
    }
}

/**
 * @brief WriteData 模板特化：写入 std::vector<double> 数组
 */
template <>
void BinaryFile::WriteData<std::vector<double>>(const std::vector<double> &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    for (const auto &value : data)
    {
        WriteBasicType(value);
    }
}

/**
 * @brief WriteData 模板特化：写入 std::vector<int> 数组
 */
template <>
void BinaryFile::WriteData<std::vector<int>>(const std::vector<int> &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    for (const auto &value : data)
    {
        WriteBasicType(value);
    }
}

/**
 * @brief WriteData 模板特化：写入 std::vector<bool> 数组（bool 转为 int 存储）
 */
template <>
void BinaryFile::WriteData<std::vector<bool>>(const std::vector<bool> &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    for (size_t i = 0; i < data.size(); ++i)
    {
        int value = data[i] ? 1 : 0;
        WriteBasicType(value);
    }
}

// 二维数组写入特化
/**
 * @brief WriteData 模板特化：写入二维 std::vector<std::vector<double>> 数组（先写行列数再写元素）
 */
template <>
void BinaryFile::WriteData<std::vector<std::vector<double>>>(const std::vector<std::vector<double>> &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    WriteBasicType(static_cast<int>(data.empty() ? 0 : data[0].size()));
    for (const auto &row : data)
    {
        for (const auto &value : row)
        {
            WriteBasicType(value);
        }
    }
}

/**
 * @brief WriteData 模板特化：写入二维 std::vector<std::vector<float>> 数组
 */
template <>
void BinaryFile::WriteData<std::vector<std::vector<float>>>(const std::vector<std::vector<float>> &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    WriteBasicType(static_cast<int>(data.empty() ? 0 : data[0].size()));
    for (const auto &row : data)
    {
        for (const auto &value : row)
        {
            WriteBasicType(value);
        }
    }
}

/**
 * @brief WriteData 模板特化：写入二维 std::vector<std::vector<int>> 数组
 */
template <>
void BinaryFile::WriteData<std::vector<std::vector<int>>>(const std::vector<std::vector<int>> &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    WriteBasicType(static_cast<int>(data.empty() ? 0 : data[0].size()));
    for (const auto &row : data)
    {
        for (const auto &value : row)
        {
            WriteBasicType(value);
        }
    }
}

// Eigen矩阵和向量写入特化
/**
 * @brief WriteData 模板特化：写入 Eigen::MatrixXd 矩阵（先写行列数再逐元素写入）
 */
template <>
void BinaryFile::WriteData<Eigen::MatrixXd>(const Eigen::MatrixXd &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.rows()));
    WriteBasicType(static_cast<int>(data.cols()));
    for (int i = 0; i < data.rows(); ++i)
    {
        for (int j = 0; j < data.cols(); ++j)
        {
            WriteBasicType(data(i, j));
        }
    }
}

/**
 * @brief WriteData 模板特化：写入 Eigen::MatrixXf 矩阵
 */
template <>
void BinaryFile::WriteData<Eigen::MatrixXf>(const Eigen::MatrixXf &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.rows()));
    WriteBasicType(static_cast<int>(data.cols()));
    for (int i = 0; i < data.rows(); ++i)
    {
        for (int j = 0; j < data.cols(); ++j)
        {
            WriteBasicType(data(i, j));
        }
    }
}

/**
 * @brief WriteData 模板特化：写入 Eigen::VectorXd 向量（先写长度再逐元素写入）
 */
template <>
void BinaryFile::WriteData<Eigen::VectorXd>(const Eigen::VectorXd &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    for (int i = 0; i < data.size(); ++i)
    {
        WriteBasicType(data(i));
    }
}

/**
 * @brief WriteData 模板特化：写入 Eigen::VectorXf 向量
 */
template <>
void BinaryFile::WriteData<Eigen::VectorXf>(const Eigen::VectorXf &data)
{
    if (!isWriting || !writer)
        return;
    WriteBasicType(static_cast<int>(data.size()));
    for (int i = 0; i < data.size(); ++i)
    {
        WriteBasicType(data(i));
    }
}

// 基本类型读取特化
/**
 * @brief ReadData 模板特化：读取 short 类型数据
 */
template <>
short BinaryFile::ReadData<short>()
{
    return ReadBasicType<short>();
}

/**
 * @brief ReadData 模板特化：读取 int 类型数据
 */
template <>
int BinaryFile::ReadData<int>()
{
    return ReadBasicType<int>();
}

/**
 * @brief ReadData 模板特化：读取 int64_t 类型数据
 */
template <>
std::int64_t BinaryFile::ReadData<std::int64_t>()
{
    return ReadBasicType<std::int64_t>();
}

/**
 * @brief ReadData 模板特化：读取 float 类型数据
 */
template <>
float BinaryFile::ReadData<float>()
{
    return ReadBasicType<float>();
}

/**
 * @brief ReadData 模板特化：读取 double 类型数据
 */
template <>
double BinaryFile::ReadData<double>()
{
    return ReadBasicType<double>();
}

/**
 * @brief ReadData 模板特化：读取 char 类型数据
 */
template <>
char BinaryFile::ReadData<char>()
{
    return ReadBasicType<char>();
}

/**
 * @brief ReadData 模板特化：读取 bool 类型数据（从 int 还原，1=true, 0=false）
 */
template <>
bool BinaryFile::ReadData<bool>()
{
    int value = ReadBasicType<int>();
    return value == 1;
}

/**
 * @brief ReadData 模板特化：读取 std::string 类型数据
 */
template <>
std::string BinaryFile::ReadData<std::string>()
{
    return ReadString();
}

// 数组类型读取特化
template <>
std::vector<char> BinaryFile::ReadData<std::vector<char>>()
{
    if (!isReading || !reader)
        return {};
    int length = ReadBasicType<int>();
    if (length <= 0)
        return {};
    std::vector<char> result(length);
    reader->read(result.data(), length);
    return result;
}

template <>
std::vector<std::string> BinaryFile::ReadData<std::vector<std::string>>()
{
    if (!isReading || !reader)
        return {};
    int length = ReadBasicType<int>();
    std::vector<std::string> result(length);
    for (int i = 0; i < length; ++i)
    {
        result[i] = ReadString();
    }
    return result;
}

template <>
std::vector<float> BinaryFile::ReadData<std::vector<float>>()
{
    if (!isReading || !reader)
        return {};
    int length = ReadBasicType<int>();
    std::vector<float> result(length);
    for (int i = 0; i < length; ++i)
    {
        result[i] = ReadBasicType<float>();
    }
    return result;
}

template <>
std::vector<double> BinaryFile::ReadData<std::vector<double>>()
{
    if (!isReading || !reader)
        return {};
    int length = ReadBasicType<int>();
    std::vector<double> result(length);
    for (int i = 0; i < length; ++i)
    {
        result[i] = ReadBasicType<double>();
    }
    return result;
}

template <>
std::vector<int> BinaryFile::ReadData<std::vector<int>>()
{
    if (!isReading || !reader)
        return {};
    int length = ReadBasicType<int>();
    std::vector<int> result(length);
    for (int i = 0; i < length; ++i)
    {
        result[i] = ReadBasicType<int>();
    }
    return result;
}

template <>
std::vector<bool> BinaryFile::ReadData<std::vector<bool>>()
{
    if (!isReading || !reader)
        return {};
    int length = ReadBasicType<int>();
    std::vector<bool> result(length);
    for (int i = 0; i < length; ++i)
    {
        int value = ReadBasicType<int>();
        result[i] = (value == 1);
    }
    return result;
}

// 二维数组读取特化
template <>
std::vector<std::vector<double>> BinaryFile::ReadData<std::vector<std::vector<double>>>()
{
    if (!isReading || !reader)
        return {};
    int rows = ReadBasicType<int>();
    int cols = ReadBasicType<int>();
    std::vector<std::vector<double>> result(rows, std::vector<double>(cols));
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            result[i][j] = ReadBasicType<double>();
        }
    }
    return result;
}

template <>
std::vector<std::vector<float>> BinaryFile::ReadData<std::vector<std::vector<float>>>()
{
    if (!isReading || !reader)
        return {};
    int rows = ReadBasicType<int>();
    int cols = ReadBasicType<int>();
    std::vector<std::vector<float>> result(rows, std::vector<float>(cols));
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            result[i][j] = ReadBasicType<float>();
        }
    }
    return result;
}

template <>
std::vector<std::vector<int>> BinaryFile::ReadData<std::vector<std::vector<int>>>()
{
    if (!isReading || !reader)
        return {};
    int rows = ReadBasicType<int>();
    int cols = ReadBasicType<int>();
    std::vector<std::vector<int>> result(rows, std::vector<int>(cols));
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            result[i][j] = ReadBasicType<int>();
        }
    }
    return result;
}

// Eigen矩阵和向量读取特化
template <>
Eigen::MatrixXd BinaryFile::ReadData<Eigen::MatrixXd>()
{
    if (!isReading || !reader)
        return {};
    int rows = ReadBasicType<int>();
    int cols = ReadBasicType<int>();
    Eigen::MatrixXd result = LinearAlgebraHelper::zeros(rows, cols);
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            result(i, j) = ReadBasicType<double>();
        }
    }
    return result;
}

template <>
Eigen::MatrixXf BinaryFile::ReadData<Eigen::MatrixXf>()
{
    if (!isReading || !reader)
        return {};
    int rows = ReadBasicType<int>();
    int cols = ReadBasicType<int>();
    Eigen::MatrixXf result = LinearAlgebraHelper::fzeros(rows, cols);
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            result(i, j) = ReadBasicType<float>();
        }
    }
    return result;
}

template <>
Eigen::VectorXd BinaryFile::ReadData<Eigen::VectorXd>()
{
    if (!isReading || !reader)
        return {};
    int size = ReadBasicType<int>();
    Eigen::VectorXd result = LinearAlgebraHelper::zeros(size);
    for (int i = 0; i < size; ++i)
    {
        result(i) = ReadBasicType<double>();
    }
    return result;
}

template <>
Eigen::VectorXf BinaryFile::ReadData<Eigen::VectorXf>()
{
    if (!isReading || !reader)
        return {};
    int size = ReadBasicType<int>();
    Eigen::VectorXf result = LinearAlgebraHelper::fzeros(size);
    for (int i = 0; i < size; ++i)
    {
        result(i) = ReadBasicType<float>();
    }
    return result;
}

// WriteLine 方法实现
/**
 * @brief WriteLine 模板实现：泛型数据写入（内部调用 WriteData）
 */
template <typename T>
void BinaryFile::WriteLine(const T &data)
{
    WriteData(data);
}

/**
 * @brief WriteLine 格式化 double 版本
 */
void BinaryFile::WriteLine(const std::string &format, double message)
{
    std::string formatted = FormatNumber(format, message);
    WriteData(formatted);
}

/**
 * @brief WriteLine 格式化 float 版本
 */
void BinaryFile::WriteLine(const std::string &format, float message)
{
    std::string formatted = FormatNumber(format, message);
    WriteData(formatted);
}

/**
 * @brief WriteLine 无参版本（写入空行，当前为占位实现）
 */
void BinaryFile::WriteLine()
{
    // C++中对于二进制文件，空行的概念需要根据具体需求定义
    // 这里暂时不实现具体内容
}

// Write 方法实现
/**
 * @brief Write 模板实现：泛型数据写入（内部调用 WriteData）
 */
template <typename T>
void BinaryFile::Write(const T &data)
{
    WriteData(data);
}

/**
 * @brief Write 格式化 double 版本
 */
void BinaryFile::Write(const std::string &format, double message)
{
    std::string formatted = FormatNumber(format, message);
    WriteData(formatted);
}

/**
 * @brief Write 格式化 float 版本
 */
void BinaryFile::Write(const std::string &format, float message)
{
    std::string formatted = FormatNumber(format, message);
    WriteData(formatted);
}

bool BinaryFile::Read2last()
{

    if (!isReading || !reader)
        return true;

    std::streampos currentPos = reader->tellg();
    int testValue = -858993460;

    try
    {
        testValue = ReadBasicType<int>();
    }
    catch (...)
    {
    }

    bool endfile = reader->eof();
    reader->seekg(currentPos); // 恢复位置
    return endfile;
}

// 私有辅助方法实现
/**
 * @brief 以二进制方式写入基本数据类型
 * @tparam T 基本数据类型
 * @param data 要写入的数据
 * @details 直接以 reinterpret_cast 将数据按字节写入文件流
 */
template <typename T>
void BinaryFile::WriteBasicType(const T &data)
{
    if (!isWriting || !writer)
        return;
    writer->write(reinterpret_cast<const char *>(&data), sizeof(T));
}

/**
 * @brief 以二进制方式读取基本数据类型
 * @tparam T 基本数据类型
 * @return 读取到的数据值
 * @details 直接以 reinterpret_cast 从文件流按字节读取数据
 */
template <typename T>
T BinaryFile::ReadBasicType()
{
    if (!isReading || !reader)
    {
        LogHelper::ErrorLog(L_BINFILE_UnsupportedType, "", "", 20, "T BinaryFile::ReadBasicType");
        return T{};
    }
    T value;
    reader->read(reinterpret_cast<char *>(&value), sizeof(T));
    if (!reader->good())
    {
        LogHelper::WarnLog(L_BINFILE_ReadFail);
    }
    return value;
}

/**
 * @brief 以二进制方式写入字符串（先写长度再写内容）
 * @param str 要写入的字符串
 */
void BinaryFile::WriteString(const std::string &str)
{
    if (!isWriting || !writer)
        return;

    // 写入字符串长度
    std::uint32_t length = static_cast<std::uint32_t>(str.length());
    WriteBasicType(length);

    // 写入字符串内容
    writer->write(str.c_str(), str.length());
}

/**
 * @brief 以二进制方式读取字符串（先读长度再读内容）
 * @return 读取到的字符串
 */
std::string BinaryFile::ReadString()
{
    if (!isReading || !reader)
        return "";

    // 读取字符串长度
    std::uint32_t length = ReadBasicType<std::uint32_t>();

    // 读取字符串内容
    std::string result(length, '\0');
    reader->read(&result[0], length);

    return result;
}

/**
 * @brief 按指定格式将 double 数值转为字符串
 * @param format 格式标识符（"F1"/"F2"/"E"/"P"/"P0"/"P1"/"C"/"N" 等）
 * @param value 要格式化的数值
 * @return 格式化后的字符串
 */
std::string BinaryFile::FormatNumber(const std::string &format, double value)
{
    std::ostringstream oss;

    if (format == "F1")
    {
        oss << std::fixed << std::setprecision(1) << value;
    }
    else if (format == "F2")
    {
        oss << std::fixed << std::setprecision(2) << value;
    }
    else if (format == "E")
    {
        oss << std::scientific << value;
    }
    else if (format == "P")
    {
        oss << std::fixed << std::setprecision(2) << (value * 100) << " %";
    }
    else if (format == "P0")
    {
        oss << std::fixed << std::setprecision(0) << (value * 100) << "%";
    }
    else if (format == "P1")
    {
        oss << std::fixed << std::setprecision(1) << (value * 100) << "%";
    }
    else if (format == "C")
    {
        oss << "$" << std::fixed << std::setprecision(2) << value;
    }
    else if (format == "N")
    {
        // 简单的千位分隔符格式
        oss << std::fixed << std::setprecision(1) << value;
        // 这里可以添加更复杂的千位分隔符逻辑
    }
    else
    {
        oss << value;
    }

    return oss.str();
}

/**
 * @brief 按指定格式将 float 数值转为字符串（委托 double 版本）
 * @param format 格式标识符
 * @param value 要格式化的 float 数值
 * @return 格式化后的字符串
 */
std::string BinaryFile::FormatNumber(const std::string &format, float value)
{
    return FormatNumber(format, static_cast<double>(value));
}

// 显式实例化常用的模板
template void BinaryFile::WriteData<short>(const short &);
template void BinaryFile::WriteData<int>(const int &);
template void BinaryFile::WriteData<std::int64_t>(const std::int64_t &);
template void BinaryFile::WriteData<float>(const float &);
template void BinaryFile::WriteData<double>(const double &);
template void BinaryFile::WriteData<char>(const char &);
template void BinaryFile::WriteData<bool>(const bool &);
template void BinaryFile::WriteData<std::string>(const std::string &);

template short BinaryFile::ReadData<short>();
template int BinaryFile::ReadData<int>();
template std::int64_t BinaryFile::ReadData<std::int64_t>();
template float BinaryFile::ReadData<float>();
template double BinaryFile::ReadData<double>();
template char BinaryFile::ReadData<char>();
template bool BinaryFile::ReadData<bool>();
template std::string BinaryFile::ReadData<std::string>();

template void BinaryFile::WriteLine<int>(const int &);
template void BinaryFile::WriteLine<double>(const double &);
template void BinaryFile::WriteLine<std::string>(const std::string &);

template void BinaryFile::Write<int>(const int &);
template void BinaryFile::Write<double>(const double &);
template void BinaryFile::Write<std::string>(const std::string &);
