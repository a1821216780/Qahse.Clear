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
// 该文件提供一个接近 C# 控制台体验的跨平台 C++20 工具，支持颜色、标题、Eigen 输出等功能。
// 通过 Write/WriteLine 多类型重载实现丰富的输出能力，并提供持续着色和按次着色两种模式。
// 跨平台实现兼顾 Windows 10+ 的 ANSI VT100 和老版本 Windows 的 Win32 API，以及 Linux/macOS 的 ANSI 转义序列。
// 还提供了 ConsoleColorScope RAII 守卫，方便局部着色输出。完整使用案例详见文件顶部的文档注释。
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

/**
 * @file Console.h
 * @brief 提供一个接近 C# 控制台体验的跨平台 C++20 工具（支持颜色、标题、Eigen 输出）。
 *
 * @details
 * 该文件为纯头文件实现，直接包含即可使用。
 *
 * 功能概览：
 * - 基础类型输出：int/float/double/bool/string/const char* 等。
 * - Eigen 输出：
 *   - 向量输出为 [x, y, z]
 *   - 矩阵输出为 [[a, b],\n [c, d]]
 * - 控制台颜色：前景色、背景色、恢复默认色。
 * - 控制台标题：Windows 与 Linux/macOS 统一接口。
 * - C# 风格接口：Title / ForegroundColor / BackgroundColor，以及 Write/WriteLine 着色重载。
 *
 * 跨平台策略：
 * - Windows 10+：优先启用 ANSI VT100。
 * - 老版本 Windows：自动回退到 Win32 SetConsoleTextAttribute。
 * - Linux/macOS：使用 ANSI 转义序列。
 *
 * @par 完整使用案例
 * @code{.cpp}
 * #include "IO/System/Console.h"
 * #include <Eigen/Dense>
 *
 * int main() {
 *     using namespace System;
 *
 *     // 1) 标题
 *     Console::Title("Qahse 控制台演示");
 *
 *     // 2) 基础输出
 *     Console::Write("步骤 = ");
 *     Console::WriteLine(42);
 *     Console::WriteLine(true);          // 输出 True
 *     Console::WriteLine(3.1415926);
 *
 *     // 3) 仅本次调用着色（最接近 C# 使用习惯）
 *     Console::WriteLine("信息", ConsoleColor::Cyan);
 *     Console::WriteLine("警告", ConsoleColor::Yellow);
 *     Console::WriteLine("错误", ConsoleColor::White, ConsoleColor::DarkRed);
 *
 *     // 4) 持续着色 + 手动恢复
 *     Console::SetForegroundColor(ConsoleColor::Green);
 *     Console::WriteLine("运行中...");
 *     Console::ResetColor();
 *
 *     // 5) Eigen 向量/矩阵输出
 *     Eigen::Vector3d v(1.0, 2.0, 3.0);
 *     Eigen::Matrix2d m;
 *     m << 1.0, 2.0,
 *          3.0, 4.0;
 *     Console::Write("v = ");
 *     Console::WriteLine(v, ConsoleColor::Magenta);  // [1, 2, 3]
 *     Console::Write("m = ");
 *     Console::WriteLine(m);
 *
 *     // 6) RAII 自动恢复颜色
 *     {
 *         ConsoleColorScope scope(ConsoleColor::Yellow);
 *         Console::WriteLine("作用域颜色块");
 *     } // 离开作用域自动 ResetColor
 *
 *     return 0;
 * }
 * @endcode
 */

#include <iostream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <type_traits>

// ── 平台相关头文件 ─────────────────────────────────────────────────────────────
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <conio.h> // Windows 单键读取
#endif

// ── Eigen 支持 ─────────────────────────────────────────────────────────────────
// 策略：
//   1. 若已包含 Eigen 头文件，可通过其宏判断。
//   2. 否则尝试使用 __has_include 自动包含 Eigen/Dense。
//   3. 若环境无 Eigen，则关闭 Eigen 输出能力（HC3_CONSOLE_EIGEN=0）。
#if defined(EIGEN_CORE_H) || defined(EIGEN_WORLD_VERSION) || \
    defined(EIGEN_MATRIXBASE_H) || defined(EIGEN_DENSE_H)
