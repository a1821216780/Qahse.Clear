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
// 集中管理项目中所有可翻译的字符串常量。
// 每个字符串通过 L10N 宏定义中英文两个版本，
// 使用时直接引用常量名，无需每次手写 TL() 宏。
//
// 命名规范：L_<模块>_<描述>
// 示例：   L_LOG_FileNotFound     → "文件未找到" / "File not found"
//          L_WIND_GridPositive    → "...要求为正数" / "...requires positive"
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include "LocaleText.hpp"

// ============================================================================
// 通用 IO / 文件操作
// ============================================================================

#define L_IO_FileNotFound          TL("无法找到文件", "File not found")
#define L_IO_CannotOpenFile        TL("无法打开文件", "Cannot open file")
#define L_IO_CannotWriteFile       TL("无法写入文件", "Cannot write file")
#define L_IO_CannotDeleteFile      TL("无法删除文件", "Cannot delete file")
#define L_IO_TargetFileExists      TL("目标文件已存在", "Target file already exists")
#define L_IO_SourceFileNotFound    TL("无法找到源文件", "Source file not found")
#define L_IO_PermissionDenied      TL("权限不够", "Permission denied")
#define L_IO_DirectoryNotExist     TL("文件夹路径不存在", "Directory does not exist")
#define L_IO_DirectoryCannotCreate TL("无法创建目录", "Cannot create directory")
#define L_IO_PathIsEmpty           TL("路径为空", "Path is empty")
#define L_IO_ConvertIntFailed      TL("无法转换为整数", "Cannot convert to integer")
#define L_IO_ConvertFloatFailed    TL("无法转换为浮点数", "Cannot convert to floating point")
#define L_IO_ConvertEnumFailed     TL("无法转换为枚举", "Cannot convert to enum")
#define L_IO_ConvertBoolFailed     TL("无效布尔令牌", "Invalid boolean token")

// ============================================================================
// LogHelper
// ============================================================================

#define L_LOG_InitFirst            TL("请先调用 DisplayInformation 函数!", "Please call DisplayInformation function first!")
#define L_LOG_ProgramEnd           TL("程序运行结束", "Program execution ended")
#define L_LOG_ForceTerminate       TL("程序被强制终止", "Program forcefully terminated")
#define L_LOG_UnknownError         TL("未知错误", "Unknown error")
#define L_LOG_ExtCallNoExit        TL("外部调用终止，程序不退出", "External call termination, program not exiting")
#define L_LOG_LogFileSaved         TL("日志已写入文件", "Log written to file")
#define L_LOG_SimFinish            TL("仿真运行完成!", "Simulation Run Finished!")
#define L_LOG_CostRealTime         TL("实际耗时", "Cost real time")
#define L_LOG_ErrorTimeParam       TL("错误的时间参数!", "Invalid time parameter!")

// ============================================================================
// WindL / SimWind — 验证与异常
// ============================================================================

