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
// LogHelper 中英文本地化字符串定义
// 使用 TL() 宏实现零开销编译期语言切换
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once
#include "LocaleText.hpp"

// Banner strings
#define L_LOG_BannerDebugLine     TL("Debug 调试模式 develop by 赵子祯 @2021, 2025", "Debug mode developed by Zhao Zizhen @2021, 2025")
#define L_LOG_BannerCopyright     TL("Copyright (c) HawtC2.Team.ZZZ,赵子祯 licensed under GPL v3", "Copyright (c) HawtC2.Team.ZZZ, Zhao Zizhen licensed under GPL v3")
#define L_LOG_BannerInstitution   TL("Copyright (c) Key Laboratory of Jiangsu province high tech design @Tg Team", "Copyright (c) Key Laboratory of Jiangsu province high tech design @Tg Team")
#define L_LOG_BannerTagline       TL(" > 风力发电机仿真设计优化平台工具链 @赵子祯", " > Wind turbine simulation design optimization platform toolchain @Zhao Zizhen")
#define L_LOG_BannerRunning       TL("****** 运行 ", "****** Running ")
#define L_LOG_BannerEnd           TL(" ******", " ******")
#define L_LOG_BannerContact       TL(" - Tel:13935201274  E:1821216780@qq.com http://www.hawtc.cn", " - Tel:13935201274  E:1821216780@qq.com http://www.hawtc.cn")
#define L_LOG_InitFirst           TL("请先调用 DisplayInformation 函数!", "Please call DisplayInformation function first!")
#define L_LOG_UnknownError        TL("未知错误：", "Unknown error: ")
#define L_LOG_SimFinish           TL("仿真运行完成! ", "Simulation Run Finished! ")
#define L_LOG_CostRealTime        TL("实际耗时", "Cost real time")
#define L_LOG_ErrorTitle          TL("错误", "Error")
#define L_LOG_NormalEnd           TL("程序正常结束", "Program terminated normally")
#define L_LOG_ForceEnd            TL("程序强制终止", "Program forcefully terminated")
#define L_LOG_ExtCallNoExit       TL("外部调用终止，程序不退出", "External call termination, program not exiting")
#define L_LOG_WritingOutput       TL("正在写入输出文件", "Writing output file")
#define L_LOG_OutFile             TL("输出文件", "Out File")
#define L_LOG_LogSaved            TL("日志已保存至", "Log saved to")
#define L_LOG_Aborting            TL("正在中止 ", "Aborting ")
#define L_LOG_DueToError          TL(" 由于错误: ", " due to error: ")
#define L_LOG_RunCompleted        TL(" 运行正常完成!", " Run completed normally!")
