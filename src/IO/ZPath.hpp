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
// 该文件提供一个类似 C# System.IO.Path 的跨平台路径工具类。
// 支持绝对路径处理、目录名/文件名提取、扩展名处理、路径拼接
// 以及常见路径规范化操作，适合 IO 模块和工具代码直接使用。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>

#include "LogHelper.h"

class ZPath
{
public:
	/**
	 * @brief 规范化路径分隔符。
	 * @param path 输入路径。
	 * @return 规范化后的路径字符串。
	 */
	static std::string NormalizeSeparators(const std::string &path)
	{
		return std::filesystem::path(path).make_preferred().string();
	}

	/**
	 * @brief 判断路径是否为空。
	 * @param path 输入路径。
	 * @return 空返回 true。
	 */
	static bool IsEmpty(const std::string &path)
	{
		return path.empty();
	}

	/**
	 * @brief 判断是否为绝对路径。
	 * @param path 输入路径。
	 * @return 绝对路径返回 true。
	 */
	static bool IsAbsolute(const std::string &path)
	{
		return std::filesystem::path(path).is_absolute();
	}

	/**
	 * @brief 获取绝对路径。
	 *
	 * 如果输入为空则直接返回空字符串；若转换失败则返回规范化后的原始路径。
	 *
	 * @param path 输入路径。
	 * @return 绝对路径字符串。
	 */
	static std::string GetABSPath(const std::string &path)
	{
		if (path.empty())
			return {};

		try
		{
			return std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().string();
		}
		catch (...)
		{
			return NormalizeSeparators(path);
		}
	}

	/**
	 * @brief 获取标准化后的完整路径。
	 * @param path 输入路径。
	 * @return 归一化路径字符串。
	 */
	static std::string GetFullPath(const std::string &path)
	{
		return std::filesystem::path(path).lexically_normal().string();
	}

	/**
	 * @brief 获取目录名。
	 * @param path 输入路径。
	 * @return 父目录路径字符串。
	 */
	static std::string GetDirectoryName(const std::string &path)
	{
		return std::filesystem::path(path).parent_path().string();
	}

	/**
	 * @brief 获取文件名。
	 * @param path 输入路径。
	 * @return 文件名字符串。
	 */
	static std::string GetFileName(const std::string &path)
	{
		return std::filesystem::path(path).filename().string();
	}

	/**
	 * @brief 获取不含扩展名的文件名。
	 * @param path 输入路径。
	 * @return 不含扩展名的文件名字符串。
	 */
	static std::string GetFileNameWithoutExtension(const std::string &path)
	{
		return std::filesystem::path(path).stem().string();
	}

	/**
	 * @brief 获取文件扩展名。
	 * @param path 输入路径。
	 * @return 扩展名字符串（包含点）。
	 */
	static std::string GetExtension(const std::string &path)
	{
		return std::filesystem::path(path).extension().string();
	}

	/**
	 * @brief 判断路径是否包含扩展名。
	 * @param path 输入路径。
	 * @return 包含扩展名返回 true。
	 */
	static bool HasExtension(const std::string &path)
	{
		return !GetExtension(path).empty();
	}

	/**
	 * @brief 删除扩展名。
	 * @param path 输入路径。
	 * @return 去掉扩展名后的路径。
	 */
	static std::string RemoveExtension(const std::string &path)
	{
		return std::filesystem::path(path).replace_extension().string();
	}

	/**
	 * @brief 以当前平台分隔符拼接两个路径片段。
	 * @param left 左侧路径。
	 * @param right 右侧路径。
	 * @return 拼接后的路径。
	 */
	static std::string Combine(const std::string &left, const std::string &right)
	{
		return (std::filesystem::path(left) / std::filesystem::path(right)).string();
	}

	/**
	 * @brief 以当前平台分隔符拼接多个路径片段。
	 * @param parts 路径片段列表。
	 * @return 拼接后的路径。
	 */
	static std::string Combine(std::initializer_list<std::string> parts)
	{
		std::filesystem::path result;
		for (const auto &part : parts)
		{
			result /= part;
		}
		return result.string();
	}

	/**
	 * @brief 为路径补充末尾分隔符。
	 * @param path 输入路径。
	 * @return 以分隔符结尾的路径。
	 */
	static std::string EnsureTrailingSeparator(const std::string &path)
	{
		if (path.empty())
			return path;

		std::string text = std::filesystem::path(path).string();
		const char separator = std::filesystem::path::preferred_separator;
		if (!text.empty() && text.back() != separator)
			text.push_back(separator);
		return text;
	}