#define L_WIND_OnlyGenerateMode    TL("SimWind 仅支持 Mode=GENERATE", "SimWind only supports Mode=GENERATE")
#define L_WIND_NumPointYPositive   TL("SimWind 要求 NumPointY 必须为正数", "SimWind requires NumPointY to be positive")
#define L_WIND_NumPointZPositive   TL("SimWind 要求 NumPointZ 必须为正数", "SimWind requires NumPointZ to be positive")
#define L_WIND_LenWidthPositive    TL("SimWind 要求 LenWidthY 必须为正数", "SimWind requires LenWidthY to be positive")
#define L_WIND_LenHeightPositive   TL("SimWind 要求 LenHeightZ 必须为正数", "SimWind requires LenHeightZ to be positive")
#define L_WIND_DurOrAnaPositive    TL("SimWind 要求 WindDuration 或 AnalysisTime 为正数", "SimWind requires WindDuration or AnalysisTime to be positive")
#define L_WIND_TimeStepPositive    TL("SimWind 要求 TimeStep 必须为正数", "SimWind requires TimeStep to be positive")
#define L_WIND_SpeedPositive       TL("SimWind 要求 MeanWindSpeed 必须为正数", "SimWind requires MeanWindSpeed to be positive")
#define L_WIND_HubHtPositive       TL("SimWind 要求 HubHt 必须为正数", "SimWind requires HubHt to be positive")
#define L_WIND_OneCompTrue         TL("CalWu/CalWv/CalWw 至少需要一个为 true", "At least one of CalWu, CalWv, or CalWw must be true")
#define L_WIND_APIOnlyForU         TL("API 相干模型仅对 u 分量 (CohMod1) 有效", "API coherence model is valid only for the u component (CohMod1)")
#define L_WIND_SpectraNeedFile     TL("USER_SPECTRA 需要 TurbFilePath/UserTurbFile", "USER_SPECTRA requires TurbFilePath/UserTurbFile")
#define L_WIND_WindSpeedNeedFile   TL("USER_WIND_SPEED 需要 TurbFilePath/UserTurbFile", "USER_WIND_SPEED requires TurbFilePath/UserTurbFile")
#define L_WIND_USRVKMNeedShear     TL("USRVKM 需要 UserShearFile", "USRVKM requires UserShearFile")
#define L_WIND_Spectra3Rows        TL("USER_SPECTRA 要求至少 3 行数据 (Frequency/uPSD/vPSD/wPSD)", "USER_SPECTRA requires at least 3 rows with Frequency/uPSD/vPSD/wPSD")
#define L_WIND_SpectraScalePos     TL("USER_SPECTRA 要求 SpecScale1/2/3 必须为正数", "USER_SPECTRA requires SpecScale1/2/3 to be positive")
#define L_WIND_SpectraPosValues    TL("USER_SPECTRA 要求 u/v/w PSD 值必须严格为正", "USER_SPECTRA requires strictly positive u/v/w PSD values")
#define L_WIND_SpectraUniqueFreq   TL("USER_SPECTRA 要求频率严格递增且唯一", "USER_SPECTRA requires unique frequencies in strictly ascending order")
#define L_WIND_SpectraSorted       TL("USER_SPECTRA 至少需要 3 个排序后的频率-PSD 行", "USER_SPECTRA requires at least 3 sorted frequency-PSD rows")
#define L_WIND_MixingDepthNeed     TL("不稳定合成湍流需要正值的混合层深度 (ZI)", "Unstable synthetic turbulence generation requires a positive mixing-layer depth (ZI)")
#define L_WIND_CholeskyNotPD       TL("互谱密度矩阵非正定", "Cross-spectral density matrix is not positive definite")
#define L_WIND_FFTWMallocFail      TL("FFTW 无法分配 SimWind 缓冲区", "FFTW failed to allocate SimWind buffers")
#define L_WIND_FFTWPlanFail        TL("FFTW 无法创建 SimWind 计划", "FFTW failed to create SimWind plan")
#define L_WIND_UserShearNoProfile  TL("USRVKM 要求 UserShearFile 的列 Height/U/WindDir/Sigma/L 行数匹配", "USRVKM requires UserShearFile columns Height/U/WindDir/Sigma/L with matching row counts")
#define L_WIND_CannotOpenOutput    TL("无法打开输出文件", "Cannot open output file")
#define L_WIND_BinaryWriteFail     TL("SimWind 二进制输出写入失败", "Failed while writing SimWind binary output")
#define L_WIND_UserWindSpeedEmpty  TL("USER_WIND_SPEED 要求非空的用户风速文件", "USER_WIND_SPEED requires a non-empty user wind speed file")
#define L_WIND_TimeNotSorted       TL("USER_WIND_SPEED 要求时间样本按升序排列", "USER_WIND_SPEED requires time samples sorted in ascending order")
#define L_WIND_SpatialWeightFail   TL("USER_WIND_SPEED 无法构建空间插值权重", "USER_WIND_SPEED could not build spatial interpolation weights")

