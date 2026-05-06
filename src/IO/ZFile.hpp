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
// 该文件提供一个类似 C# System.IO.File 的跨平台文件工具类，支持文件读写、
// 权限检查等常见文件操作，适合 IO 模块和工具代码直接使用。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ZPath.hpp"
#include "LocaleString.hpp"

/**
 * @class ZFile
 * @brief 类似 C# System.IO.File 的跨平台文件工具类。
 * @details 提供文件存在性判断、大小获取、删除、复制、移动以及文本/字节读写等静态方法。
 *          所有方法自动将路径转换为绝对路径，避免工作目录不一致导致的问题。
 */
class ZFile
{
public:
	/**
	 * @brief 判断文件是否存在。
	 * @param path 文件路径。
	 * @return 存在且为普通文件返回 true，不存在返回 false。
	 * @note 传入空路径返回 false；若路径指向目录也返回 false。
	 * @code ZFile::Exists("test.txt"); @endcode
	 */
	static bool Exists(const std::string &path)
	{
		return std::filesystem::is_regular_file(std::filesystem::path(path));
	}

	/**
	 * @brief 获取文件大小。
	 * @param path 文件路径。
	 * @return 文件字节数（std::uintmax_t）。
	 * @note 文件不存在时抛出 std::runtime_error。
	 * @code auto sz = ZFile::GetLength("data.bin"); @endcode
	 */
	static std::uintmax_t GetLength(const std::string &path)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		if (!Exists(absPath))
			throw std::runtime_error(std::string(L_ZFILE_FileNotFound) + ": " + absPath);

