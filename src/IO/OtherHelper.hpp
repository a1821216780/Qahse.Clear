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
// 该软件按"原样"提供，不提供任何明示或暗示的保证，包括但不限于适销性、特定用途的适用性、所有权和非侵权。在任何情况下，版权持有人或任何分
// 发软件的人都不对任何索赔、损害或其他责任负责，无论是在合同诉讼、侵权诉讼或其他诉讼中，还是在软件使用或其他交易中产生的。
//
//**********************************************************************************************************************************

// ───────────────────────────────── File Info ─────────────────────────────────
//
// 该文件是 OtherHelper 的单头文件(.hpp)，合并了 OtherHelper.h 和 OtherHelper.cpp 的
// 所有内容，提供各种常用的辅助方法和嵌套实用类，涵盖文件处理、字符串操作、进程管理等
// 功能。该类旨在简化常见的编程任务，提供统一的接口和实现细节封装。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <string>
#include <vector>
#include <array>
#include <queue>
#include <optional>
#include <typeinfo>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <thread>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unordered_map>
#include <Eigen/Dense>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <process.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "version.lib")
#pragma comment(lib, "psapi.lib")
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <dlfcn.h>
#endif

#include "LogHelper.h"
#include "ZPath.hpp"

class OtherHelper
{
public:
	class ProgressBar
	{
	private:
		static constexpr int TotalBars = 30;
		static constexpr char ProgressBarChar = '#';
		static constexpr char BackgroundChar = '-';

		const int _totalIterations;
		int _currentIteration;
		std::string levels;

	public:
		ProgressBar(int totalIterations, int level = 0);

		void UpdateProgress();
	};

	template <typename T>
	struct FieldInfo
	{
		std::string name;
		std::type_info const *type;
		T value;
	};

public:
	static std::string FormortPath(const std::string &path);

	static std::tuple<int, std::string> FindBestMatch(const std::vector<std::string> &strings, const std::string &target);

	static std::string GetMathAcc();

	static void SysRun(const std::string &exePath, const std::string &filePath);

	static void RunCmd(const std::string &command);

	static void RunPowershell(const std::string &cmd);

	static void RunExe(const std::string &path);

	static std::string GetCurrentProjectName();

	static std::string GetCurrentExeName();

	static std::string GetCurrentVersion(const std::string &path = "");

	static std::string GetCurrentAssemblyVersion();

	static std::string GetCurrentBuildMode();

	template <typename T>
	static bool AreEqual(const T &left, const T &right, const std::vector<std::string> &ignoreFieldsName = {});

	static bool isWinform();

	static std::string GetBuildMode();

	static std::vector<std::string> FindFilesWithExtension(const std::string &directoryPath, const std::string &fileExtension);

#ifdef _WIN32
public:
	static void CopyFileW(const std::string &sourceDirectory, const std::string &targetDirectory,
						  const std::string &fileType, bool overwrite = true);
#endif

	static void SetCurrentDirectoryW(const std::string &mainFilePath);

	template <typename T>
	static std::string Tostring(const T &message, char fg = '\t', bool coloum = true);

	template <typename T>
	struct ParseLineArgs
	{
		const std::vector<std::string> &lines;
		const std::string &filename;
		std::tuple<int, std::string> pp;
		std::optional<T> moren = std::nullopt;
		int num = 0;
		char fg = ' ';
		char fg1 = '\t';
		int station = 0;
		const std::vector<std::string> *namelist = nullptr;
		bool row = false;
		const std::string *titleLine = nullptr;
		bool warning = true;
		const std::string &errorInf = "";
	};

	template <typename T>
	static T ParseLine(ParseLineArgs<T> args);

	static std::vector<std::string> ReadOutputWord(const std::vector<std::string> &data, int index, bool deleteSame);

	static std::vector<int> GetMatchingLineIndexes(const std::vector<std::string> &input, const std::string &searchTerm,
												   const std::string &path, bool error = true, bool show = true);

	static bool GetMatchingLineIndexes(const std::string &input, const std::string &searchTerm);

	template <typename T>
	static std::vector<FieldInfo<typename T::value_type>> GetStructFields(const T &str);

