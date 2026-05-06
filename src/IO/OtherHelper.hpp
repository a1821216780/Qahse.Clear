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
#include "LocaleString.hpp"

/**
 * @class OtherHelper
 * @brief 通用帮助类，提供文件处理、字符串操作、进程管理、时间日期、枚举转换等常用辅助方法
 * @details 该类是一个静态工具类，所有方法均为静态方法，无需实例化即可使用。
 *          包含嵌套类 ProgressBar（控制台进度条）和 FieldInfo（结构体字段信息模板）。
 */
class OtherHelper
{
public:
	/**
	 * @class ProgressBar
	 * @brief 控制台进度条显示类，用于仿真或循环进度的可视化输出
	 * @details 通过 UpdateProgress() 更新进度条，在控制台以百分比和条形图方式显示当前进度。
	 */
	class ProgressBar
	{
	private:
		static constexpr int TotalBars = 30;          ///< 进度条总长度（字符数）
		static constexpr char ProgressBarChar = '#';  ///< 已完成进度显示字符
		static constexpr char BackgroundChar = '-';   ///< 未完成进度背景字符

		const int _totalIterations;                   ///< 总迭代次数
		int _currentIteration;                        ///< 当前迭代次数（0 起始）
		std::string levels;                           ///< 缩进字符串（用于多层级进度条对齐）

	public:
		/**
		 * @brief 构造进度条对象
		 * @param totalIterations 总迭代次数
		 * @param level 缩进级别（每级 2 空格），默认 0
		 */
		ProgressBar(int totalIterations, int level = 0);

		/**
		 * @brief 更新进度条显示（每次迭代调用一次）
		 * @details 在控制台刷新当前进度百分比和条形图，迭代完成时自动换行。
		 */
		void UpdateProgress();
	};

	/**
	 * @struct FieldInfo
	 * @brief 结构体字段信息模板，记录字段名称、类型指针和当前值
	 * @tparam T 字段值的类型
	 */
	template <typename T>
	struct FieldInfo
	{
		std::string name;              ///< 字段名称
		std::type_info const *type;    ///< 字段类型信息的指针
		T value;                       ///< 字段当前值
	};

public:
	/**
	 * @brief 将相对路径转换为规范化的绝对路径
	 * @param path 输入的路径字符串（可为相对路径）
	 * @return 规范化后的绝对路径字符串，转换失败时返回原路径
	 */
	static std::string FormortPath(const std::string &path);

	/**
	 * @brief 在字符串列表中查找与目标字符串编辑距离最小的最佳匹配
	 * @param strings 候选字符串列表（非空）
	 * @param target 目标匹配字符串
	 * @return 包含最佳匹配索引和匹配字符串的 tuple，索引为 -1 表示列表为空
	 */
	static std::tuple<int, std::string> FindBestMatch(const std::vector<std::string> &strings, const std::string &target);

	/**
	 * @brief 获取当前编译使用的数学库加速方案
	 * @return 加速方案字符串: "MKL"/"Blas"/"CUDA"/"none"/"pps"
	 */
	static std::string GetMathAcc();

	/**
	 * @brief 以系统命令方式启动指定可执行文件并传递文件路径参数
	 * @param exePath 可执行文件路径
	 * @param filePath 传递给可执行文件的参数路径
	 */
	static void SysRun(const std::string &exePath, const std::string &filePath);

	/**
	 * @brief 通过 cmd 命令行执行指定命令
	 * @param command 要执行的命令字符串
	 */
	static void RunCmd(const std::string &command);

	/**
	 * @brief 通过 PowerShell 执行指定命令（仅 Windows）
	 * @param cmd 要执行的 PowerShell 命令字符串
	 */
	static void RunPowershell(const std::string &cmd);

	/**
	 * @brief 以系统命令方式启动可执行文件
	 * @param path 可执行文件路径
	 */
	static void RunExe(const std::string &path);