	/**
	 * @brief 移除路径末尾分隔符。
	 * @param path 输入路径。
	 * @return 去除末尾分隔符后的路径。
	 */
	static std::string RemoveTrailingSeparator(const std::string &path)
	{
		if (path.empty())
			return path;

		std::string text = std::filesystem::path(path).string();
		while (!text.empty() && (text.back() == '/' || text.back() == '\\'))
		{
			if (text.size() == 1)
				break;
			text.pop_back();
		}
		return text;
	}

	/**
	 * @brief 获取平台首选分隔符。
	 * @return 单字符分隔符字符串。
	 */
	static std::string GetPathSeparator()
	{
		return std::string(1, std::filesystem::path::preferred_separator);
	}

	/**
	 * @brief 判断路径是否存在。
	 * @param path 输入路径。
	 * @return 存在返回 true。
	 */
	static bool Exists(const std::string &path)
	{
		return std::filesystem::exists(std::filesystem::path(path));
	}

	/**
	 * @brief 判断路径是否为目录。
	 * @param path 输入路径。
	 * @return 目录返回 true。
	 */
	static bool IsDirectory(const std::string &path)
	{
		return std::filesystem::is_directory(std::filesystem::path(path));
	}

	/**
	 * @brief 判断路径是否为文件。
	 * @param path 输入路径。
	 * @return 文件返回 true。
	 */
	static bool IsFile(const std::string &path)
	{
		return std::filesystem::is_regular_file(std::filesystem::path(path));
	}

	/**
	 * @brief 按原路径或补充后缀查找已存在文件。
	 * @param filePath 原始文件路径。
	 * @param fallbackExtension 原始路径不存在时尝试追加的后缀。
	 * @return 可打开的实际路径；如果都不存在，则返回原始路径。
	 */
	static std::filesystem::path ResolveExistingPath(const std::string &filePath, const std::string &fallbackExtension)
	{
		std::filesystem::path path(filePath);
		if (std::filesystem::is_regular_file(path))
			return path;

		if (!fallbackExtension.empty() && path.extension().empty())
		{
			path += fallbackExtension;
			if (std::filesystem::is_regular_file(path))
				return path;
		}

		return std::filesystem::path(filePath);
	}

		/// @brief 解析相对于当前文件路径的路径。
	static std::string ResolvePath(const std::string &baseFilePath,
	                               const std::string &relative)
	{
		if (relative.empty()) return {};
		std::filesystem::path base(baseFilePath);
		std::filesystem::path rel(relative);
		if (rel.is_absolute()) return relative;
		return (base.parent_path() / rel).lexically_normal().string();
	}

#pragma region 兼容HawtC2之前版本的路径检查方法

	/**
	 * @brief 检查指定目录是否存在，不存在时可选择自动创建。
	 * @param name 目录路径。
	 * @param createDir 是否自动创建目录。
	 * @details 若目录不存在且createDir为true，则尝试自动创建。
	 * @code
	 * ZPath::CheckDir("./output", true);
	 * @endcode
	 */
	static void CheckDir(const std::string &name, bool createDir)
	{
		if (!Exists(name))
		{
			LogHelper::WriteLogO("文件夹路径：" + name + "不存在！");
			if (createDir)
			{
				try
				{
					std::filesystem::create_directories(name); ///< 自动创建目录
				}
				catch (const std::exception &)
				{
					LogHelper::WarnLog("您的权限不够！", "CheckDir");
					LogHelper::ErrorLog("文件路径：" + name + "没有不存在,且系统权限不够，无法创建", "", "", 20, "ZPath::CheckDir");
				}
			}
		}
	}

	/**
	 * @brief 检查函数指针是否为空。
	 * @param ptr 待检查的指针。
	 * @param name 函数名。
	 * @param dllpath DLL路径。
	 * @details 若ptr为nullptr则输出错误日志。
	 * @code
	 * void* funcPtr = GetProcAddress(...);
	 * ZPath::CheckInptr(funcPtr, "MyFunc", "./lib.dll");
	 * @endcode
	 */
	static void CheckInptr(void *ptr, const std::string &name, const std::string &dllpath)
	{
		if (ptr == nullptr)
		{
			LogHelper::ErrorLog("Cant find function name: " + name + " in dll path:\n  " + dllpath, "", "", 20, "ZPath::CheckInptr");
		}
	}

	/**
	 * @brief 检查并修正文件路径的扩展名。
	 * @param path 文件路径（引用，可能被修改）。
	 * @param extension1 期望的扩展名（如".txt"）。
	 * @param show 是否输出警告。
	 * @param information 警告信息前缀。
	 * @details 若扩展名为空则自动补全，若不一致则报错。
	 * @code
	 * std::string path = "result";
	 * ZPath::CheckPath(path, ".dat"); // path会变为"result.dat"
	 * @endcode
	 */
	static void CheckPath(std::string &path, const std::string &extension1,
						  bool show, const std::string &information = "")
	{
		std::filesystem::path filePath(path);
		std::string extension = GetExtension(path);
		std::string info = "" ? "当前文件的拓展名称为空，已自动补全为" + extension1 + "！请检查后再尝试" : information;

		if (extension.empty())
		{
			if (show)
			{
				LogHelper::WarnLog(info + extension1, "CheckPath");
			}
			else
			{
				LogHelper::WriteLogO(info + extension1);
			}
			path += extension1; ///< 自动补全扩展名
		}
		else if (extension != extension1)
		{
			LogHelper::ErrorLog("当前文件的拓展名称" + extension + " 与指定名称" +
									extension1 + " 不同！请检查后再尝试",
								"", "", 20, "ZPath::CheckPath");
		}
	}

