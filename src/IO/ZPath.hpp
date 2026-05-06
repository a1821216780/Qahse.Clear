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
#include "LocaleString.hpp"

/**
 * @class ZPath
 * @brief 类似 C# System.IO.Path 的跨平台路径工具类。
 * @details 支持绝对路径处理、目录名/文件名提取、扩展名处理、路径拼接、
 *          以及常见路径规范化操作。所有静态方法基于 std::filesystem 实现，
 *          自动适配 Windows/Linux/macOS 路径分隔符。
 */
class ZPath
{
public:
	/**
	 * @brief 规范化路径分隔符为当前平台首选格式。
	 * @param path 输入路径。
	 * @return 规范化后的路径字符串。
	 * @note 空路径返回空字符串。
	 * @code auto p = ZPath::NormalizeSeparators("a/b\\c"); @endcode
	 */
	static std::string NormalizeSeparators(const std::string &path)
	{
		return std::filesystem::path(path).make_preferred().string();
	}

	/**
	 * @brief 判断路径是否为空。
	 * @param path 输入路径。
	 * @return 空字符串返回 true。
	 * @note 空白字符（如空格）不算空路径。
	 * @code bool empty = ZPath::IsEmpty(""); @endcode
	 */
	static bool IsEmpty(const std::string &path)
	{
		return path.empty();
	}

	/**
	 * @brief 判断是否为绝对路径。
	 * @param path 输入路径。
	 * @return 绝对路径返回 true。
	 * @note Windows 下以盘符或 \\ 开头的路径为绝对路径；Linux/macOS 下以 / 开头为绝对路径。
	 * @code bool abs = ZPath::IsAbsolute("/home/user"); @endcode
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
	 * @note 空路径直接返回空字符串；转换异常时回退为规范化原始路径。
	 * @code auto abs = ZPath::GetABSPath("../data"); @endcode
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
	 * @brief 获取标准化后的完整路径（消除 . 和 ..）。
	 * @param path 输入路径。
	 * @return 归一化路径字符串。
	 * @note 不会转换成绝对路径，仅做词法规范化。
	 * @code auto full = ZPath::GetFullPath("a/./b/../c"); @endcode
	 */
	static std::string GetFullPath(const std::string &path)
	{
		return std::filesystem::path(path).lexically_normal().string();
	}

	/**
	 * @brief 获取目录名（父路径）。
	 * @param path 输入路径。
	 * @return 父目录路径字符串，若已是根目录则可能为空。
	 * @note 路径末尾分隔符会被忽略；根目录的父目录为空。
	 * @code auto dir = ZPath::GetDirectoryName("/home/file.txt"); @endcode
	 */
	static std::string GetDirectoryName(const std::string &path)
	{
		return std::filesystem::path(path).parent_path().string();
	}

	/**
	 * @brief 获取文件名（含扩展名）。
	 * @param path 输入路径。
	 * @return 文件名字符串。
	 * @note 若路径以分隔符结尾，结果可能为空。
	 * @code auto name = ZPath::GetFileName("/tmp/data.txt"); @endcode
	 */
	static std::string GetFileName(const std::string &path)
	{
		return std::filesystem::path(path).filename().string();
	}

	/**
	 * @brief 获取不含扩展名的文件名。
	 * @param path 输入路径。
	 * @return 不含扩展名的文件名字符串。
	 * @note 无扩展名时返回完整文件名；仅有点号开头（如 .gitignore）的 stem 返回完整名。
	 * @code auto stem = ZPath::GetFileNameWithoutExtension("test.cpp"); @endcode
	 */
	static std::string GetFileNameWithoutExtension(const std::string &path)
	{
		return std::filesystem::path(path).stem().string();
	}

	/**
	 * @brief 获取文件扩展名。
	 * @param path 输入路径。
	 * @return 扩展名字符串（包含前导点，如 ".cpp"）。
	 * @note 无扩展名时返回空字符串；多扩展名（如 .tar.gz）仅返回最后一个。
	 * @code auto ext = ZPath::GetExtension("photo.jpg"); @endcode
	 */
	static std::string GetExtension(const std::string &path)
	{
		return std::filesystem::path(path).extension().string();
	}

	/**
	 * @brief 判断路径是否包含扩展名。
	 * @param path 输入路径。
	 * @return 包含扩展名返回 true。
	 * @note 点号开头的隐藏文件（如 .bashrc）会被视为扩展名为 .bashrc。
	 * @code bool hasExt = ZPath::HasExtension("doc.pdf"); @endcode
	 */
	static bool HasExtension(const std::string &path)
	{
		return !GetExtension(path).empty();
	}