	/**
	 * @brief 获取当前项目名称
	 * @return 项目名称字符串
	 */
	static std::string GetCurrentProjectName();

	/**
	 * @brief 获取当前可执行文件名称
	 * @return 可执行文件名（含扩展名）
	 */
	static std::string GetCurrentExeName();

	/**
	 * @brief 获取当前程序版本号
	 * @param path 可执行文件路径（用于版本提取），默认为空
	 * @return 版本号字符串
	 */
	static std::string GetCurrentVersion(const std::string &path = "");

	/**
	 * @brief 获取当前程序集版本号（委托 GetCurrentVersion）
	 * @return 版本号字符串
	 */
	static std::string GetCurrentAssemblyVersion();

	/**
	 * @brief 获取当前构建模式（Debug 或 Release）
	 * @return "Debug" 或 "Release"
	 */
	static std::string GetCurrentBuildMode();

	/**
	 * @brief 比较两个对象是否相等（模板方法）
	 * @tparam T 比较对象的类型
	 * @param left 左操作数
	 * @param right 右操作数
	 * @param ignoreFieldsName 比较时忽略的字段名称列表
	 * @return 相等返回 true，否则 false
	 */
	template <typename T>
	static bool AreEqual(const T &left, const T &right, const std::vector<std::string> &ignoreFieldsName = {});

	/**
	 * @brief 判断当前程序是否为 WinForm 应用程序
	 * @return 当前始终返回 false
	 */
	static bool isWinform();

	/**
	 * @brief 获取当前构建的架构模式（x64/x32）
	 * @return "_x64" 或 "_x32"
	 */
	static std::string GetBuildMode();

	/**
	 * @brief 在指定目录中递归查找具有给定扩展名的所有文件
	 * @param directoryPath 要搜索的目录路径
	 * @param fileExtension 文件扩展名（含点号）
	 * @return 匹配文件的绝对路径列表
	 */
	static std::vector<std::string> FindFilesWithExtension(const std::string &directoryPath, const std::string &fileExtension);

#ifdef _WIN32
public:
	/**
	 * @brief 复制文件或目录（Windows 下使用宽字符 API）
	 * @param sourceDirectory 源目录路径
	 * @param targetDirectory 目标目录路径
	 * @param fileType 文件类型筛选（"*" 表示全部，或指定扩展名如 ".txt"）
	 * @param overwrite 是否覆盖已存在文件，默认 true
	 */
	static void CopyFileW(const std::string &sourceDirectory, const std::string &targetDirectory,
						  const std::string &fileType, bool overwrite = true);
#endif

	/**
	 * @brief 将当前工作目录切换到指定文件所在的目录
	 * @param mainFilePath 目标文件路径（将切换到其父目录）
	 */
	static void SetCurrentDirectoryW(const std::string &mainFilePath);

	/**
	 * @brief 将任意类型数据转换为字符串表示（模板方法）
	 * @tparam T 要转换的数据类型
	 * @param message 要转换的数据
	 * @param fg 元素间分隔符，默认制表符
	 * @param coloum 是否按列输出（true=每元素换行，false=分隔符连接）
	 * @return 转换后的字符串
	 */
	template <typename T>
	static std::string Tostring(const T &message, char fg = '\t', bool coloum = true);

	/**
	 * @struct ParseLineArgs
	 * @brief 按行解析参数集，用于 ParseLine() 方法的参数封装
	 * @tparam T 解析目标数据类型
	 */
	template <typename T>
	struct ParseLineArgs
	{
		const std::vector<std::string> &lines;            ///< 待解析的所有行
		const std::string &filename;                      ///< 源文件名（用于错误报告）
		std::tuple<int, std::string> pp;                  ///< （行索引，搜索键）
		std::optional<T> moren = std::nullopt;            ///< 解析失败时的默认值
		int num = 0;                                      ///< 附加数值参数
		char fg = ' ';                                    ///< 字段分隔符
		char fg1 = '\t';                                  ///< 第二字段分隔符
		int station = 0;                                  ///< 键值分割后的取值位置（列索引）
		const std::vector<std::string> *namelist = nullptr; ///< 可选字段名列表
		bool row = false;                                 ///< 是否按行模式解析
		const std::string *titleLine = nullptr;            ///< 可选标题行指针
		bool warning = true;                              ///< 解析失败时是否输出警告
		const std::string &errorInf = "";                  ///< 错误附加信息引用
	};