#define HC3_CONSOLE_EIGEN 1
#elif defined(__has_include) && __has_include(<Eigen/Dense>)
#include <Eigen/Dense>
#define HC3_CONSOLE_EIGEN 1
#else
#define HC3_CONSOLE_EIGEN 0
#endif

// ══════════════════════════════════════════════════════════════════════════════
//  ConsoleColor  — 对齐 C# 的 ConsoleColor，增加 Default 表示恢复默认色
// ══════════════════════════════════════════════════════════════════════════════
/**
 * @brief 控制台颜色枚举。
 * @note 取值布局与 C# ConsoleColor 保持一致，便于迁移已有调用习惯。
 */
enum class ConsoleColor : int
{
    Default = -1, ///< 恢复终端默认颜色（配合 ResetColor 使用）
    Black = 0,
    DarkBlue = 1,
    DarkGreen = 2,
    DarkCyan = 3,
    DarkRed = 4,
    DarkMagenta = 5,
    DarkYellow = 6,
    Gray = 7,
    DarkGray = 8,
    Blue = 9,
    Green = 10,
    Cyan = 11,
    Red = 12,
    Magenta = 13,
    Yellow = 14,
    White = 15
};

// ══════════════════════════════════════════════════════════════════════════════
//  Console
// ══════════════════════════════════════════════════════════════════════════════
/**
 * @brief C# 风格控制台工具类。
 * @details
 * - 提供 Write/WriteLine 多类型重载。
 * - 提供颜色控制与按次着色输出。
 * - 提供标题、清屏、输入、stderr 输出等能力。
 */
class ZConsole
{
public:
    // ── 标题 ────────────────────────────────────────────────────────────────

    /**
     * @brief 设置控制台标题。
     * @param title 标题文本。
     * @return 无返回值。
     * @note Windows 使用 Win32 SetConsoleTitleA，Linux/macOS 使用 XTerm OSC-0 序列。
     * @code ZConsole::SetTitle("My App"); @endcode
     */
    static void SetTitle(const std::string &title)
    {
        s_title = title;
#ifdef _WIN32
        ::SetConsoleTitleA(title.c_str());
#else
        // 使用 XTerm OSC-0 序列设置标题（多数 Linux/macOS 终端支持）
        std::cout << "\033]0;" << title << "\007";
        std::cout.flush();
#endif
    }

    /**
     * @brief C# 风格别名：设置标题。
     * @param title 标题文本。
     * @return 无返回值。
     * @note 功能等同于 SetTitle。
     * @code ZConsole::Title("My App"); @endcode
     */
    static void Title(const std::string &title) { SetTitle(title); }

    /**
     * @brief C# 风格别名：获取最近一次设置的标题文本。
     * @return 当前标题字符串引用。
     * @note 若从未设置标题，返回空字符串。
     * @code auto t = ZConsole::Title(); @endcode
     */
    static const std::string &Title() { return s_title; }

    // ── 颜色 ─────────────────────────────────────────────────────────────────

    /**
     * @brief 设置前景色（持续生效，直到再次修改或 ResetColor）。
     * @param c 前景色枚举值。
     * @return 无返回值。
     * @note 自动触发 ensureInit() 初始化终端；Windows 老版本回退 Win32 API。
     * @code ZConsole::SetForegroundColor(ConsoleColor::Green); @endcode
     */
    static void SetForegroundColor(ConsoleColor c)
    {
        ensureInit();
        s_fg = c;
        applyColor();
    }

    /**
     * @brief 获取当前记录的前景色状态。
     * @return 当前前景色枚举值。
     * @note 返回的是内部记录值，不保证已被应用到终端。
     * @code auto fg = ZConsole::GetForegroundColor(); @endcode
     */
    static ConsoleColor GetForegroundColor() { return s_fg; }

    /**
     * @brief C# 风格别名：设置前景色。
     * @param c 前景色枚举值。
     * @return 无返回值。
     * @note 功能等同于 SetForegroundColor。
     * @code ZConsole::ForegroundColor(ConsoleColor::Red); @endcode
     */
    static void ForegroundColor(ConsoleColor c) { SetForegroundColor(c); }