		return std::filesystem::file_size(std::filesystem::path(absPath));
	}

	/**
	 * @brief 删除文件。
	 * @param path 文件路径。
	 * @return 无返回值。
	 * @note 文件不存在时静默返回，不抛出异常。
	 * @code ZFile::Delete("temp.log"); @endcode
	 */
	static void Delete(const std::string &path)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		if (!Exists(absPath))
			return;

		if (!std::filesystem::remove(std::filesystem::path(absPath)))
			throw std::runtime_error(std::string(L_ZFILE_CannotDelete) + ": " + absPath);
	}

	/**
	 * @brief 复制文件。
	 * @param source 源文件路径。
	 * @param destination 目标文件路径。
	 * @param overwrite 是否覆盖已有文件，默认 false。
	 * @return 无返回值。
	 * @note 源文件不存在时抛出 std::runtime_error；目标存在且不覆盖时也抛出异常。
	 * @code ZFile::Copy("a.txt", "b.txt", true); @endcode
	 */
	static void Copy(const std::string &source, const std::string &destination, bool overwrite = false)
	{
		const std::string absSource = ZPath::GetABSPath(source);
		const std::string absDestination = ZPath::GetABSPath(destination);
		if (!Exists(absSource))
			throw std::runtime_error(std::string(L_ZFILE_SourceNotFound) + ": " + absSource);

		std::filesystem::path destinationPath(absDestination);
		if (std::filesystem::exists(destinationPath))
		{
			if (!overwrite)
				throw std::runtime_error(std::string(L_ZFILE_DestExists) + ": " + absDestination);
			std::filesystem::remove(destinationPath);
		}

		std::filesystem::copy_file(std::filesystem::path(absSource), destinationPath, std::filesystem::copy_options::none);
	}

	/**
	 * @brief 移动文件。
	 * @param source 源文件路径。
	 * @param destination 目标文件路径。
	 * @param overwrite 是否覆盖已有文件，默认 false。
	 * @return 无返回值。
	 * @note 优先尝试 rename，跨卷失败时回退为复制+删除原文件。
	 * @code ZFile::Move("old.txt", "new.txt"); @endcode
	 */
	static void Move(const std::string &source, const std::string &destination, bool overwrite = false)
	{
		const std::string absSource = ZPath::GetABSPath(source);
		const std::string absDestination = ZPath::GetABSPath(destination);
		if (!Exists(absSource))
			throw std::runtime_error(std::string(L_ZFILE_SourceNotFound) + ": " + absSource);

		std::filesystem::path destinationPath(absDestination);
		if (std::filesystem::exists(destinationPath))
		{
			if (!overwrite)
				throw std::runtime_error(std::string(L_ZFILE_DestExists) + ": " + absDestination);
			std::filesystem::remove(destinationPath);
		}

		try
		{
			std::filesystem::rename(std::filesystem::path(absSource), destinationPath);
		}
		catch (const std::filesystem::filesystem_error &)
		{
			std::filesystem::copy_file(std::filesystem::path(absSource), destinationPath, std::filesystem::copy_options::none);
			std::filesystem::remove(std::filesystem::path(absSource));
		}
	}

	/**
	 * @brief 读取文件全部文本。
	 * @param path 文件路径。
	 * @return 文件全部内容字符串。
	 * @note 文件不存在或无法打开时抛出 std::runtime_error。
	 * @code auto text = ZFile::ReadAllText("config.json"); @endcode
	 */
	static std::string ReadAllText(const std::string &path)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ifstream file(absPath, std::ios::in | std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error(std::string(L_ZFILE_CannotRead) + ": " + absPath);

		std::ostringstream stream;
		stream << file.rdbuf();
		return stream.str();
	}

	/**
	 * @brief 读取文件所有行。
	 * @param path 文件路径。
	 * @return 行字符串数组，每行不含换行符。
	 * @note 空文件返回空 vector。
	 * @code auto lines = ZFile::ReadAllLines("data.txt"); @endcode
	 */
	static std::vector<std::string> ReadAllLines(const std::string &path)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ifstream file(absPath);
		if (!file.is_open())
			throw std::runtime_error(std::string(L_ZFILE_CannotRead) + ": " + absPath);

		std::vector<std::string> lines;
		std::string line;
		while (std::getline(file, line))
			lines.push_back(line);
		return lines;
	}

	/**
	 * @brief 读取文件全部字节。
	 * @param path 文件路径。
	 * @return 字节数组（std::vector<std::uint8_t>）。
	 * @note 空文件返回空 vector；文件不存在时抛出异常。
	 * @code auto bytes = ZFile::ReadAllBytes("image.png"); @endcode
	 */
	static std::vector<std::uint8_t> ReadAllBytes(const std::string &path)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ifstream file(absPath, std::ios::in | std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error(std::string(L_ZFILE_CannotRead) + ": " + absPath);

		return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	/**
	 * @brief 写入全部文本（覆盖模式）。
	 * @param path 文件路径。
	 * @param content 文本内容。
	 * @return 无返回值。
	 * @note 若文件已存在则覆盖；若目录不存在则抛出异常。
	 * @code ZFile::WriteAllText("log.txt", "Hello World"); @endcode
	 */
	static void WriteAllText(const std::string &path, const std::string &content)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file.is_open())
			throw std::runtime_error(std::string(L_ZFILE_CannotWrite) + ": " + absPath);

		file << content;
	}

	/**
	 * @brief 追加文本到文件。
	 * @param path 文件路径。
	 * @param content 文本内容。
	 * @return 无返回值。
	 * @note 若文件不存在则自动创建；多次写入不会覆盖已有内容。
	 * @code ZFile::AppendAllText("log.txt", "Appended line"); @endcode
	 */
	static void AppendAllText(const std::string &path, const std::string &content)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath, std::ios::out | std::ios::binary | std::ios::app);
		if (!file.is_open())
			throw std::runtime_error(std::string(L_ZFILE_CannotWrite) + ": " + absPath);

		file << content;
	}

	/**
	 * @brief 写入全部行，每行自动追加换行符（覆盖模式）。
	 * @param path 文件路径。
	 * @param lines 行字符串数组。
	 * @return 无返回值。
	 * @note 空 vector 将产生空文件；最后一行也会追加换行符。
	 * @code ZFile::WriteAllLines("out.txt", {"A", "B", "C"}); @endcode
	 */
	static void WriteAllLines(const std::string &path, const std::vector<std::string> &lines)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file.is_open())
			throw std::runtime_error(std::string(L_ZFILE_CannotWrite) + ": " + absPath);

		for (const auto &line : lines)
			file << line << '\n';
	}

	/**
	 * @brief 追加多行文本，每行自动追加换行符。
	 * @param path 文件路径。
	 * @param lines 行字符串数组。
	 * @return 无返回值。
	 * @note 若文件不存在则自动创建；每次调用都在末尾追加。
	 * @code ZFile::AppendAllLines("log.txt", {"Line1", "Line2"}); @endcode
	 */
	static void AppendAllLines(const std::string &path, const std::vector<std::string> &lines)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath, std::ios::out | std::ios::binary | std::ios::app);
		if (!file.is_open())
			throw std::runtime_error(std::string(L_ZFILE_CannotWrite) + ": " + absPath);

		for (const auto &line : lines)
			file << line << '\n';
	}

	/**
	 * @brief 写入全部字节（覆盖模式）。
	 * @param path 文件路径。
	 * @param bytes 字节数组。
	 * @return 无返回值。
	 * @note 空 vector 将产生空文件但不报错。
	 * @code ZFile::WriteAllBytes("data.bin", {0x00, 0xFF, 0xAB}); @endcode
	 */
	static void WriteAllBytes(const std::string &path, const std::vector<std::uint8_t> &bytes)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file.is_open())
			throw std::runtime_error(std::string(L_ZFILE_CannotWrite) + ": " + absPath);

		if (!bytes.empty())
			file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
};