	/**
	 * @brief 从文本行中按指定参数解析指定类型的值（模板方法）
	 * @tparam T 解析目标数据类型（int/double/float/string 等）
	 * @param args 解析参数包（ParseLineArgs 实例）
	 * @return 解析到的值，失败时返回默认值或零值
	 */
	template <typename T>
	static T ParseLine(ParseLineArgs<T> args);

	/**
	 * @brief 从数据数组中按索引读取单词块（以 "END"/"end" 结束），可选择去重
	 * @param data 输入文本行数组
	 * @param index 起始读取索引
	 * @param deleteSame 是否删除重复项
	 * @return 读取并处理后的单词列表
	 */
	static std::vector<std::string> ReadOutputWord(const std::vector<std::string> &data, int index, bool deleteSame);

	/**
	 * @brief 在多行文本中搜索包含指定词的行的索引
	 * @param input 输入文本行数组
	 * @param searchTerm 要搜索的关键词
	 * @param path 源文件路径（用于错误报告）
	 * @param error 未找到时是否记录错误日志，默认 true
	 * @param show 未找到时是否显示警告，默认 true
	 * @return 匹配行的索引列表，未找到时返回 [-1]
	 */
	static std::vector<int> GetMatchingLineIndexes(const std::vector<std::string> &input, const std::string &searchTerm,
												   const std::string &path, bool error = true, bool show = true);

	/**
	 * @brief 判断输入字符串中是否包含指定的搜索词
	 * @param input 要搜索的字符串
	 * @param searchTerm 搜索关键词
	 * @return 包含返回 true，否则 false
	 */
	static bool GetMatchingLineIndexes(const std::string &input, const std::string &searchTerm);

	/**
	 * @brief 获取结构体的字段信息列表（返回 FieldInfo 向量）
	 * @tparam T 结构体类型
	 * @param str 结构体实例
	 * @return 字段信息（名称、类型、值）向量
	 */
	template <typename T>
	static std::vector<FieldInfo<typename T::value_type>> GetStructFields(const T &str);

	/**
	 * @brief 获取结构体的字段信息列表（返回字段名、类型指针、值的三元组向量）
	 * @tparam T 结构体类型
	 * @param structure 结构体实例
	 * @return 三元组（名称, 类型信息指针, 值）向量
	 */
	template <typename T>
	static std::vector<std::tuple<std::string, const std::type_info *, T>> GetStructFields(const T &structure);

	/**
	 * @brief 获取结构体的字段名和类型信息列表
	 * @tparam T 结构体类型
	 * @param structure 结构体实例
	 * @return 二元组（名称, 类型信息指针）向量
	 */
	template <typename T>
	static std::vector<std::tuple<std::string, const std::type_info *>> GetStructNameAndType(const T &structure);

	/**
	 * @brief 编译期判断类型 T 是否为结构体/类（非指针、非引用）
	 * @tparam T 待判断的类型
	 * @return 是结构体返回 true，否则 false
	 */
	template <typename T>
	static bool IsStruct();

	/**
	 * @brief 编译期判断类型 T 是否为结构体或结构体数组
	 * @tparam T 待判断的类型
	 * @return 是结构体或结构体数组返回 true，否则 false
	 */
	template <typename T>
	static bool IsStructOrStructArray();

	/**
	 * @brief 判断对象是否为列表类型（当前默认返回 false）
	 * @tparam T 对象类型
	 * @param obj 对象实例
	 * @return 当前始终返回 false
	 */
	template <typename T>
	static bool IsList(const T &obj);