    /**
     * @brief C# 风格别名：获取前景色。
     * @return 当前前景色枚举值。
     * @note 功能等同于 GetForegroundColor。
     * @code auto fg = ZConsole::ForegroundColor(); @endcode
     */
    static ConsoleColor ForegroundColor() { return GetForegroundColor(); }

    /**
     * @brief 设置背景色（持续生效，直到再次修改或 ResetColor）。
     * @param c 背景色枚举值。
     * @return 无返回值。
     * @note 自动触发 ensureInit() 初始化终端；与前景色独立设置。
     * @code ZConsole::SetBackgroundColor(ConsoleColor::DarkBlue); @endcode
     */
    static void SetBackgroundColor(ConsoleColor c)
    {
        ensureInit();
        s_bg = c;
        applyColor();
    }

    /**
     * @brief 获取当前记录的背景色状态。
     * @return 当前背景色枚举值。
     * @note 返回的是内部记录值，不保证已被应用到终端。
     * @code auto bg = ZConsole::GetBackgroundColor(); @endcode
     */
    static ConsoleColor GetBackgroundColor() { return s_bg; }

    /**
     * @brief C# 风格别名：设置背景色。
     * @param c 背景色枚举值。
     * @return 无返回值。
     * @note 功能等同于 SetBackgroundColor。
     * @code ZConsole::BackgroundColor(ConsoleColor::DarkGreen); @endcode
     */
    static void BackgroundColor(ConsoleColor c) { SetBackgroundColor(c); }

    /**
     * @brief C# 风格别名：获取背景色。
     * @return 当前背景色枚举值。
     * @note 功能等同于 GetBackgroundColor。
     * @code auto bg = ZConsole::BackgroundColor(); @endcode
     */
    static ConsoleColor BackgroundColor() { return GetBackgroundColor(); }

    /**
     * @brief 恢复前景/背景到终端默认颜色。
     * @return 无返回值。
     * @note Windows 下恢复至首次捕获的默认属性；Linux/macOS 使用 ANSI \033[0m。
     * @code ZConsole::ResetColor(); @endcode
     */
    static void ResetColor()
    {
        ensureInit();
        s_fg = ConsoleColor::Default;
        s_bg = ConsoleColor::Default;
#ifdef _WIN32
        if (s_useWin32)
        {
            ::SetConsoleTextAttribute(::GetStdHandle(STD_OUTPUT_HANDLE), s_defaultAttrib);
            return;
        }
#endif
        std::cout << "\033[0m";
        std::cout.flush();
    }

    // ── Write（不自动换行） ─────────────────────────────────────────────────

    /**
     * @brief 输出 bool 值，格式与 C# 一致（True/False），不换行。
     * @param v 布尔值。
     * @return 无返回值。
     * @note 输出 "True" 或 "False"，而非 "1" 或 "0"。
     * @code ZConsole::Write(true); @endcode
     */
    template <typename T, std::enable_if_t<std::is_same_v<T, bool>, int> = 0>
    static void Write(T v)
    {
        ensureInit();
        std::cout << (v ? "True" : "False");
        std::cout.flush();
    }

    /**
     * @brief 输出单个字符，不自动换行。
     * @param v 字符。
     * @return 无返回值。
     * @note 输出后自动刷新 stdout。
     * @code ZConsole::Write('A'); @endcode
     */
    static void Write(char v)
    {
        ensureInit();
        std::cout.put(v);
        std::cout.flush();
    }

    /**
     * @brief 输出 C 字符串，不自动换行。
     * @param v C 字符串指针。
     * @return 无返回值。
     * @note 当传入 nullptr 时不输出任何内容，也不会崩溃。
     * @code ZConsole::Write("Hello"); @endcode
     */
    static void Write(const char *v)
    {
        ensureInit();
        if (v)
            std::cout << v;
        std::cout.flush();
    }

    /**
     * @brief 输出字符串，不自动换行。
     * @param v 字符串引用。
     * @return 无返回值。
     * @note 空字符串不输出任何内容，但会刷新 stdout。
     * @code ZConsole::Write(std::string("Hello")); @endcode
     */
    static void Write(const std::string &v)
    {
        ensureInit();
        std::cout << v;
        std::cout.flush();
    }