	/**
	 * @brief 检查指定文件路径的父目录是否存在，不存在时可选择自动创建。
	 * @param name 文件路径。
	 * @param createdir 是否自动创建父目录。
	 * @details 若父目录不存在且createdir为true，则尝试自动创建。
	 * @code
	 * ZPath::CheckPath("./data/test.txt", true);
	 * @endcode
	 */
	static void CheckPath(const std::string &name, bool createdir)
	{
		std::string name1 = GetABSPath(name); ///< 获取绝对路径
		// std::filesystem::path path(name1);
		std::string temp = GetDirectoryName(name1); ///< 父目录路径

		if (temp.empty())
		{
			LogHelper::ErrorLog("文件路径：" + name1 + "文件的父文件夹为空！或路径定义错误！", "", "", 20, "ZPath::CheckPath");
			return;
		}

		if (!std::filesystem::exists(temp))
		{
			LogHelper::WriteLogO("文件路径：" + temp + "没有不存在！");
			if (createdir)
			{
				try
				{
					std::filesystem::create_directories(temp); ///< 自动创建父目录
				}
				catch (const std::exception &)
				{
					LogHelper::WarnLog("您的权限不够！", "CheckPath");
					LogHelper::ErrorLog("文件路径：" + temp + "没有不存在,且系统权限不够，无法创建", "", "", 20, "ZPath::CheckPath");
				}
			}
		}
	}

	/**
	 * @brief 检查指定文件是否存在。
	 * @param filePath 文件路径。
	 * @param error 不存在时是否输出错误。
	 * @param showwaring 不存在时是否输出警告。
	 * @param inf 自定义提示信息。
	 * @param FunctionName 调用函数名。
	 * @return 存在返回true，否则false。
	 * @code
	 * bool ok = ZPath::Filexists("./data/input.txt");
	 * @endcode
	 */
	static bool Filexists(const std::string &filePath, bool error, bool showwaring,
						  const std::string &inf, const std::string &FunctionName)
	{
		if (std::filesystem::exists(filePath))
		{
			return true;
		}

		std::string message = inf.empty() ? "当前文件：" + filePath + "不存在！请检查路径" : inf;

		if (error)
		{
			LogHelper::ErrorLog(message, "", "", 20, "ZPath::Filexists");
		}
		else if (showwaring)
		{
			LogHelper::WarnLog(message, "", ConsoleColor::DarkYellow, 0, "ZPath::Filexists");
		}
		else
		{
			LogHelper::WriteLogO(message);
		}
		return false;
	}

	/**
	 * @brief 检查并修正文件路径，必要时自动创建目录。
	 * @param oripath 原始文件路径。
	 * @param Path 目标路径（引用，可能被修改）。
	 * @param createdir 是否自动创建目录。
	 * @param extren 期望的扩展名。
	 * @param outfile 是否为输出文件。
	 * @param error 不存在时是否输出错误。
	 * @details 若目标路径不存在则尝试修正并创建。
	 * @code
	 * std::string path = "result";
	 * CheckError::Filexists("./data/input.txt", path, true, ".dat");
	 * @endcode
	 */
	static void Filexists(const std::string &oripath, std::string &Path, bool createdir,
						  const std::string &extren, bool outfile, bool error)
	{
		CheckPath(Path, extren, false); ///< 检查扩展名
		CheckPath(Path, createdir);		///< 检查并创建目录

		std::filesystem::path originalPath(oripath);
		std::string oriDir = originalPath.parent_path().string();

		if (!std::filesystem::exists(Path))
		{
			std::filesystem::path newPath = std::filesystem::path(oriDir) / Path;
			Path = std::filesystem::absolute(newPath).string();
			CheckPath(Path, createdir);

			if (!outfile && error && !std::filesystem::exists(Path))
			{
				LogHelper::ErrorLog("当前文件： " + oripath +
										" \n   当中的路径参数： " + Path + " 无法读取",
									"", "", 20, "CheckError::Filexists");
			}
		}
		else
		{
			Path = std::filesystem::absolute(Path).string(); ///< 绝对化路径
		}
	}



#pragma endregion 兼容HawtC2之前版本的路径检查方法
};