	/**
	 * @brief 获取结构体/类的编译器类型名称
	 * @tparam T 结构体类型
	 * @param structure 结构体实例
	 * @return typeid 解析出的类型名称字符串
	 */
	template <typename T>
	static std::string GetStructName(const T &structure);

	/**
	 * @brief 获取系统可用逻辑线程数
	 * @return 硬件支持的并发线程数
	 */
	static int GetThreadCount();

	/**
	 * @brief 尝试将字符串转换为枚举值（不抛出异常）
	 * @tparam T 枚举类型
	 * @param value 要转换的字符串
	 * @param enumValue 输出参数，转换成功时存储结果
	 * @return 转换成功返回 true，否则 false
	 */
	template <typename T>
	static bool TryConvertToEnum(const std::string &value, T &enumValue);

	/**
	 * @brief 将字符串转换为枚举值（可能抛出异常）
	 * @tparam T 枚举类型
	 * @param value 要转换的字符串
	 * @return 对应的枚举值，转换失败时抛出 std::invalid_argument
	 */
	template <typename T>
	static T ConvertToEnum(const std::string &value);

	/**
	 * @brief 将矩阵标题、行标题、列标题和矩阵数据组合为输出文件格式的字符串数组
	 * @tparam MatrixType 矩阵类型
	 * @param title 矩阵标题
	 * @param rowtitle 行标题列表
	 * @param columtitle 列标题列表
	 * @param matrix 矩阵数据
	 * @return 格式化的输出字符串数组
	 */
	template <typename MatrixType>
	static std::vector<std::string> ConvertMatrixTitleToOutfile(const std::string &title,
																const std::vector<std::string> &rowtitle,
																const std::vector<std::string> &columtitle,
																const MatrixType &matrix);

	/**
	 * @brief 提取文件路径中的扩展名（含点号，转小写）
	 * @param path 文件路径
	 * @return 小写扩展名字符串（如 ".txt"），无扩展名返回空字符串
	 */
	static std::string GetFileExtension(const std::string &path);

	/**
	 * @brief 修改文件路径的扩展名（若文件存在则重命名）
	 * @param path 输入/输出参数，原始路径，操作后变为新路径
	 * @param newExtension 新扩展名（含点号或不含均可）
	 */
	static void SetFileExtension(std::string &path, const std::string &newExtension);

	/**
	 * @brief 在字符串各字符之间插入指定分隔符
	 * @param source 源字符串
	 * @param spilt 插入的分隔符
	 * @param num 每对字符间插入的分隔符重复次数，默认 1
	 * @return 填充后的字符串
	 */
	static std::string FillString(const std::string &source, const std::string &spilt, int num = 1);

	/**
	 * @brief 生成指定长度的随机字母字符串
	 * @param len 随机字符串长度
	 * @return 随机字母字符串（仅含大小写字母）
	 */
	static std::string RandomString(int len);

	/**
	 * @brief 将字符串在指定宽度内居中（两端填充符号）
	 * @param input 要居中的字符串
	 * @param width 目标总宽度
	 * @param symbol 填充用字符，默认空格
	 * @return 居中后的字符串
	 */
	static std::string CenterText(const std::string &input, int width, char symbol = ' ');

	/**
	 * @brief 设置控制台光标位置（ANSI 转义序列）
	 * @param left 水平偏移量（正=右移，负=左移）
	 * @param top 垂直偏移量（正=下移，负=上移）
	 */
	static void SetCursorPosition(int left, int top);

	/**
	 * @brief 线程安全地将 time_t 转换为本地时间结构体（按值返回）
	 * @param time 要转换的 time_t 时间戳
	 * @return 本地时间的 std::tm 结构体副本
	 */
	static std::tm GetSafeLocalTime(const std::time_t &time);