    /**
     * @brief 本次输出使用指定前景色，输出后自动恢复先前颜色状态。
     * @param v 要输出的内容（任意可流输出类型）。
     * @param fg 临时前景色。
     * @return 无返回值。
     * @note 颜色恢复通过 saveColorState/restoreColorState 确保先前状态不丢失。
     * @code ZConsole::Write("Warning", ConsoleColor::Yellow); @endcode
     */
    template <typename T>
    static void Write(const T &v, ConsoleColor fg)
    {
        const ColorState prev = saveColorState();
        SetForegroundColor(fg);
        Write(v);
        restoreColorState(prev);
    }

    /**
     * @brief 本次输出使用指定前景色和背景色，输出后自动恢复先前颜色状态。
     * @param v 要输出的内容。
     * @param fg 临时前景色。
     * @param bg 临时背景色。
     * @return 无返回值。
     * @note 同时设置前景和背景色，输出完毕后恢复。
     * @code ZConsole::Write("Error", ConsoleColor::White, ConsoleColor::DarkRed); @endcode
     */
    template <typename T>
    static void Write(const T &v, ConsoleColor fg, ConsoleColor bg)
    {
        const ColorState prev = saveColorState();
        SetForegroundColor(fg);
        SetBackgroundColor(bg);
        Write(v);
        restoreColorState(prev);
    }

#if HC3_CONSOLE_EIGEN
    /**
     * @brief 输出 Eigen 表达式（向量/矩阵），不自动换行。
     * @param expr Eigen 稠密矩阵或向量表达式。
     * @return 无返回值。
     * @details
     * - 向量输出： [x, y, z]
     * - 矩阵输出： [[a, b],\n [c, d]]
     * @note 仅在 HC3_CONSOLE_EIGEN 宏为 1 时可用；零维矩阵输出 "[]"。
     * @code ZConsole::Write(eigenVector); @endcode
     */
    template <typename Derived>
    static void Write(const Eigen::DenseBase<Derived> &expr)
    {
        ensureInit();
        eigenFmt(std::cout, expr.eval());
        std::cout.flush();
    }
#endif

    /**
     * @brief 通用输出重载，支持任意可被 ostream << 的类型（如 int、float、double 等）。
     * @param v 要输出的值。
     * @return 无返回值。
     * @note 通过 SFINAE 避免与 bool/char/string/C 字符串等专用重载冲突。
     * @code ZConsole::Write(42); ZConsole::Write(3.14); @endcode
     */
    template <typename T,
              std::enable_if_t<
                  !std::is_same_v<std::decay_t<T>, bool> &&
                      !std::is_same_v<std::decay_t<T>, char> &&
                      !std::is_same_v<std::decay_t<T>, char *> &&
                      !std::is_same_v<std::decay_t<T>, const char *> &&
                      !std::is_same_v<std::decay_t<T>, std::string>,
                  int> = 0>
    static void Write(const T &v)
    {
        ensureInit();
        std::cout << v;
        std::cout.flush();
    }

    // ── WriteLine（追加换行） ────────────────────────────────────────────────

    /**
     * @brief 输出换行，无额外内容。
     * @return 无返回值。
     * @note 相当于输出 '\n' 并刷新。
     * @code ZConsole::WriteLine(); @endcode
     */
    static void WriteLine()
    {
        ensureInit();
        std::cout.put('\n');
        std::cout.flush();
    }

    /**
     * @brief 输出一行空行，并临时使用指定前景色。
     * @param fg 临时前景色。
     * @return 无返回值。
     * @note 仅输出换行，不输出任何文本内容。
     * @code ZConsole::WriteLine(ConsoleColor::Cyan); @endcode
     */
    static void WriteLine(ConsoleColor fg)
    {
        const ColorState prev = saveColorState();
        SetForegroundColor(fg);
        WriteLine();
        restoreColorState(prev);
    }