// ============================================================================
// WindL — Warnings
// ============================================================================

#define L_WARN_GridBottomClamped   TL("网格底部在地面处或低于地面；低处网格高度在剖面计算中已被钳制", "Grid bottom is at or below ground; low grid heights are clamped in profile calculations")
#define L_WARN_UserWindMeteoSkip   TL("USER_WIND_SPEED 导入的时间序列不包含稳定性、相干性或雷诺应力修正", "USER_WIND_SPEED imports the supplied time series without inferred stability, coherence, or Reynolds-stress corrections")
#define L_WARN_ReynoldsStressSkipNonSynthetic TL("雷诺应力目标仅对合成随机湍流模型生效；当前风模型跳过此步骤", "Reynolds-stress targets are only applied to synthetic stochastic turbulence models; the current wind model skips this step")
#define L_WARN_UserShearColMismatch TL("用户剪切数据列与高度列大小不匹配", "UserShear data column size does not match the height column")
#define L_WARN_ImprovedVkFallback  TL("B_IVKAL 需要有效的 Latitude 和 Roughness 来推导改进 von Karman 剖面；缺少大气输入时回退到 Bladed von Karman 默认值", "B_IVKAL requires valid Latitude and Roughness to derive the improved von Karman profile; missing atmospheric inputs fall back to Bladed von Karman defaults")
#define L_WARN_KroneckerActive     TL("分量使用了 WindL Kronecker 相干加速；设置 AllowCohApprox=false 可强制使用精确严格相干路径", "Component uses the legacy WindL Kronecker coherence acceleration; set AllowCohApprox=false to force the exact strict-coherence path")
#define L_WARN_ReynoldsSoften      TL("部分点处雷诺应力目标协方差被软化以保持正定性，同时保留最终分量标准差", "Reynolds-stress target covariance was softened at some points to remain positive-definite while preserving the final component sigmas")
#define L_WARN_ReynoldsSkipSingular TL("部分点处的雷诺应力缩放因局部协方差矩阵奇异或数值退化而跳过", "Reynolds-stress scaling skipped at some points because the local covariance matrix was singular or numerically degenerate")
#define L_WARN_MannRepeat          TL("Mann 盒在时间上周期重复；增大 MannNx 以避免", "Mann box repeats periodically in time; increase MannNx to avoid")
#define L_WARN_MannScaleIEC0       TL("B_MANN 由于 ScaleIEC=0 使用 MannAlphaEps 作为绝对能量；设置 ScaleIEC=1 或 2 可精确匹配 IEC 目标标准差", "B_MANN uses MannAlphaEps for absolute energy because ScaleIEC=0; set ScaleIEC=1 or 2 to match IEC target sigma exactly")
#define L_WARN_LargeStrictCoh      TL("大规模严格相干估算可能消耗大量资源", "Large strict-coherence estimate may consume significant resources")

// ============================================================================
// ZFile — 文件操作异常消息
// ============================================================================

#define L_ZFILE_FileNotFound       TL("无法找到文件", "File not found")
#define L_ZFILE_CannotDelete        TL("无法删除文件", "Cannot delete file")
#define L_ZFILE_SourceNotFound      TL("无法找到源文件", "Source file not found")
#define L_ZFILE_DestExists          TL("目标文件已存在", "Target file already exists")
#define L_ZFILE_CannotRead          TL("无法打开文件", "Cannot open file")
#define L_ZFILE_CannotWrite         TL("无法写入文件", "Cannot write file")

// ============================================================================
// ZPath — 路径操作异常消息
// ============================================================================