	/**
	 * @brief 线程安全地将 time_t 转换为本地时间结构体（按指针返回）
	 * @param time 要转换的 time_t 时间戳
	 * @param ptr 占位参数（用于重载区分）
	 * @return 指向线程局部 std::tm 结构体的指针
	 */
	static std::tm *GetSafeLocalTime(const std::time_t &time, const bool ptr);

	/**
	 * @brief 获取当前年份（字符串格式）
	 * @return 四位年份字符串（如 "2025"）
	 */
	static std::string GetCurrentYear();
	/**
	 * @brief 获取当前年份（整数格式）
	 * @param temp111 占位参数（用于重载区分）
	 * @return 四位年份整数（如 2025）
	 */
	static int GetCurrentYear(bool temp111);
	/**
	 * @brief 获取当前月份（字符串格式）
	 * @return 月份字符串（"1"～"12"）
	 */
	static std::string GetCurrentMonth();
	/**
	 * @brief 获取当前月份（整数格式）
	 * @param temp111 占位参数
	 * @return 月份整数（1～12）
	 */
	static int GetCurrentMonth(bool temp111);
	/**
	 * @brief 获取当前日（字符串格式）
	 * @return 日字符串（"1"～"31"）
	 */
	static std::string GetCurrentDay();
	/**
	 * @brief 获取当前日（整数格式）
	 * @param temp111 占位参数
	 * @return 日整数（1～31）
	 */
	static int GetCurrentDay(bool temp111);
	/**
	 * @brief 获取当前小时（24 小时制，字符串格式）
	 * @return 小时字符串（"0"～"23"）
	 */
	static std::string GetCurrentHour();
	/**
	 * @brief 获取当前小时（24 小时制，整数格式）
	 * @param temp111 占位参数
	 * @return 小时整数（0～23）
	 */
	static int GetCurrentHour(bool temp111);
	/**
	 * @brief 获取当前分钟（字符串格式）
	 * @return 分钟字符串（"0"～"59"）
	 */
	static std::string GetCurrentMinute();
	/**
	 * @brief 获取当前分钟（整数格式）
	 * @param temp111 占位参数
	 * @return 分钟整数（0～59）
	 */
	static int GetCurrentMinute(bool temp111);
	/**
	 * @brief 获取当前秒（字符串格式）
	 * @return 秒字符串（"0"～"59"）
	 */
	static std::string GetCurrentSecond();
	/**
	 * @brief 获取当前秒（整数格式）
	 * @param temp111 占位参数
	 * @return 秒整数（0～59）
	 */
	static int GetCurrentSecond(bool temp111);
	/**
	 * @brief 获取当前日期时间的格式化字符串（YYYY-MM-DD HH:MM:SS）
	 * @return 格式化的时间字符串
	 */
	static std::string GetCurrentTimeW();
	/**
	 * @brief 获取程序编译时的构建日期时间（由 __DATE__/__TIME__ 宏提供）
	 * @return 构建时间字符串
	 */
	static std::string GetBuildTime();

	/**
	 * @brief 去除 std::vector 中的重复元素（保持原顺序）
	 * @tparam T 元素类型（需支持排序和相等比较）
	 * @param values 输入向量
	 * @return 去重后的向量
	 */
	template <typename T>
	static std::vector<T> Distinct(const std::vector<T> &values);

	/**
	 * @brief 提取 std::vector 中出现次数大于 1 的重复元素
	 * @tparam T 元素类型（需支持哈希）
	 * @param values 输入向量
	 * @return 重复元素向量（每种元素仅出现一次）
	 */
	template <typename T>
	static std::vector<T> Duplicates(const std::vector<T> &values);

	/**
	 * @brief 去除 std::array 中的重复元素
	 * @tparam T 元素类型
	 * @tparam N 数组大小
	 * @param values 输入数组
	 * @return 去重后的数组（不足部分填充默认值）
	 */
	template <typename T, size_t N>
	static std::array<T, N> Distinct(const std::array<T, N> &values);