    /**
     * @brief 输出一行空行，并临时使用指定前景色和背景色。
     * @param fg 临时前景色。
     * @param bg 临时背景色。
     * @return 无返回值。
     * @note 仅输出换行，不输出任何文本内容。
     * @code ZConsole::WriteLine(ConsoleColor::White, ConsoleColor::DarkRed); @endcode
     */
    static void WriteLine(ConsoleColor fg, ConsoleColor bg)
    {
        const ColorState prev = saveColorState();
        SetForegroundColor(fg);
        SetBackgroundColor(bg);
        WriteLine();
        restoreColorState(prev);
    }

    /**
     * @brief 输出任意内容并追加换行。
     * @param v 要输出的内容。
     * @return 无返回值。
     * @note 内部调用 Write(v) 后追加 '\n'。
     * @code ZConsole::WriteLine("Hello World"); @endcode
     */
    template <typename T>
    static void WriteLine(const T &v)
    {
        Write(v);
        std::cout.put('\n');
        std::cout.flush();
    }

    /**
     * @brief 输出任意内容并换行，且仅本次调用使用指定前景色。
     * @param v 要输出的内容。
     * @param fg 临时前景色。
     * @return 无返回值。
     * @note 输出后自动恢复先前颜色状态。
     * @code ZConsole::WriteLine("Info", ConsoleColor::Cyan); @endcode
     */
    template <typename T>
    static void WriteLine(const T &v, ConsoleColor fg)
    {
        const ColorState prev = saveColorState();
        SetForegroundColor(fg);
        WriteLine(v);
        restoreColorState(prev);
    }

    /**
     * @brief 输出任意内容并换行，且仅本次调用使用指定前景色和背景色。
     * @param v 要输出的内容。
     * @param fg 临时前景色。
     * @param bg 临时背景色。
     * @return 无返回值。
     * @note 输出后自动恢复先前颜色状态。
     * @code ZConsole::WriteLine("Error", ConsoleColor::White, ConsoleColor::DarkRed); @endcode
     */
    template <typename T>
    static void WriteLine(const T &v, ConsoleColor fg, ConsoleColor bg)
    {
        const ColorState prev = saveColorState();
        SetForegroundColor(fg);
        SetBackgroundColor(bg);
        WriteLine(v);
        restoreColorState(prev);
    }

    // ── 错误输出（stderr） ────────────────────────────────────────────────────

    /**
     * @brief 输出到标准错误流（stderr），不换行。
     * @param v 要输出的内容。
     * @return 无返回值。
     * @note 输出到 stderr 而非 stdout，适合错误信息。
     * @code ZConsole::WriteError("Fatal error occurred"); @endcode
     */
    template <typename T>
    static void WriteError(const T &v)
    {
        std::cerr << v;
        std::cerr.flush();
    }

    /**
     * @brief 向标准错误流输出换行。
     * @return 无返回值。
     * @note 相当于向 stderr 输出 '\n' 并刷新。
     * @code ZConsole::WriteErrorLine(); @endcode
     */
    static void WriteErrorLine()
    {
        std::cerr.put('\n');
        std::cerr.flush();
    }

    /**
     * @brief 输出到标准错误流并换行。
     * @param v 要输出的内容。
     * @return 无返回值。
     * @note 输出到 stderr 后自动追加换行。
     * @code ZConsole::WriteErrorLine("An error occurred"); @endcode
     */
    template <typename T>
    static void WriteErrorLine(const T &v)
    {
        std::cerr << v << '\n';
        std::cerr.flush();
    }

    // ── 清屏 ─────────────────────────────────────────────────────────────────

    /**
     * @brief 清空控制台并将光标移动到左上角。
     * @return 无返回值。
     * @note Windows 使用 FillConsoleOutput 填充空白；Linux/macOS 使用 ANSI \033[2J\033[H。
     * @code ZConsole::Clear(); @endcode
     */
    static void Clear()
    {
        ensureInit();
#ifdef _WIN32
        HANDLE h = ::GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        if (::GetConsoleScreenBufferInfo(h, &csbi))
        {
            const DWORD total = static_cast<DWORD>(csbi.dwSize.X * csbi.dwSize.Y);
            const COORD origin = {0, 0};
            DWORD written = 0;
            ::FillConsoleOutputCharacterA(h, ' ', total, origin, &written);
            ::FillConsoleOutputAttribute(h, csbi.wAttributes, total, origin, &written);
            ::SetConsoleCursorPosition(h, origin);
        }
#else
        std::cout << "\033[2J\033[H";
        std::cout.flush();
#endif
    }