	/**
	 * @brief 删除扩展名。
	 * @param path 输入路径。
	 * @return 去掉扩展名后的路径。
	 * @note 无扩展名时返回原路径；仅移除最后一个扩展名。
	 * @code auto noExt = ZPath::RemoveExtension("file.csv"); @endcode
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
	 * @note 若 right 为绝对路径则直接返回 right。
	 * @code auto combined = ZPath::Combine("/home", "docs"); @endcode
	 */
	static std::string Combine(const std::string &left, const std::string &right)
	{
		return (std::filesystem::path(left) / std::filesystem::path(right)).string();
	}

	/**
	 * @brief 以当前平台分隔符拼接多个路径片段。
	 * @param parts 路径片段列表（initializer_list）。
	 * @return 拼接后的路径。
	 * @note 若中间某段为绝对路径，后续片段仅追加到该段之后。
	 * @code auto p = ZPath::Combine({"/home", "user", "doc.txt"}); @endcode
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
	 * @note 空路径直接返回；已以分隔符结尾则不做修改。
	 * @code auto dir = ZPath::EnsureTrailingSeparator("/tmp/data"); @endcode
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
	 * @note 空路径直接返回；保留根目录形式（如 "C:\\" 变为 "C:"）。
	 * @code auto clean = ZPath::RemoveTrailingSeparator("dir/"); @endcode
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
	 * @return 单字符分隔符字符串（Windows 为 "\\"，Linux/macOS 为 "/"）。
	 * @note 无参数，始终返回当前编译平台的分隔符。
	 * @code auto sep = ZPath::GetPathSeparator(); @endcode
	 */
	static std::string GetPathSeparator()
	{
		return std::string(1, std::filesystem::path::preferred_separator);
	}

	/**
	 * @brief 判断路径是否存在（文件或目录均可）。
	 * @param path 输入路径。
	 * @return 存在返回 true。
	 * @note 不区分文件与目录，只要文件系统条目存在即返回 true。
	 * @code bool ok = ZPath::Exists("/tmp/test"); @endcode
	 */
	static bool Exists(const std::string &path)
	{
		return std::filesystem::exists(std::filesystem::path(path));
	}

	/**
	 * @brief 判断路径是否为目录。
	 * @param path 输入路径。
	 * @return 目录返回 true。
	 * @note 不存在的路径返回 false；需先通过 Exists 确认存在。
	 * @code bool isDir = ZPath::IsDirectory("/tmp"); @endcode
	 */
	static bool IsDirectory(const std::string &path)
	{
		return std::filesystem::is_directory(std::filesystem::path(path));
	}

	/**
	 * @brief 判断路径是否为普通文件。
	 * @param path 输入路径。
	 * @return 普通文件返回 true。
	 * @note 目录、符号链接（指向目录）、不存在的路径均返回 false。
	 * @code bool isFile = ZPath::IsFile("doc.pdf"); @endcode
	 */
	static bool IsFile(const std::string &path)
	{
		return std::filesystem::is_regular_file(std::filesystem::path(path));
	}

	/**
	 * @brief 按原路径或补充后缀查找已存在文件。
	 * @param filePath 原始文件路径。
	 * @param fallbackExtension 原始路径不存在时尝试追加的后缀（含点，如 ".txt"）。
	 * @return 可打开的实际路径；如果都不存在，则返回原始路径。
	 * @note 仅当原路径无扩展名时才会尝试追加后缀。
	 * @code auto real = ZPath::ResolveExistingPath("shader", ".glsl"); @endcode
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

	/**
	 * @brief 解析相对于基准文件路径的相对路径。
	 * @param baseFilePath 基准文件路径（含文件名）。
	 * @param relative 相对路径字符串。
	 * @return 解析后的绝对/规范化路径；若 relative 为空则返回空字符串。
	 * @note 若 relative 本身为绝对路径则直接返回 relative。
	 * @code auto resolved = ZPath::ResolvePath("/app/conf.ini", "../data"); @endcode
	 */
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
	 * @return 无返回值。
	 * @details 若目录不存在且createDir为true，则尝试自动创建。
	 * @note 权限不足时输出警告和错误日志，不会抛出异常。
	 * @code ZPath::CheckDir("./output", true); @endcode
	 */
	static void CheckDir(const std::string &name, bool createDir)
	{
		if (!Exists(name))
		{
			LogHelper::WriteLogO(std::string(L_ZPATH_DirNotExist) + ": " + name);
			if (createDir)
			{
				try
				{
					std::filesystem::create_directories(name); ///< 自动创建目录
				}
				catch (const std::exception &)
				{
					LogHelper::WarnLog(L_ZPATH_PermissionDenied, "CheckDir");
					LogHelper::ErrorLog(std::string(L_ZPATH_PermissionDenied) + ": " + name, "", "", 20, "ZPath::CheckDir");
				}
			}
		}
	}

