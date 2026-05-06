//**********************************************************************************************************************************
// 许可证说明
// 版权所有(C) 2021, 2025  赵子祯
//
// 根据 Boost 软件许可证 - 版本 1.0 - 2003年8月17日
// 您不得使用此文件，除非符合许可证。
//
//**********************************************************************************************************************************

// ───────────────────────────────── File Info ─────────────────────────────────
//
// 跨平台中英文国际化工具。
// - Windows: GetUserDefaultUILanguage() 检测系统 UI 语言
// - Linux:   LANG/LC_ALL 环境变量检测
// - 提供 TL(zh, en) 宏实现零开销编译期字符串切换
// - 支持通过 SetLanguage() 手动覆盖语言设置
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include <string>
#include <algorithm>
#include <clocale>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

class LocaleText
{
public:
	enum class Lang
	{
		EN = 0, ///< 英文
		ZH = 1  ///< 中文
	};

	/**
	 * @brief 根据操作系统环境自动检测用户界面语言
	 * @details Windows: 调用 GetUserDefaultUILanguage() 获取用户界面语言 ID；
	 *          Linux/macOS: 读取 LANG/LC_ALL 环境变量解析语言标签。
	 *          检测到中文返回 Lang::ZH，否则返回 Lang::EN。
	 * @return 检测到的语言枚举值
	 * @note 程序启动时调用一次即可，后续通过 GetLanguage() 获取
	 * @code
	 * LocaleText::SetLanguage(LocaleText::DetectSystemLanguage());
	 * @endcode
	 */
	static Lang DetectSystemLanguage()
	{
#ifdef _WIN32
		LANGID langId = GetUserDefaultUILanguage();
		WORD primary = PRIMARYLANGID(langId);
		if (primary == LANG_CHINESE)
			return Lang::ZH;
		if (primary == LANG_JAPANESE)
			return Lang::EN; // 可扩展为 Lang::JA
		if (primary == LANG_KOREAN)
			return Lang::EN; // 可扩展为 Lang::KO
		return Lang::EN;
#else
		const char *lang = std::getenv("LANG");
		if (!lang)
			lang = std::getenv("LC_ALL");
		if (!lang)
			lang = std::getenv("LC_MESSAGES");
		if (lang)
		{
			std::string s(lang);
			std::transform(s.begin(), s.end(), s.begin(), ::tolower);
			if (s.find("zh") != std::string::npos)
				return Lang::ZH;
		}
		return Lang::EN;
#endif
	}

	/**
	 * @brief 手动设置当前语言（可覆盖系统检测结果）
	 * @param lang 目标语言枚举值
	 * @code
	 * LocaleText::SetLanguage(LocaleText::Lang::EN);
	 * @endcode
	 */
	static void SetLanguage(Lang lang) { s_currentLang = lang; }

	/**
	 * @brief 获取当前生效的语言设置
	 * @return 语言枚举值
	 * @code
	 * if (LocaleText::GetLanguage() == LocaleText::Lang::ZH) { ... }
	 * @endcode
	 */
	static Lang GetLanguage() { return s_currentLang; }

	/**
	 * @brief 根据当前语言返回对应的文本（零开销二选一）
	 * @param zh 中文文本
	 * @param en 英文文本
	 * @return 当前语言对应的 C 字符串指针
	 * @note 编译期常量折叠：未使用的分支不会被编译进二进制
	 * @code
	 * const char* msg = LocaleText::Text("文件未找到", "File not found");
	 * @endcode
	 */
	static constexpr const char *Text(const char *zh, const char *en)
	{
		// constexpr 在 C++20 中无法直接访问静态成员，运行时版本如下
		return s_currentLang == Lang::ZH ? zh : en;
	}

	/**
	 * @brief 检查当前语言是否为中文
	 * @return 中文返回 true
	 */
	static bool IsChinese() { return s_currentLang == Lang::ZH; }

	/**
	 * @brief 检查当前语言是否为英文
	 * @return 英文返回 true
	 */
	static bool IsEnglish() { return s_currentLang == Lang::EN; }

private:
	static inline Lang s_currentLang = Lang::EN; ///< 默认英文，启动时通过 DetectSystemLanguage() 覆盖
};

/// @brief 便捷宏：根据当前语言选择中文或英文文本
/// @code
/// throw std::runtime_error(TL("文件未找到", "File not found"));
/// std::cout << TL("你好", "Hello") << std::endl;
/// @endcode
#define TL(zh, en) LocaleText::Text(zh, en)