    // ── 输入 ─────────────────────────────────────────────────────────────────

    /**
     * @brief 读取一整行输入（含中文支持）。
     * @return 读取到的字符串（不含换行符）。
     * @note 使用 std::getline 从 std::cin 读取。
     * @code auto line = ZConsole::ReadLine(); @endcode
     */
    static std::string ReadLine()
    {
        // 输入字符串应该支持中文
        std::string line;
        std::getline(std::cin, line);
        // // 将GBK编码的line转为utf-8,然后返回
        // #ifdef _WIN32
        //             int wlen = MultiByteToWideChar(CP_ACP, 0, line.c_str(), -1, nullptr, 0);
        //             if (wlen <= 0)
        //                 return "";
        //             std::wstring wline(wlen - 1, L'\0');
        //             MultiByteToWideChar(CP_ACP, 0, line.c_str(), -1, &wline[0], wlen);

        //             int u8len = WideCharToMultiByte(CP_UTF8, 0, wline.c_str(), -1, nullptr, 0, nullptr, nullptr);
        //             if (u8len <= 0)
        //                 return "";
        //             std::string u8line(u8len - 1, '\0');
        //             WideCharToMultiByte(CP_UTF8, 0, wline.c_str(), -1, &u8line[0], u8len, nullptr, nullptr);
        //             return u8line;
        // #else
        //             return line;
        // #endif

        return line;
    }

    /**
     * @brief 读取一个按键（不回显）。
     * @return 按键的整数值（ASCII 或虚拟键码）。
     * @details
     * - Windows 使用 _getch()（通常不回显）。
     * - Linux/macOS 使用 getchar() 简化实现。
     * @note Windows 下可捕获方向键等功能键（返回两字节序列）。
     * @code int key = ZConsole::ReadKey(); @endcode
     */
    static int ReadKey()
    {
#ifdef _WIN32
        return ::_getch();
#else
        return std::getchar();
#endif
    }

    // ── 显式刷新 ─────────────────────────────────────────────────────────────

    /**
     * @brief 强制刷新 stdout 缓冲区。
     * @return 无返回值。
     * @note 正常情况下 ZConsole 每次输出后自动刷新，此方法用于手动确保输出。
     * @code ZConsole::Flush(); @endcode
     */
    static void Flush()
    {
        std::cout.flush();
    }

private:
    struct ColorState
    {
        ConsoleColor fg;
        ConsoleColor bg;
    };

    /**
     * @brief 保存当前前景色和背景色状态。
     * @return ColorState 结构体，包含当前 fg 和 bg。
     * @note 用于配合 restoreColorState 实现临时着色后恢复。
     * @code auto prev = saveColorState(); @endcode
     */
    static ColorState saveColorState()
    {
        return ColorState{s_fg, s_bg};
    }

    /**
     * @brief 恢复颜色到指定状态并应用到终端。
     * @param state 之前保存的 ColorState。
     * @return 无返回值。
     * @note 恢复后刷新终端颜色，不关心是否与当前状态相同。
     * @code restoreColorState(prev); @endcode
     */
    static void restoreColorState(const ColorState &state)
    {
        s_fg = state.fg;
        s_bg = state.bg;
        applyColor();
    }

    // ── 静态状态 ─────────────────────────────────────────────────────────────
    static inline ConsoleColor s_fg = ConsoleColor::Default;
    static inline ConsoleColor s_bg = ConsoleColor::Default;
    static inline std::string s_title{};

#ifdef _WIN32
    static inline bool s_useWin32 = false;
    static inline WORD s_defaultAttrib =
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // 默认：灰色前景 + 黑色背景
#endif