#define L_ZPATH_DirNotExist         TL("文件夹路径不存在", "Directory does not exist")
#define L_ZPATH_PermissionDenied    TL("权限不够", "Permission denied")
#define L_ZPATH_NoPermission        TL("没有操作权限,请联系系统权限管理员", "No permission, contact system administrator")
#define L_ZPATH_CantFindFunc        TL("Cant find function name", "Cant find function name")
#define L_ZPATH_ExtMismatch         TL("当前文件扩展名与指定名称不同", "File extension does not match specified name")
#define L_ZPATH_EmptyParent         TL("文件的父文件夹为空，无路径不可用", "Parent directory is empty, path unavailable")
#define L_ZPATH_NameConflict        TL("与已打开的程序冲突", "Conflicts with already opened program")

// ============================================================================
// OutFile — 输出文件异常消息
// ============================================================================

#define L_OUTFILE_CannotOpen        TL("无法打开文件", "Cannot open file")
#define L_OUTFILE_PathOccupied      TL("当前路径被其他程序占用，或没有该文件夹无法继续", "Path is occupied or directory does not exist")
#define L_OUTFILE_NotInitialized    TL("读写器没有初始化", "Reader/writer not initialized")
#define L_OUTFILE_NoDecimalPlaces   TL("OutFile 不允许不指定小数位数", "OutFile requires decimal places to be specified")
#define L_OUTFILE_MustSpecifyFormat TL("必须指定 decimalPlaces 或 Scientific 参数之一", "Must specify decimalPlaces or Scientific")
#define L_OUTFILE_FilenameConflict  TL("当前文件名称与已打开文件冲突", "Current filename conflicts with already opened file")

// ============================================================================
// BinaryFileHandler
// ============================================================================

#define L_BINFILE_OpenReadFail     TL("无法以读取模式打开文件: ", "Failed to open file for reading: ")
#define L_BINFILE_OpenWriteFail    TL("无法以写入模式打开文件: ", "Failed to open file for writing: ")
#define L_BINFILE_OpenAppendFail   TL("无法以追加模式打开文件: ", "Failed to open file for appending: ")
#define L_BINFILE_ModeNotFound     TL("IO.BinaryFile 找不到模式=", "IO.BinaryFile Cant find mode=")
#define L_BINFILE_UnsupportedType  TL("Qahse.IO 不支持的数据类型或流不可用", "Qahse.IO Unsupported data type or stream not available")
#define L_BINFILE_ReadFail         TL("从文件读取数据失败，可能已到文件末尾!", "Failed to read data from file, it may end of file!")

// ============================================================================
// MSExcel
// ============================================================================

#define L_MSEXCEL_CannotFindFile   TL("MSExcel: 无法找到文件 ", "MSExcel: Cannot find file ")
#define L_MSEXCEL_NoFileAppend     TL("当前文件不存在！无法执行增量数据操作！", "Current file does not exist! Cannot perform incremental data operation!")
#define L_MSEXCEL_CannotFindSheet  TL("MSExcel: 无法找到工作表 ", "MSExcel: Cannot find worksheet ")
#define L_MSEXCEL_NoRowCount       TL("未指定行数，读取直到空单元格", "No rowcount specified, reading until empty cell")
#define L_MSEXCEL_MustSpecifyRC    TL("MSExcel: 必须同时指定rowcount和columncount来读取矩阵", "MSExcel: Must specify both rowcount and columncount for matrix reading")

// ============================================================================
// Yaml
// ============================================================================

#define L_YAML_CannotOpen          TL("无法打开yaml文件: ", "Cannot open yaml file: ")
#define L_YAML_CannotSaveEmpty     TL("无法保存yaml，因为savepath和path均为空。", "Cannot save yaml because both savepath and path are empty.")
#define L_YAML_CannotWrite         TL("无法写入yaml文件: ", "Cannot write yaml file: ")

// ============================================================================
// TimesHelper
// ============================================================================

#define L_TIME_SimElapsed          TL("仿真时间已过: ", "Simulation time elapsed: ")
#define L_TIME_IllegalParam        TL("非法的时间参数!", "Illegal time parameter!")