	template <typename T>
	static std::vector<std::tuple<std::string, const std::type_info *, T>> GetStructFields(const T &structure);

	template <typename T>
	static std::vector<std::tuple<std::string, const std::type_info *>> GetStructNameAndType(const T &structure);

	template <typename T>
	static bool IsStruct();

	template <typename T>
	static bool IsStructOrStructArray();

	template <typename T>
	static bool IsList(const T &obj);

	template <typename T>
	static std::string GetStructName(const T &structure);

	static int GetThreadCount();

	template <typename T>
	static bool TryConvertToEnum(const std::string &value, T &enumValue);

	template <typename T>
	static T ConvertToEnum(const std::string &value);

	template <typename MatrixType>
	static std::vector<std::string> ConvertMatrixTitleToOutfile(const std::string &title,
																const std::vector<std::string> &rowtitle,
																const std::vector<std::string> &columtitle,
																const MatrixType &matrix);

	static std::string GetFileExtension(const std::string &path);

	static void SetFileExtension(std::string &path, const std::string &newExtension);

	static std::string FillString(const std::string &source, const std::string &spilt, int num = 1);

	static std::string RandomString(int len);

	static std::string CenterText(const std::string &input, int width, char symbol = ' ');

	static void SetCursorPosition(int left, int top);

	static std::tm GetSafeLocalTime(const std::time_t &time);

	static std::tm *GetSafeLocalTime(const std::time_t &time, const bool ptr);

	static std::string GetCurrentYear();
	static int GetCurrentYear(bool temp111);
	static std::string GetCurrentMonth();
	static int GetCurrentMonth(bool temp111);
	static std::string GetCurrentDay();
	static int GetCurrentDay(bool temp111);
	static std::string GetCurrentHour();
	static int GetCurrentHour(bool temp111);
	static std::string GetCurrentMinute();
	static int GetCurrentMinute(bool temp111);
	static std::string GetCurrentSecond();
	static int GetCurrentSecond(bool temp111);
	static std::string GetCurrentTimeW();
	static std::string GetBuildTime();

	template <typename T>
	static std::vector<T> Distinct(const std::vector<T> &values);

	template <typename T>
	static std::vector<T> Duplicates(const std::vector<T> &values);

	template <typename T, size_t N>
	static std::array<T, N> Distinct(const std::array<T, N> &values);

	template <typename T, size_t N>
	static std::vector<T> Duplicates(const std::array<T, N> &values);

	static int LevenshteinDistance(const std::string &a, const std::string &b);

	static std::vector<std::string> SplitString(const std::string &str, const std::string &delimiter);

	static std::string TrimString(const std::string &str);

	static std::string ToLowerString(const std::string &str);

	static bool FileExists(const std::string &path);

	static bool DirectoryExists(const std::string &path);

	static void CreateDirectories(const std::string &path);
};

// ──────────────────────────────────────────────────────────────────────────────
// Inline implementations of non-template member functions
// ──────────────────────────────────────────────────────────────────────────────

inline OtherHelper::ProgressBar::ProgressBar(int totalIterations, int level)
	: _totalIterations(totalIterations), _currentIteration(0)
{
	levels = std::string(level * 2, ' ');
}

inline void OtherHelper::ProgressBar::UpdateProgress()
{
	_currentIteration++;

	float progress = static_cast<float>(_currentIteration) / _totalIterations;
	int barLength = static_cast<int>(progress * TotalBars);

	std::cout << "\r" << levels << "[";

	for (int i = 0; i < TotalBars; ++i)
	{
		if (i < barLength)
		{
			std::cout << ProgressBarChar;
		}
		else
		{
			std::cout << BackgroundChar;
		}
	}

	std::cout << "] " << static_cast<int>(progress * 100) << "% "
			  << "(" << _currentIteration << "/" << _totalIterations << ")";

	if (_currentIteration == _totalIterations)
	{
		std::cout << '\n';
	}

	std::cout.flush();
}

inline std::string OtherHelper::FormortPath(const std::string &path)
{
	try
	{
		std::filesystem::path fsPath(path);
		return std::filesystem::absolute(fsPath).string();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error formatting path: " << e.what() << '\n';
		return path;
	}
}