    // ── 一次性初始化（C++11 局部静态变量线程安全） ────────────────────────────
    /**
     * @brief 一次性初始化终端环境（线程安全）。
     * @return 无返回值。
     * @details 首次调用时检测 ANSI 虚拟终端支持（Windows 10+），
     *          不可用时自动回退 Win32 SetConsoleTextAttribute。
     * @note 使用 C++11 局部静态变量保证只执行一次且线程安全。
     * @code ensureInit(); @endcode
     */
    static void ensureInit()
    {
        static const bool kOnce = []
        {
#ifdef _WIN32
            HANDLE h = ::GetStdHandle(STD_OUTPUT_HANDLE);
            // 保存当前控制台属性，便于后续恢复
            CONSOLE_SCREEN_BUFFER_INFO csbi{};
            if (::GetConsoleScreenBufferInfo(h, &csbi))
                s_defaultAttrib = csbi.wAttributes;
            // 尝试开启 ANSI 虚拟终端模式（Windows 10+）
            DWORD mode = 0;
            if (::GetConsoleMode(h, &mode) &&
                !::SetConsoleMode(h, mode | ENABLE_PROCESSED_OUTPUT |
                                         ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                s_useWin32 = true; // ANSI 不可用时回退 Win32 方式
#endif
            return true;
        }();
        (void)kOnce;
    }

    // ── ANSI 转义码构造 ─────────────────────────────────────────────────────
    //
    // ConsoleColor 索引 -> ANSI SGR 参数：
    //   0-7   标准色（前景 30-37 / 背景 40-47）
    //   8-15  高亮色（前景 90-97 / 背景 100-107）
    /**
     * @brief 将 ConsoleColor 转换为 ANSI SGR 转义序列。
     * @param c 颜色枚举值。
     * @param bg true 表示生成背景色转义码，false 为前景色。
     * @return ANSI 转义序列字符串（如 "\033[32m"）；Default 返回 "\033[39m" 或 "\033[49m"。
     * @note 0-7 为标准色（30-37/40-47），8-15 为高亮色（90-97/100-107）。
     * @code auto esc = ansiEsc(ConsoleColor::Red, false); @endcode
     */
    static std::string ansiEsc(ConsoleColor c, bool bg)
    {
        if (c == ConsoleColor::Default)
            return bg ? "\033[49m" : "\033[39m";
        static constexpr int FG[16] = {30, 34, 32, 36, 31, 35, 33, 37,
                                       90, 94, 92, 96, 91, 95, 93, 97};
        static constexpr int BG[16] = {40, 44, 42, 46, 41, 45, 43, 47,
                                       100, 104, 102, 106, 101, 105, 103, 107};
        const int i = static_cast<int>(c);
        if (i < 0 || i > 15)
            return {};
        char buf[12];
        std::snprintf(buf, sizeof(buf), "\033[%dm", bg ? BG[i] : FG[i]);
        return buf;
    }

    // ── 将当前 s_fg / s_bg 应用到终端 ─────────────────────────────────────────
    /**
     * @brief 将当前保存的前景色/背景色应用到终端输出。
     * @return 无返回值。
     * @details Windows 老版本通过 SetConsoleTextAttribute 设置；
     *          ANSI 模式下拼接前景/背景转义序列后输出。
     * @note 直接在 stdout 输出转义序列，不检查终端是否支持。
     * @code applyColor(); @endcode
     */
    static void applyColor()
    {
#ifdef _WIN32
        if (s_useWin32)
        {
            HANDLE h = ::GetStdHandle(STD_OUTPUT_HANDLE);
            WORD attr = s_defaultAttrib;
            attr &= ~(WORD)0x00FF; // 清除前景（0-3 位）和背景（4-7 位）
            if (s_fg != ConsoleColor::Default)
                attr |= static_cast<WORD>(static_cast<int>(s_fg) & 0x0F);
            if (s_bg != ConsoleColor::Default)
                attr |= static_cast<WORD>((static_cast<int>(s_bg) & 0x0F) << 4);
            ::SetConsoleTextAttribute(h, attr);
            return;
        }
#endif
        std::string esc;
        esc.reserve(14);
        if (s_fg != ConsoleColor::Default)
            esc += ansiEsc(s_fg, false);
        if (s_bg != ConsoleColor::Default)
            esc += ansiEsc(s_bg, true);
        if (!esc.empty())
        {
            std::cout << esc;
            std::cout.flush();
        }
    }

#if HC3_CONSOLE_EIGEN
    // ── Eigen 格式化输出 ─────────────────────────────────────────────────────
    //
    //   向量（rows==1 或 cols==1） -> [x, y, z]
    //   矩阵（rows>1 且 cols>1）  -> [[a, b, c],
    //                                 [d, e, f]]
    /**
     * @brief Eigen 表达式格式化输出内部实现。
     * @param os 输出流引用。
     * @param m 已求值的稠密矩阵/向量。
     * @return 无返回值。
     * @details
     * - 向量（rows==1 或 cols==1） -> [x, y, z]
     * - 矩阵（rows>1 且 cols>1）  -> [[a, b, c],\n [d, e, f]]
     * @note 零维矩阵输出 "[]"；直接写入 os 流。
     * @code eigenFmt(std::cout, matrix.eval()); @endcode
     */
    template <typename EvalType>
    static void eigenFmt(std::ostream &os, const EvalType &m)
    {
        const int rows = static_cast<int>(m.rows());
        const int cols = static_cast<int>(m.cols());

        if (rows == 0 || cols == 0)
        {
            os << "[]";
            return;
        }

        if (rows == 1 || cols == 1)
        {
            // ── 向量 ────────────────────────────────────────────────────
            const int n = (rows == 1) ? cols : rows;
            os << "[";
            for (int k = 0; k < n; ++k)
            {
                if (k)
                    os << ", ";
                os << ((rows == 1) ? m(0, k) : m(k, 0));
            }
            os << "]";
        }
        else
        {
            // ── 矩阵 ────────────────────────────────────────────────────
            for (int r = 0; r < rows; ++r)
            {
                os << (r == 0 ? "[[" : " [");
                for (int c = 0; c < cols; ++c)
                {
                    if (c)
                        os << ", ";
                    os << m(r, c);
                }
                if (r < rows - 1)
                    os << "],\n";
                else
                    os << "]]";
            }
        }
    }
#endif // HC3_CONSOLE_EIGEN
};

// ══════════════════════════════════════════════════════════════════════════════
//  ConsoleColorScope  — RAII 守卫，离开作用域自动恢复颜色
// ══════════════════════════════════════════════════════════════════════════════
/**
 * @class ConsoleColorScope
 * @brief RAII 作用域颜色守卫，离开作用域自动恢复默认颜色。
 * @details 构造时设置颜色，析构时自动调用 ZConsole::ResetColor() 恢复默认颜色。
 *          不可拷贝，不可赋值，仅支持构造时指定颜色。
 *
 * @code { ConsoleColorScope scope(ConsoleColor::Yellow); ZConsole::WriteLine("Warning"); } // 离开作用域后自动恢复颜色 @endcode
 */
class ConsoleColorScope
{
public:
    /**
     * @brief 构造时设置前景色和可选的背景色。
     * @param fg 前景色（必选，若为 Default 则不设置前景）。
     * @param bg 背景色（可选，默认 ConsoleColor::Default 表示不修改）。
     * @note 构造后立即通过 ZConsole::SetForegroundColor/SetBackgroundColor 应用颜色。
     * @code ConsoleColorScope scope(ConsoleColor::Red, ConsoleColor::DarkBlue); @endcode
     */
    explicit ConsoleColorScope(ConsoleColor fg,
                               ConsoleColor bg = ConsoleColor::Default)
    {
        if (fg != ConsoleColor::Default)
            ZConsole::SetForegroundColor(fg);
        if (bg != ConsoleColor::Default)
            ZConsole::SetBackgroundColor(bg);
    }

    /**
     * @brief 析构时自动恢复默认颜色。
     * @note 调用 ZConsole::ResetColor() 恢复终端默认前景/背景色。
     * @code // 作用域结束时自动调用 @endcode
     */
    ~ConsoleColorScope()
    {
        ZConsole::ResetColor();
    }
    ConsoleColorScope(const ConsoleColorScope &) = delete;
    ConsoleColorScope &operator=(const ConsoleColorScope &) = delete;
};