	/**
	 * @brief 提取 std::array 中的重复元素
	 * @tparam T 元素类型
	 * @tparam N 数组大小
	 * @param values 输入数组
	 * @return 重复元素向量
	 */
	template <typename T, size_t N>
	static std::vector<T> Duplicates(const std::array<T, N> &values);

	/**
	 * @brief 计算两个字符串之间的编辑距离（Levenshtein 距离）
	 * @param a 第一个字符串
	 * @param b 第二个字符串
	 * @return 编辑距离（整数值，越小越相似）
	 */
	static int LevenshteinDistance(const std::string &a, const std::string &b);

	/**
	 * @brief 按指定分隔符拆分字符串
	 * @param str 要拆分的字符串
	 * @param delimiter 分隔符字符串
	 * @return 拆分后的子字符串向量
	 */
	static std::vector<std::string> SplitString(const std::string &str, const std::string &delimiter);

	/**
	 * @brief 去除字符串首尾空白字符
	 * @param str 要修剪的字符串
	 * @return 修剪后的字符串，全空白时返回空字符串
	 */
	static std::string TrimString(const std::string &str);

	/**
	 * @brief 将字符串转换为全小写形式
	 * @param str 输入字符串
	 * @return 全小写字符串
	 */
	static std::string ToLowerString(const std::string &str);

	/**
	 * @brief 检查指定路径是否存在且为普通文件
	 * @param path 要检查的文件路径
	 * @return 存在且为普通文件返回 true，否则 false
	 */
	static bool FileExists(const std::string &path);

	/**
	 * @brief 检查指定路径是否存在且为目录
	 * @param path 要检查的目录路径
	 * @return 存在且为目录返回 true，否则 false
	 */
	static bool DirectoryExists(const std::string &path);

	/**
	 * @brief 递归创建目录（类似 mkdir -p）
	 * @param path 要创建的目录路径
	 */
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
		std::cerr << L_OTHER_ErrorFormatPath << e.what() << '\n';
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
		std::cerr << L_OTHER_FailedExecute << command << '\n';
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
	std::cout << L_OTHER_NoPowerShell << '\n';
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
		std::cerr << L_OTHER_ErrorFindFiles << e.what() << '\n';
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
		std::cerr << L_OTHER_ErrorCopyFiles << e.what() << '\n';
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
			std::cerr << L_OTHER_DirNotExist << dirPath << '\n';
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << L_OTHER_ErrorSetDir << e.what() << '\n';
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
			std::cerr << L_OTHER_FileNotExist << path << '\n';
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << L_OTHER_ErrorChangeExt << e.what() << '\n';
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
		std::cerr << L_OTHER_ErrorCreateDirs << e.what() << '\n';
	}
}

// ──────────────────────────────────────────────────────────────────────────────
// Template implementations
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief 比较两个对象是否相等（模板实现）
 * @tparam T 比较对象类型
 * @param left 左操作数
 * @param right 右操作数
 * @param ignoreFieldsName 忽略字段名列表（当前未使用）
 * @return 算术类型直接相等比较，其他类型调用 operator==
 */
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