inline std::tuple<int, std::string> OtherHelper::FindBestMatch(const std::vector<std::string> &strings, const std::string &target)
{
	if (strings.empty())
	{
		return std::make_tuple(-1, "");
	}

	int bestIndex = 0;
	int minDistance = LevenshteinDistance(strings[0], target);

	for (size_t i = 1; i < strings.size(); ++i)
	{
		int distance = LevenshteinDistance(strings[i], target);
		if (distance < minDistance)
		{
			minDistance = distance;
			bestIndex = static_cast<int>(i);
		}
	}

	return std::make_tuple(bestIndex, strings[bestIndex]);
}

inline std::string OtherHelper::GetMathAcc()
{
#if defined(EIGEN_USE_MKL_ALL)
	return "MKL";
#elif defined(EIGEN_USE_BLAS)
	return "Blas";
#elif defined(EIGEN_USE_CUDA)
	return "CUDA";
#elif defined(EIGEN_USE_GPU)
	return "none";
#else
	return "pps";
#endif
}

inline void OtherHelper::SysRun(const std::string &exePath, const std::string &filePath)
{
	std::string command = "\"" + exePath + "\" \"" + filePath + "\"";

	int result = std::system(command.c_str());
	if (result != 0)
	{
		std::cerr << "Failed to execute: " << command << '\n';
	}
}

inline void OtherHelper::RunCmd(const std::string &command)
{
	std::string fullCommand = "cmd /c " + command;
	std::system(fullCommand.c_str());
}

inline void OtherHelper::RunPowershell(const std::string &cmd)
{
	std::string command = "powershell -Command \"" + cmd + "\"";

#ifdef _WIN32
	std::system(command.c_str());
#else
	std::cout << "PowerShell not available on this platform" << '\n';
#endif
}

inline void OtherHelper::RunExe(const std::string &path)
{
	std::string command = "\"" + path + "\"";
	std::system(command.c_str());
}

inline std::string OtherHelper::GetCurrentProjectName()
{
	return "Qahse";
}

inline std::string OtherHelper::GetCurrentExeName()
{
	return "Qahse.exe";
}

inline std::string OtherHelper::GetCurrentVersion(const std::string &path)
{
	return "3.0.0";
}

inline std::string OtherHelper::GetCurrentAssemblyVersion()
{
	return GetCurrentVersion("");
}

inline std::string OtherHelper::GetCurrentBuildMode()
{
#ifdef _DEBUG
	return "Debug";
#else
	return "Release";
#endif
}

inline bool OtherHelper::isWinform()
{
	return false;
}

inline std::string OtherHelper::GetBuildMode()
{
#ifdef _WIN64
	return "_x64";
#elif defined(_WIN32)
	return "_x32";
#else
	return sizeof(void *) == 8 ? "_x64" : "_x32";
#endif
}

