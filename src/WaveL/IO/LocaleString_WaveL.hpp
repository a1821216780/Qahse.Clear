#pragma once

#include "../../IO/LocaleText.hpp"

/**
 * @file LocaleString_WaveL.hpp
 * @brief WaveL 模块的本地化字符串定义，支持中英文双语错误/警告消息。
 *        Localized string definitions for the WaveL module, supporting bilingual (Chinese/English) error/warning messages.
 *
 * 使用 TL() 宏实现编译期字符串选择，与 LocaleText 框架集成。
 * 中文在前，英文在后，系统根据全局语言设置自动选择对应文本。
 *
 * Uses the TL() macro for compile-time string selection, integrated with the LocaleText framework.
 * Chinese text comes first, English second; the system auto-selects based on global language setting.
 */

#define L_WAVEL_ModeUnsupported      TL("WaveL 当前不支持该模式", "WaveL does not support this mode yet")
#define L_WAVEL_HsNonNegative        TL("WaveL 要求 Hs 不能为负", "WaveL requires Hs to be non-negative")
#define L_WAVEL_TpPositive           TL("WaveL 要求 Tp 必须为正", "WaveL requires Tp to be positive")
#define L_WAVEL_TimeStepPositive     TL("WaveL 要求 timeStep 必须为正", "WaveL requires timeStep to be positive")
#define L_WAVEL_DurationPositive     TL("WaveL 要求 simDuration 必须为正", "WaveL requires simDuration to be positive")
#define L_WAVEL_DepthPositive        TL("WaveL 要求 waterDepth 必须为正", "WaveL requires waterDepth to be positive")
#define L_WAVEL_FreqBinsPositive     TL("WaveL 要求 numFreqBins 必须为正", "WaveL requires numFreqBins to be positive")
#define L_WAVEL_DirBinsPositive      TL("WaveL 要求 numDirBins 必须为正", "WaveL requires numDirBins to be positive")
#define L_WAVEL_FreqRangeInvalid     TL("WaveL 频率范围无效", "WaveL frequency range is invalid")
#define L_WAVEL_UserSpectrumNeeded   TL("USER_SPECTRUM 需要 importedSpectrumPath", "USER_SPECTRUM requires importedSpectrumPath")
#define L_WAVEL_UserSeriesNeeded     TL("USER_TIMESERIES 需要 importedTimeSeriesPath", "USER_TIMESERIES requires importedTimeSeriesPath")
#define L_WAVEL_ImportComponentsNeed TL("IMPORT 模式需要 importedComponentsPath", "IMPORT mode requires importedComponentsPath")
#define L_WAVEL_CannotOpenOutput     TL("无法打开 WaveL 输出文件", "Cannot open WaveL output file")
#define L_WAVEL_InvalidSpectrumFile  TL("无效的用户谱文件", "Invalid user spectrum file")
#define L_WAVEL_InvalidSeriesFile    TL("无效的用户时程序列文件", "Invalid user timeseries file")
#define L_WAVEL_InvalidComponentFile TL("无效的波浪成分文件", "Invalid wave component file")
#define L_WAVEL_EmptyWaveField       TL("WaveL 组件为空", "WaveL component set is empty")
#define L_WAVEL_BatchUnsupported     TL("WaveL 首版不支持 BATCH 工作流", "WaveL first release does not support BATCH workflow")
#define L_WAVEL_GridInvalid          TL("WaveL 运动学网格参数无效", "WaveL kinematics grid parameters are invalid")
#define L_WAVEL_CacheMismatch        TL("WaveL 预计算缓存与直接叠加结果不一致", "WaveL precomputed cache does not match direct summation")