/**
 * @brief 从文本行中按指定参数解析指定类型的值（模板实现）
 * @tparam T 解析目标数据类型
 * @param args ParseLineArgs 参数包
 * @return 解析到的值，失败时返回默认值或零值
 * @details 在指定行中查找键（key），提取键后的值部分，按分隔符拆分后取指定位置，转换为目标类型。
 */
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
			std::cerr << L_OTHER_WarnParseLine << args.filename
			  << " for key: " << std::get<1>(args.pp) << '\n';
			}
			return T{};
		}
	}
	catch (const std::exception &e)
	{
		if (args.warning)
		{
			std::cerr << L_OTHER_ErrorParseLine << args.filename << ": " << e.what() << '\n';
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

/**
 * @brief 获取结构体字段信息（返回三元组向量，模板实现）
 * @tparam T 结构体类型
 * @param structure 结构体实例
 * @return 空向量（默认未实现具体反射逻辑）
 */
template <typename T>
std::vector<std::tuple<std::string, const std::type_info *, T>> OtherHelper::GetStructFields(const T &structure)
{
	std::vector<std::tuple<std::string, const std::type_info *, T>> result;
	return result;
}

/**
 * @brief 获取结构体字段名和类型信息（模板实现）
 * @tparam T 结构体类型
 * @param structure 结构体实例
 * @return 空向量（默认未实现具体反射逻辑）
 */
template <typename T>
std::vector<std::tuple<std::string, const std::type_info *>> OtherHelper::GetStructNameAndType(const T &structure)
{
	std::vector<std::tuple<std::string, const std::type_info *>> result;
	return result;
}

/**
 * @brief 编译期判断类型 T 是否为结构体/类（模板实现）
 * @tparam T 待判断类型
 * @return is_class_v && !is_pointer_v && !is_reference_v
 */
template <typename T>
bool OtherHelper::IsStruct()
{
	return std::is_class_v<T> && !std::is_pointer_v<T> && !std::is_reference_v<T>;
}

/**
 * @brief 编译期判断类型 T 是否为结构体或结构体数组（模板实现）
 * @tparam T 待判断类型
 * @return 是结构体或结构体数组返回 true
 */
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

/**
 * @brief 判断对象是否为列表类型（模板实现，当前默认返回 false）
 * @tparam T 对象类型
 * @param obj 对象实例
 * @return 当前始终返回 false
 */
template <typename T>
bool OtherHelper::IsList(const T &obj)
{
	return false;
}

/**
 * @brief 获取结构体的编译器类型名称（模板实现）
 * @tparam T 结构体类型
 * @param structure 结构体实例
 * @return typeid 解析的类型名称
 */
template <typename T>
std::string OtherHelper::GetStructName(const T &structure)
{
	return typeid(T).name();
}

/**
 * @brief 尝试将字符串转换为枚举值（模板实现）
 * @tparam T 枚举类型
 * @param value 输入字符串
 * @param enumValue 输出枚举值
 * @return 转换成功返回 true，异常捕获后返回 false
 */
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

/**
 * @brief 将字符串转换为枚举值（模板实现，默认抛出异常）
 * @tparam T 枚举类型
 * @param value 输入字符串
 * @return 位实现具体转换，直接抛出异常
 * @throws std::invalid_argument 始终抛出
 */
template <typename T>
T OtherHelper::ConvertToEnum(const std::string &value)
{
	throw std::invalid_argument("Enum conversion not implemented for this type");
}

/**
 * @brief 将矩阵数据与标题信息组合为输出文件格式字符串数组（模板实现）
 * @tparam MatrixType 矩阵类型
 * @param title 矩阵主标题
 * @param rowtitle 行标题列表
 * @param columtitle 列标题列表
 * @param matrix 矩阵数据
 * @return 格式化的输出行数组（首行为标题行，后续为数据行）
 */
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

/**
 * @brief 去除 std::vector 中的重复元素（模板实现）
 * @tparam T 元素类型
 * @param values 输入向量
 * @return 排序去重后的向量
 */
template <typename T>
std::vector<T> OtherHelper::Distinct(const std::vector<T> &values)
{
	std::vector<T> result = values;
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

/**
 * @brief 提取 std::vector 中的重复元素（模板实现）
 * @tparam T 元素类型（需支持哈希）
 * @param values 输入向量
 * @return 重复元素向量
 */
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

/**
 * @brief 去除 std::array 中的重复元素（模板实现）
 * @tparam T 元素类型
 * @tparam N 数组大小
 * @param values 输入数组
 * @return 去重后的数组
 */
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

/**
 * @brief 提取 std::array 中的重复元素（模板实现）
 * @tparam T 元素类型
 * @tparam N 数组大小
 * @param values 输入数组
 * @return 重复元素向量
 */
template <typename T, size_t N>
std::vector<T> OtherHelper::Duplicates(const std::array<T, N> &values)
{
	std::vector<T> temp(values.begin(), values.end());
	return Duplicates(temp);
}

// ──────────────────────────────────────────────────────────────────────────────
// Explicit template specializations with inline keyword (ODR-safe in header-only)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief Tostring 模板特化：int 转字符串
 * @param message 整数值
 * @param fg 分隔符（未使用）
 * @param coloum 列模式标志（未使用）
 * @return 整数的字符串表示
 */
template <>
inline std::string OtherHelper::Tostring<int>(const int &message, char fg, bool coloum)
{
	return std::to_string(message);
}

/**
 * @brief Tostring 模板特化：bool 转字符串
 * @param message 布尔值
 * @return "1" 或 "0"
 */
template <>
inline std::string OtherHelper::Tostring<bool>(const bool &message, char fg, bool coloum)
{
	return std::to_string(message);
}

/**
 * @brief Tostring 模板特化：double 转字符串
 * @param message 双精度浮点值
 * @return 浮点数的字符串表示
 */
template <>
inline std::string OtherHelper::Tostring<double>(const double &message, char fg, bool coloum)
{
	return std::to_string(message);
}

/**
 * @brief Tostring 模板特化：float 转字符串
 * @param message 单精度浮点值
 * @return 浮点数的字符串表示
 */
template <>
inline std::string OtherHelper::Tostring<float>(const float &message, char fg, bool coloum)
{
	return std::to_string(message);
}

/**
 * @brief Tostring 模板特化：std::string 直接返回
 * @param message 字符串
 * @return 原字符串（不变）
 */
template <>
inline std::string OtherHelper::Tostring<std::string>(const std::string &message, char fg, bool coloum)
{
	return message;
}

/**
 * @brief Tostring 模板特化：Eigen::VectorXd 转字符串
 * @param message 双精度 Eigen 向量
 * @param fg 元素间分隔符
 * @param coloum 是否按列模式（每元素换行）
 * @return 格式化的向量字符串
 */
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

/**
 * @brief Tostring 模板特化：Eigen::VectorXf 转字符串
 * @param message 单精度 Eigen 向量
 * @param fg 元素间分隔符
 * @param coloum 是否按列模式
 * @return 格式化的向量字符串
 */
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

/**
 * @brief Tostring 模板特化：Eigen::MatrixXd 转字符串
 * @param message 双精度 Eigen 矩阵
 * @param fg 元素间列分隔符
 * @param coloum 列模式标志（未使用）
 * @return 多行格式化的矩阵字符串
 */
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

/**
 * @brief Tostring 模板特化：Eigen::MatrixXf 转字符串
 * @param message 单精度 Eigen 矩阵
 * @param fg 元素间列分隔符
 * @param coloum 列模式标志（未使用）
 * @return 多行格式化的矩阵字符串
 */
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

/**
 * @brief Tostring 模板特化：std::vector<double> 转字符串
 * @param message 双精度向量
 * @param fg 元素间分隔符
 * @param coloum 是否按列模式（true=每元素换行，false=分隔符连接）
 * @return 格式化的向量字符串
 */
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

/**
 * @brief Tostring 模板特化：std::vector<float> 转字符串
 * @param message 单精度浮点向量
 * @param fg 元素间分隔符
 * @param coloum 是否按列模式
 * @return 格式化的向量字符串
 */
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

/**
 * @brief Tostring 模板特化：std::vector<int> 转字符串
 * @param message 整数向量
 * @param fg 元素间分隔符
 * @param coloum 是否按列模式
 * @return 格式化的向量字符串
 */
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

/**
 * @brief Tostring 模板特化：std::vector<bool> 转字符串
 * @param message 布尔向量
 * @param fg 元素间分隔符
 * @param coloum 是否按列模式
 * @return 格式化的向量字符串（0/1 表示）
 */
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