inline std::vector<std::string> OtherHelper::FindFilesWithExtension(const std::string &directoryPath, const std::string &fileExtension)
{
	std::vector<std::string> result;

	try
	{
		if (DirectoryExists(directoryPath))
		{
			for (const auto &entry : std::filesystem::recursive_directory_iterator(directoryPath))
			{
				if (entry.is_regular_file())
				{
					std::string ext = entry.path().extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

					std::string targetExt = fileExtension;
					std::transform(targetExt.begin(), targetExt.end(), targetExt.begin(), ::tolower);

					if (ext == targetExt)
					{
						result.push_back(ZPath::GetABSPath(entry.path().string()));
					}
				}
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error finding files: " << e.what() << '\n';
	}

	return result;
}

#ifdef _WIN32
inline void OtherHelper::CopyFileW(const std::string &sourceDirectory, const std::string &targetDirectory,
								   const std::string &fileType, bool overwrite)
{
	try
	{
		if (!DirectoryExists(targetDirectory))
		{
			CreateDirectories(targetDirectory);
		}

		std::filesystem::copy_options options = std::filesystem::copy_options::recursive;
		if (overwrite)
		{
			options |= std::filesystem::copy_options::overwrite_existing;
		}

		if (fileType == "*")
		{
			std::filesystem::copy(sourceDirectory, targetDirectory, options);
		}
		else
		{
			for (const auto &entry : std::filesystem::recursive_directory_iterator(sourceDirectory))
			{
				if (entry.is_regular_file())
				{
					std::string ext = entry.path().extension().string();
					if (ext == fileType || ("*" + ext) == fileType)
					{
						std::filesystem::path relativePath = std::filesystem::relative(entry.path(), sourceDirectory);
						std::filesystem::path targetPath = std::filesystem::path(targetDirectory) / relativePath;

						CreateDirectories(targetPath.parent_path().string());
						std::filesystem::copy_file(entry.path(), targetPath,
												   overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none);
					}
				}
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error copying files: " << e.what() << '\n';
	}
}
#endif

inline void OtherHelper::SetCurrentDirectoryW(const std::string &mainFilePath)
{
	try
	{
		std::filesystem::path filePath(mainFilePath);
		std::filesystem::path dirPath = filePath.parent_path();

		if (DirectoryExists(dirPath.string()))
		{
			std::filesystem::current_path(dirPath);
		}
		else
		{
			std::cerr << "Directory does not exist: " << dirPath << '\n';
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error setting current directory: " << e.what() << '\n';
	}
}

inline std::vector<std::string> OtherHelper::ReadOutputWord(const std::vector<std::string> &data, int index, bool deleteSame)
{
	std::vector<std::string> result;

	for (size_t i = static_cast<size_t>(index); i < data.size(); ++i)
	{
		std::string line = TrimString(data[i]);

		if (line == "END" || line == "end")
		{
			break;
		}

		if (!line.empty())
		{
			result.push_back(line);
		}
	}

	if (deleteSame)
	{
		std::sort(result.begin(), result.end());
		result.erase(std::unique(result.begin(), result.end()), result.end());
	}

	return result;
}

inline std::vector<int> OtherHelper::GetMatchingLineIndexes(const std::vector<std::string> &input, const std::string &searchTerm,
															const std::string &path, bool error, bool show)
{
	std::vector<int> result;

	for (size_t i = 0; i < input.size(); ++i)
	{
		if (input[i].find(searchTerm) != std::string::npos)
		{
			result.push_back(static_cast<int>(i));
		}
	}

	if (result.empty())
	{
		if (error)
		{
			std::string inf = "Search term '" + searchTerm + "' not found in file: " + path;
			LogHelper::ErrorLog(inf, "", "", 20, "OtherHelper::GetMatchingLineIndexes");
		}
		else if (show)
		{
			std::string inf = "Search term '" + searchTerm + "' not found in file: " + path;
			LogHelper::WarnLog(inf, "", ConsoleColor::DarkYellow, 20, "OtherHelper::GetMatchingLineIndexes");
		}
		else
		{
			std::string inf = "Search term '" + searchTerm + "' not found in file: " + path;
			LogHelper::WriteLogO(inf);
		}
		result.push_back(-1);
	}

	return result;
}

inline bool OtherHelper::GetMatchingLineIndexes(const std::string &input, const std::string &searchTerm)
{
	return input.find(searchTerm) != std::string::npos;
}

inline int OtherHelper::GetThreadCount()
{
	return static_cast<int>(std::thread::hardware_concurrency());
}

inline std::string OtherHelper::GetFileExtension(const std::string &path)
{
	std::filesystem::path filePath(path);
	std::string extension = filePath.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	return extension;
}

inline void OtherHelper::SetFileExtension(std::string &path, const std::string &newExtension)
{
	try
	{
		std::filesystem::path oldPath(path);
		std::filesystem::path newPath = oldPath;
		newPath.replace_extension(newExtension);

		if (FileExists(oldPath.string()))
		{
			std::filesystem::rename(oldPath, newPath);
			path = newPath.string();
		}
		else
		{
			std::cerr << "File does not exist: " << path << '\n';
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error changing file extension: " << e.what() << '\n';
	}
}

inline std::string OtherHelper::FillString(const std::string &source, const std::string &spilt, int num)
{
	if (source.empty() || spilt.empty() || num <= 0)
	{
		return source;
	}
	std::string result = "";
	for (size_t i = 0; i < source.length(); ++i)
	{
		result += source[i];
		if (i < source.length() - 1)
		{
			for (int j = 0; j < num; ++j)
			{
				result += spilt;
			}
		}
	}
	return result;
}

inline std::string OtherHelper::RandomString(int len)
{
	const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	std::string result;
	result.reserve(len);

	static bool seeded = false;
	if (!seeded)
	{
		std::srand(static_cast<unsigned int>(std::time(nullptr)));
		seeded = true;
	}

	for (int i = 0; i < len; ++i)
	{
		result += chars[std::rand() % chars.size()];
	}

	return result;
}

inline std::string OtherHelper::CenterText(const std::string &input, int width, char symbol)
{
	if (static_cast<int>(input.length()) >= width)
	{
		return input;
	}

	int totalPadding = width - static_cast<int>(input.length());
	int leftPadding = totalPadding / 2;
	int rightPadding = totalPadding - leftPadding;

	return std::string(leftPadding, symbol) + input + std::string(rightPadding, symbol);
}

inline void OtherHelper::SetCursorPosition(int left, int top)
{
	if (left != 0)
	{
		if (left > 0)
		{
			std::cout << "\033[" << left << "C";
		}
		else
		{
			std::cout << "\033[" << (-left) << "D";
		}
	}
	if (top != 0)
	{
		if (top > 0)
		{
			std::cout << "\033[" << top << "B";
		}
		else
		{
			std::cout << "\033[" << (-top) << "A";
		}
	}
}

inline std::tm OtherHelper::GetSafeLocalTime(const std::time_t &time)
{
	std::tm tm_result = {};
#ifdef _WIN32
	localtime_s(&tm_result, &time);
#else
	localtime_r(&time, &tm_result);
#endif
	return tm_result;
}

inline std::tm *OtherHelper::GetSafeLocalTime(const std::time_t &time, const bool ptr)
{
	static thread_local std::tm tm_result = {};
#ifdef _WIN32
	localtime_s(&tm_result, &time);
#else
	localtime_r(&time, &tm_result);
#endif
	return &tm_result;
}

inline std::string OtherHelper::GetCurrentYear()
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return std::to_string(tm.tm_year + 1900);
}

inline int OtherHelper::GetCurrentYear(bool temp111)
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return tm.tm_year + 1900;
}

inline std::string OtherHelper::GetCurrentMonth()
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return std::to_string(tm.tm_mon + 1);
}

inline int OtherHelper::GetCurrentMonth(bool temp111)
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return tm.tm_mon + 1;
}

inline std::string OtherHelper::GetCurrentDay()
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return std::to_string(tm.tm_mday);
}

inline int OtherHelper::GetCurrentDay(bool temp111)
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return tm.tm_mday;
}