// ============================================================================
// CLI — main.cpp 命令行接口字符串
// ============================================================================

#define L_CLI_Title               TL("OpenWECD.Qahse CLI", "OpenWECD.Qahse CLI")
#define L_CLI_Banner              TL("Qahse 命令行接口 (CLI) - 版本 1.0", "Qahse Command Line Interface (CLI) - Version 1.0")
#define L_CLI_Usage               TL("用法: Qahse [选项]", "Usage: Qahse [options]")
#define L_CLI_Options             TL("选项:", "Options:")
#define L_CLI_OptionTest          TL("  --test                 运行 QFEM 测试", "  --test                 Run QFEM tests")
#define L_CLI_OptionQWD           TL("  --qwd <文件>           使用 WindL .qwd 文件生成风场", "  --qwd <file>           Generate wind field using WindL .qwd file")
#define L_CLI_OptionHelp          TL("  --help                 显示帮助信息", "  --help                 Show help information")
#define L_CLI_ErrorQwdPath        TL("--qwd 需要指定 .qwd 文件路径", "--qwd requires a .qwd file path")
#define L_CLI_RunningSimWind      TL(" 正在运行 WindL SimWind，输入文件 \"", " Running WindL SimWind with input file \"")
#define L_CLI_GeneratedFiles      TL("SimWind 已生成风场文件:", "SimWind generated wind files:")
#define L_CLI_OutputBTS           TL("  BTS: ", "  BTS: ")
#define L_CLI_OutputBladedWND     TL("  Bladed WND: ", "  Bladed WND: ")
#define L_CLI_OutputTurbSimWND    TL("  TurbSim WND: ", "  TurbSim WND: ")
#define L_CLI_OutputSUM           TL("  SUM: ", "  SUM: ")
#define L_CLI_RunningBatch        TL(" 正在运行 WindL 批量模式...", " Running WindL batch mode...")
#define L_CLI_BatchManifest       TL(" 批量清单: ", " Batch manifest: ")
#define L_CLI_BatchStatus         TL(" 批量状态: ", " Batch status: ")
#define L_CLI_BatchSummary        TL(" 批量摘要: ", " Batch summary: ")
#define L_CLI_ImportNotImpl       TL("WindL 导入模式尚未实现", "WindL import mode is not implemented yet")
#define L_CLI_SimWindFailed       TL("SimWind 失败: ", "SimWind failed: ")
#define L_CLI_BatchRunnerFailed   TL("WindL 批量运行失败: ", "WindL batch runner failed: ")

// ============================================================================
// OtherHelper — 通用错误/警告消息
// ============================================================================

#define L_OTHER_ErrorFormatPath    TL("格式化路径出错：", "Error formatting path: ")
#define L_OTHER_FailedExecute      TL("执行失败：", "Failed to execute: ")
#define L_OTHER_NoPowerShell       TL("此平台不支持 PowerShell", "PowerShell not available on this platform")
#define L_OTHER_ErrorFindFiles     TL("查找文件出错：", "Error finding files: ")
#define L_OTHER_ErrorCopyFiles     TL("复制文件出错：", "Error copying files: ")
#define L_OTHER_DirNotExist        TL("文件夹路径不存在：", "Directory does not exist: ")
#define L_OTHER_ErrorSetDir        TL("设置当前目录出错：", "Error setting current directory: ")
#define L_OTHER_FileNotExist       TL("文件不存在：", "File does not exist: ")
#define L_OTHER_ErrorChangeExt     TL("修改文件扩展名出错：", "Error changing file extension: ")
#define L_OTHER_ErrorCreateDirs    TL("创建目录出错：", "Error creating directories: ")
#define L_OTHER_WarnParseLine      TL("警告：无法解析行，文件 ", "Warning: Failed to parse line in ")
#define L_OTHER_ErrorParseLine     TL("解析行出错，文件 ", "Error parsing line in ")
