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

class ZFile
{
public:
	/**
	 * @brief 判断文件是否存在。
	 * @param path 文件路径。
	 * @return 存在且为普通文件返回 true。
	 */
	static bool Exists(const std::string &path)
	{
		return std::filesystem::is_regular_file(std::filesystem::path(path));
	}

	/**
	 * @brief 获取文件大小。
	 * @param path 文件路径。
	 * @return 文件字节数。
	 */
	static std::uintmax_t GetLength(const std::string &path)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		if (!Exists(absPath))
			throw std::runtime_error("无法找到文件: " + absPath);

		return std::filesystem::file_size(std::filesystem::path(absPath));
	}

	/**
	 * @brief 删除文件。
	 * @param path 文件路径。
	 */
	static void Delete(const std::string &path)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		if (!Exists(absPath))
			return;

		if (!std::filesystem::remove(std::filesystem::path(absPath)))
			throw std::runtime_error("无法删除文件: " + absPath);
	}

	/**
	 * @brief 复制文件。
	 * @param source 源文件。
	 * @param destination 目标文件。
	 * @param overwrite 是否覆盖已有文件，默认 false。
	 */
	static void Copy(const std::string &source, const std::string &destination, bool overwrite = false)
	{
		const std::string absSource = ZPath::GetABSPath(source);
		const std::string absDestination = ZPath::GetABSPath(destination);
		if (!Exists(absSource))
			throw std::runtime_error("无法找到源文件: " + absSource);

		std::filesystem::path destinationPath(absDestination);
		if (std::filesystem::exists(destinationPath))
		{
			if (!overwrite)
				throw std::runtime_error("目标文件已存在: " + absDestination);
			std::filesystem::remove(destinationPath);
		}

		std::filesystem::copy_file(std::filesystem::path(absSource), destinationPath, std::filesystem::copy_options::none);
	}

	/**
	 * @brief 移动文件。
	 * @param source 源文件。
	 * @param destination 目标文件。
	 * @param overwrite 是否覆盖已有文件，默认 false。
	 */
	static void Move(const std::string &source, const std::string &destination, bool overwrite = false)
	{
		const std::string absSource = ZPath::GetABSPath(source);
		const std::string absDestination = ZPath::GetABSPath(destination);
		if (!Exists(absSource))
			throw std::runtime_error("无法找到源文件: " + absSource);

		std::filesystem::path destinationPath(absDestination);
		if (std::filesystem::exists(destinationPath))
		{
			if (!overwrite)
				throw std::runtime_error("目标文件已存在: " + absDestination);
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
	 * @return 文件内容。
	 */
	static std::string ReadAllText(const std::string &path)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ifstream file(absPath, std::ios::in | std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error("无法打开文件: " + absPath);

		std::ostringstream stream;
		stream << file.rdbuf();
		return stream.str();
	}

	/**
	 * @brief 读取文件所有行。
	 * @param path 文件路径。
	 * @return 行数组。
	 */
	static std::vector<std::string> ReadAllLines(const std::string &path)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ifstream file(absPath);
		if (!file.is_open())
			throw std::runtime_error("无法打开文件: " + absPath);

		std::vector<std::string> lines;
		std::string line;
		while (std::getline(file, line))
			lines.push_back(line);
		return lines;
	}

	/**
	 * @brief 读取文件全部字节。
	 * @param path 文件路径。
	 * @return 字节数组。
	 */
	static std::vector<std::uint8_t> ReadAllBytes(const std::string &path)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ifstream file(absPath, std::ios::in | std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error("无法打开文件: " + absPath);

		return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	/**
	 * @brief 写入全部文本。
	 * @param path 文件路径。
	 * @param content 文本内容。
	 */
	static void WriteAllText(const std::string &path, const std::string &content)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file.is_open())
			throw std::runtime_error("无法写入文件: " + absPath);

		file << content;
	}

	/**
	 * @brief 追加文本到文件。
	 * @param path 文件路径。
	 * @param content 文本内容。
	 */
	static void AppendAllText(const std::string &path, const std::string &content)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath, std::ios::out | std::ios::binary | std::ios::app);
		if (!file.is_open())
			throw std::runtime_error("无法写入文件: " + absPath);

		file << content;
	}

	/**
	 * @brief 写入全部行，每行自动换行。
	 * @param path 文件路径。
	 * @param lines 行数组。
	 */
	static void WriteAllLines(const std::string &path, const std::vector<std::string> &lines)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file.is_open())
			throw std::runtime_error("无法写入文件: " + absPath);

		for (const auto &line : lines)
			file << line << '\n';
	}

	/**
	 * @brief 追加多行文本，每行自动换行。
	 * @param path 文件路径。
	 * @param lines 行数组。
	 */
	static void AppendAllLines(const std::string &path, const std::vector<std::string> &lines)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath, std::ios::out | std::ios::binary | std::ios::app);
		if (!file.is_open())
			throw std::runtime_error("无法写入文件: " + absPath);

		for (const auto &line : lines)
			file << line << '\n';
	}

	/**
	 * @brief 写入全部字节。
	 * @param path 文件路径。
	 * @param bytes 字节数组。
	 */
	static void WriteAllBytes(const std::string &path, const std::vector<std::uint8_t> &bytes)
	{
		const std::string absPath = ZPath::GetABSPath(path);
		std::ofstream file(absPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file.is_open())
			throw std::runtime_error("无法写入文件: " + absPath);

		if (!bytes.empty())
			file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
};