inline std::string OtherHelper::GetCurrentHour()
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return std::to_string(tm.tm_hour);
}

inline int OtherHelper::GetCurrentHour(bool temp111)
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return tm.tm_hour;
}

inline std::string OtherHelper::GetCurrentMinute()
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return std::to_string(tm.tm_min);
}

inline int OtherHelper::GetCurrentMinute(bool temp111)
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return tm.tm_min;
}

inline std::string OtherHelper::GetCurrentSecond()
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return std::to_string(tm.tm_sec);
}

inline int OtherHelper::GetCurrentSecond(bool temp111)
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);
	return tm.tm_sec;
}

inline std::string OtherHelper::GetCurrentTimeW()
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = GetSafeLocalTime(time_t);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
	return oss.str();
}

inline std::string OtherHelper::GetBuildTime()
{
	return __DATE__ " " __TIME__;
}

inline int OtherHelper::LevenshteinDistance(const std::string &a, const std::string &b)
{
	const size_t m = a.length();
	const size_t n = b.length();

	if (m == 0)
		return static_cast<int>(n);
	if (n == 0)
		return static_cast<int>(m);

	std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

	for (size_t i = 0; i <= m; ++i)
	{
		dp[i][0] = static_cast<int>(i);
	}
	for (size_t j = 0; j <= n; ++j)
	{
		dp[0][j] = static_cast<int>(j);
	}

	for (size_t i = 1; i <= m; ++i)
	{
		for (size_t j = 1; j <= n; ++j)
		{
			int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
			dp[i][j] = std::min({dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost});
		}
	}

	return dp[m][n];
}