	/**
	 * @brief 检查函数指针是否为空。
	 * @param ptr 待检查的指针。
	 * @param name 函数名。
	 * @param dllpath DLL路径。
	 * @return 无返回值。
	 * @details 若ptr为nullptr则输出错误日志。
	 * @note 仅在 ptr == nullptr 时输出日志，不会抛出异常。
	 * @code ZPath::CheckInptr(funcPtr, "MyFunc", "./lib.dll"); @endcode
	 */
	static void CheckInptr(void *ptr, const std::string &name, const std::string &dllpath)
	{
		if (ptr == nullptr)
		{
			LogHelper::ErrorLog(std::string(L_ZPATH_CantFindFunc) + ": " + name + " in dll path:\n  " + dllpath, "", "", 20, "ZPath::CheckInptr");
		}
	}

	/**
	 * @brief 检查并修正文件路径的扩展名。
	 * @param path 文件路径（引用，可能被修改为追加扩展名后的结果）。
	 * @param extension1 期望的扩展名（含点，如".txt"）。
	 * @param show 是否输出警告日志。
	 * @param information 自定义警告信息前缀。
	 * @return 无返回值。
	 * @details 若扩展名为空则自动补全，若不一致则输出错误日志。
	 * @note 仅输出日志，不会抛出异常；扩展名不一致时仅报错不修正。
	 * @code std::string p="result"; ZPath::CheckPath(p,".dat",true); // p变为"result.dat" @endcode
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
LogHelper::ErrorLog(std::string(L_ZPATH_ExtMismatch) + ": " + extension + " vs " + extension1,
							"", "", 20, "ZPath::CheckPath");
		}
	}

	/**
	 * @brief 检查指定文件路径的父目录是否存在，不存在时可选择自动创建。
	 * @param name 文件路径。
	 * @param createdir 是否自动创建父目录。
	 * @return 无返回值。
	 * @details 若父目录不存在且createdir为true，则尝试使用 create_directories 自动创建。
	 * @note 父目录路径为空时输出错误日志并直接返回；权限不足时输出警告。
	 * @code ZPath::CheckPath("./data/test.txt", true); @endcode
	 */
	static void CheckPath(const std::string &name, bool createdir)
	{
		std::string name1 = GetABSPath(name); ///< 获取绝对路径
		// std::filesystem::path path(name1);
		std::string temp = GetDirectoryName(name1); ///< 父目录路径

		if (temp.empty())
		{
			LogHelper::ErrorLog(std::string(L_ZPATH_EmptyParent) + ": " + name1, "", "", 20, "ZPath::CheckPath");
			return;
		}

		if (!std::filesystem::exists(temp))
		{
			LogHelper::WriteLogO(std::string(L_ZPATH_DirNotExist) + ": " + temp);
			if (createdir)
			{
				try
				{
					std::filesystem::create_directories(temp); ///< 自动创建父目录
				}
				catch (const std::exception &)
				{
					LogHelper::WarnLog(L_ZPATH_PermissionDenied, "CheckPath");
					LogHelper::ErrorLog(std::string(L_ZPATH_PermissionDenied) + ": " + temp, "", "", 20, "ZPath::CheckPath");
				}
			}
		}
	}

	/**
	 * @brief 检查指定文件是否存在，并输出相应的日志。
	 * @param filePath 文件路径。
	 * @param error 不存在时是否输出错误级别日志。
	 * @param showwaring 不存在时是否输出警告级别日志。
	 * @param inf 自定义提示信息（为空则使用默认消息）。
	 * @param FunctionName 调用函数名（用于日志标识）。
	 * @return 存在返回 true，否则返回 false。
	 * @note error 与 showwaring 同时为 true 时优先输出 error 日志。
	 * @code bool ok = ZPath::Filexists("./input.txt",true,false,"","MyFunc"); @endcode
	 */
	static bool Filexists(const std::string &filePath, bool error, bool showwaring,
						  const std::string &inf, const std::string &FunctionName)
	{
		if (std::filesystem::exists(filePath))
		{
			return true;
		}

		std::string message = inf.empty() ? std::string(L_ZFILE_FileNotFound) + ": " + filePath : inf;

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
	 * @param oripath 原始文件路径（用于参照目录）。
	 * @param Path 目标路径（引用，可能被修改为绝对路径）。
	 * @param createdir 是否自动创建目录。
	 * @param extren 期望的扩展名（含点，如".dat"）。
	 * @param outfile 是否为输出文件（true 则跳过错误日志）。
	 * @param error 不存在时是否输出错误日志。
	 * @return 无返回值。
	 * @details 若目标路径不存在则尝试以 oripath 的目录为基准修正并创建。
	 * @note 目标路径存在时直接绝对化；不存在时拼接到 oripath 目录下重试。
	 * @code std::string p="result"; ZPath::Filexists("./data/input.txt",p,true,".dat",false,true); @endcode
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