inline std::vector<std::string> OtherHelper::SplitString(const std::string &str, const std::string &delimiter)
{
	std::vector<std::string> tokens;
	size_t start = 0;
	size_t end = str.find(delimiter);

	while (end != std::string::npos)
	{
		tokens.push_back(str.substr(start, end - start));
		start = end + delimiter.length();
		end = str.find(delimiter, start);
	}

	tokens.push_back(str.substr(start));
	return tokens;
}

inline std::string OtherHelper::TrimString(const std::string &str)
{
	const std::string whitespace = " \t\n\r\f\v";

	size_t start = str.find_first_not_of(whitespace);
	if (start == std::string::npos)
	{
		return "";
	}

	size_t end = str.find_last_not_of(whitespace);
	return str.substr(start, end - start + 1);
}

inline std::string OtherHelper::ToLowerString(const std::string &str)
{
	std::string result = str;
	std::transform(result.begin(), result.end(), result.begin(), ::tolower);
	return result;
}

inline bool OtherHelper::FileExists(const std::string &path)
{
	return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

inline bool OtherHelper::DirectoryExists(const std::string &path)
{
	return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

inline void OtherHelper::CreateDirectories(const std::string &path)
{
	try
	{
		std::filesystem::create_directories(path);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error creating directories: " << e.what() << '\n';
	}
}

// ──────────────────────────────────────────────────────────────────────────────
// Template implementations
// ──────────────────────────────────────────────────────────────────────────────

template <typename T>
bool OtherHelper::AreEqual(const T &left, const T &right, const std::vector<std::string> &ignoreFieldsName)
{
	if constexpr (std::is_arithmetic_v<T>)
	{
		return left == right;
	}
	else
	{
		return left == right;
	}
}

template <typename T>
T OtherHelper::ParseLine(ParseLineArgs<T> args)
{
	try
	{
		int index = std::get<0>(args.pp);
		std::string key = std::get<1>(args.pp);

		if (index >= 0 && index < static_cast<int>(args.lines.size()))
		{
			std::string line = args.lines[index];

			size_t keyPos = line.find(key);
			if (keyPos != std::string::npos)
			{
				std::string valuePart = line.substr(keyPos + key.length());

				valuePart = TrimString(valuePart);

				std::vector<std::string> tokens = SplitString(valuePart, std::string(1, args.fg));

				if (args.station < static_cast<int>(tokens.size()))
				{
					std::string valueStr = TrimString(tokens[args.station]);

					if constexpr (std::is_same_v<T, int>)
					{
						return std::stoi(valueStr);
					}
					else if constexpr (std::is_same_v<T, double>)
					{
						return std::stod(valueStr);
					}
					else if constexpr (std::is_same_v<T, float>)
					{
						return std::stof(valueStr);
					}
					else if constexpr (std::is_same_v<T, std::string>)
					{
						return valueStr;
					}
					else
					{
						std::istringstream iss(valueStr);
						T result;
						iss >> result;
						return result;
					}
				}
			}
		}

		if (args.moren.has_value())
		{
			return args.moren.value();
		}
		else
		{
			if (args.warning)
			{
				std::cerr << "Warning: Failed to parse line in " << args.filename
						  << " for key: " << std::get<1>(args.pp) << '\n';
			}
			return T{};
		}
	}
	catch (const std::exception &e)
	{
		if (args.warning)
		{
			std::cerr << "Error parsing line in " << args.filename << ": " << e.what() << '\n';
		}

		if (args.moren.has_value())
		{
			return args.moren.value();
		}
		else
		{
			return T{};
		}
	}
}

template <typename T>
std::vector<std::tuple<std::string, const std::type_info *, T>> OtherHelper::GetStructFields(const T &structure)
{
	std::vector<std::tuple<std::string, const std::type_info *, T>> result;
	return result;
}

template <typename T>
std::vector<std::tuple<std::string, const std::type_info *>> OtherHelper::GetStructNameAndType(const T &structure)
{
	std::vector<std::tuple<std::string, const std::type_info *>> result;
	return result;
}

template <typename T>
bool OtherHelper::IsStruct()
{
	return std::is_class_v<T> && !std::is_pointer_v<T> && !std::is_reference_v<T>;
}

template <typename T>
bool OtherHelper::IsStructOrStructArray()
{
	if constexpr (std::is_array_v<T>)
	{
		using ElementType = std::remove_extent_t<T>;
		return IsStruct<ElementType>();
	}
	else
	{
		return IsStruct<T>();
	}
}

template <typename T>
bool OtherHelper::IsList(const T &obj)
{
	return false;
}

template <typename T>
std::string OtherHelper::GetStructName(const T &structure)
{
	return typeid(T).name();
}

template <typename T>
bool OtherHelper::TryConvertToEnum(const std::string &value, T &enumValue)
{
	try
	{
		enumValue = ConvertToEnum<T>(value);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

template <typename T>
T OtherHelper::ConvertToEnum(const std::string &value)
{
	throw std::invalid_argument("Enum conversion not implemented for this type");
}

template <typename MatrixType>
std::vector<std::string> OtherHelper::ConvertMatrixTitleToOutfile(const std::string &title,
																  const std::vector<std::string> &rowtitle,
																  const std::vector<std::string> &columtitle,
																  const MatrixType &matrix)
{
	std::vector<std::string> output;

	std::string titleLine = title;
	for (const auto &rowTitle : rowtitle)
	{
		titleLine += "\t" + rowTitle;
	}
	output.push_back(titleLine);

	for (size_t i = 0; i < columtitle.size(); ++i)
	{
		std::string dataLine = columtitle[i];
		for (size_t j = 0; j < rowtitle.size(); ++j)
		{
			dataLine += "\t0";
		}
		output.push_back(dataLine);
	}

	return output;
}

template <typename T>
std::vector<T> OtherHelper::Distinct(const std::vector<T> &values)
{
	std::vector<T> result = values;
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

template <typename T>
std::vector<T> OtherHelper::Duplicates(const std::vector<T> &values)
{
	std::unordered_map<T, int> counts;
	std::vector<T> duplicates;

	for (const auto &value : values)
	{
		counts[value]++;
	}

	for (const auto &pair : counts)
	{
		if (pair.second > 1)
		{
			duplicates.push_back(pair.first);
		}
	}

	return duplicates;
}

template <typename T, size_t N>
std::array<T, N> OtherHelper::Distinct(const std::array<T, N> &values)
{
	std::vector<T> temp(values.begin(), values.end());
	auto distinctVec = Distinct(temp);

	std::array<T, N> result{};
	size_t copySize = std::min<size_t>(distinctVec.size(), N);
	std::copy(distinctVec.begin(), distinctVec.begin() + copySize, result.begin());

	return result;
}

template <typename T, size_t N>
std::vector<T> OtherHelper::Duplicates(const std::array<T, N> &values)
{
	std::vector<T> temp(values.begin(), values.end());
	return Duplicates(temp);
}

// ──────────────────────────────────────────────────────────────────────────────
// Explicit template specializations with inline keyword (ODR-safe in header-only)
// ──────────────────────────────────────────────────────────────────────────────

template <>
inline std::string OtherHelper::Tostring<int>(const int &message, char fg, bool coloum)
{
	return std::to_string(message);
}

template <>
inline std::string OtherHelper::Tostring<bool>(const bool &message, char fg, bool coloum)
{
	return std::to_string(message);
}

template <>
inline std::string OtherHelper::Tostring<double>(const double &message, char fg, bool coloum)
{
	return std::to_string(message);
}

template <>
inline std::string OtherHelper::Tostring<float>(const float &message, char fg, bool coloum)
{
	return std::to_string(message);
}

template <>
inline std::string OtherHelper::Tostring<std::string>(const std::string &message, char fg, bool coloum)
{
	return message;
}

template <>
inline std::string OtherHelper::Tostring<Eigen::VectorXd>(const Eigen::VectorXd &message, char fg, bool coloum)
{
	std::ostringstream oss;
	if (coloum)
	{
		for (int i = 0; i < message.size(); ++i)
		{
			oss << message(i);
			if (i < message.size() - 1)
			{
				oss << fg;
			}
		}
	}
	else
	{
		for (int i = 0; i < message.size(); ++i)
		{
			oss << message(i);
			if (i < message.size() - 1)
			{
				oss << fg;
			}
		}
	}
	return oss.str();
}

template <>
inline std::string OtherHelper::Tostring<Eigen::VectorXf>(const Eigen::VectorXf &message, char fg, bool coloum)
{
	std::ostringstream oss;
	if (coloum)
	{
		for (int i = 0; i < message.size(); ++i)
		{
			oss << message(i);
			if (i < message.size() - 1)
			{
				oss << fg;
			}
		}
	}
	else
	{
		for (int i = 0; i < message.size(); ++i)
		{
			oss << message(i);
		}
	}
	return oss.str();
}

template <>
inline std::string OtherHelper::Tostring<Eigen::MatrixXd>(const Eigen::MatrixXd &message, char fg, bool coloum)
{
	std::ostringstream oss;

	for (int i = 0; i < message.rows(); ++i)
	{
		for (int j = 0; j < message.cols(); ++j)
		{
			oss << message(i, j);
			if (j < message.cols() - 1)
			{
				oss << fg;
			}
		}
		if (i < message.rows() - 1)
		{
			oss << '\n';
		}
	}

	return oss.str();
}

template <>
inline std::string OtherHelper::Tostring<Eigen::MatrixXf>(const Eigen::MatrixXf &message, char fg, bool coloum)
{
	std::ostringstream oss;

	for (int i = 0; i < message.rows(); ++i)
	{
		for (int j = 0; j < message.cols(); ++j)
		{
			oss << message(i, j);
			if (j < message.cols() - 1)
			{
				oss << fg;
			}
		}
		if (i < message.rows() - 1)
		{
			oss << '\n';
		}
	}

	return oss.str();
}

template <>
inline std::string OtherHelper::Tostring<std::vector<double>>(const std::vector<double> &message, char fg, bool coloum)
{
	std::ostringstream oss;
	if (coloum)
	{
		for (size_t i = 0; i < message.size(); i++)
		{
			oss << std::to_string(message.at(i));
			if (i < message.size() - 1)
			{
				oss << '\n';
			}
		}
	}
	else
	{
		for (size_t i = 0; i < message.size(); i++)
		{
			oss << std::to_string(message.at(i));
			if (i < message.size() - 1)
			{
				oss << fg;
			}
		}
	}
	return oss.str();
}

template <>
inline std::string OtherHelper::Tostring<std::vector<float>>(const std::vector<float> &message, char fg, bool coloum)
{
	std::ostringstream oss;
	if (coloum)
	{
		for (size_t i = 0; i < message.size(); i++)
		{
			oss << std::to_string(message.at(i));
			if (i < message.size() - 1)
			{
				oss << '\n';
			}
		}
	}
	else
	{
		for (size_t i = 0; i < message.size(); i++)
		{
			oss << std::to_string(message.at(i));
			if (i < message.size() - 1)
			{
				oss << fg;
			}
		}
	}
	return oss.str();
}

template <>
inline std::string OtherHelper::Tostring<std::vector<int>>(const std::vector<int> &message, char fg, bool coloum)
{
	std::ostringstream oss;
	if (coloum)
	{
		for (size_t i = 0; i < message.size(); i++)
		{
			oss << std::to_string(message.at(i));
			if (i < message.size() - 1)
			{
				oss << '\n';
			}
		}
	}
	else
	{
		for (size_t i = 0; i < message.size(); i++)
		{
			oss << std::to_string(message.at(i));
			if (i < message.size() - 1)
			{
				oss << fg;
			}
		}
	}
	return oss.str();
}

template <>
inline std::string OtherHelper::Tostring<std::vector<bool>>(const std::vector<bool> &message, char fg, bool coloum)
{
	std::ostringstream oss;
	if (coloum)
	{
		for (size_t i = 0; i < message.size(); i++)
		{
			oss << std::to_string(message.at(i));
			if (i < message.size() - 1)
			{
				oss << '\n';
			}
		}
	}
	else
	{
		for (size_t i = 0; i < message.size(); i++)
		{
			oss << std::to_string(message.at(i));
			if (i < message.size() - 1)
			{
				oss << fg;
			}
		}
	}
	return oss.str();
}