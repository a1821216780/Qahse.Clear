#pragma once

#include "LocaleString.hpp"

// ============================================================================
// SimWind — Additional exception messages (not in LocaleString.hpp)
// ============================================================================

#define L_WIND_FFTWMalloc3D       TL("FFTW 无法分配 SimWind 3D Mann 缓冲区", "FFTW failed to allocate SimWind 3D Mann buffers")
#define L_WIND_FFTWPlan3D         TL("FFTW 无法创建 SimWind 3D Mann IFFT 计划", "FFTW failed to create SimWind 3D Mann IFFT plan")
#define L_WIND_NumPointZPositive  TL("SimWind 要求 NumPointZ 必须为正数", "SimWind requires NumPointZ to be positive")
#define L_WIND_CannotOpenBTS      TL("无法打开 BTS 输出文件", "Cannot open BTS output")
#define L_WIND_CannotOpenBlndWND  TL("无法打开 Bladed WND 输出文件", "Cannot open Bladed WND output")
#define L_WIND_CannotOpenTSWND    TL("无法打开 TurbSim WND 输出文件", "Cannot open TurbSim-compatible WND output")
#define L_WIND_CannotOpenSUM      TL("无法打开 SUM 输出文件", "Cannot open SUM output")
#define L_WIND_USRVKMNoData       TL("USRVKM 要求可读的 UserShearFile 且包含剖面数据行", "USRVKM requires a readable UserShearFile with profile rows")

// ============================================================================
// SimWind — Additional warnings (not in LocaleString.hpp)
// ============================================================================

#define L_WARN_UserShearDirMismatch    TL("用户风切变风向列大小与高度列不匹配；忽略风向剖面", "UserShear wind-direction column size does not match the height column; direction profile is ignored")
#define L_WARN_UserShearSigmaMismatch  TL("用户风切变标准差列大小与高度列不匹配；忽略标准差剖面", "UserShear standard-deviation column size does not match the height column; sigma profile is ignored")
#define L_WARN_UserShearLengthMismatch TL("用户风切变长度尺度列大小与高度列不匹配；忽略长度尺度剖面", "UserShear length-scale column size does not match the height column; length-scale profile is ignored")
#define L_WARN_LargeStrictCohEstimate  TL("大规模严格相干估算：峰值内存 %s，Cholesky FLOPs %s", "Large strict-coherence estimate: peak memory %s, Cholesky FLOPs %s")

// ============================================================================
// SimWind — Progress messages
// ============================================================================

#define L_PROG_ReadingInput        TL("正在读取和标准化 SimWind 输入文件", " Reading and normalizing the SimWind input file")
#define L_PROG_GridTimePrepared    TL("网格/时间已准备", " Grid/time prepared")
#define L_PROG_WarningPrefix       TL("警告", " Warning")
#define L_PROG_LargeStrictCoh      TL("请求大规模严格相干运行；将报告实测进度耗时及 ETA", " Large strict-coherence run requested; continuing and reporting elapsed time/ETA from measured progress")
#define L_PROG_InitRuntimeEstimate TL("初始运行时间估算", " Initial runtime estimate")
#define L_PROG_ComputingStats      TL("正在计算生成风场统计量", " Computing generated-field statistics")
#define L_PROG_WritingOutput       TL("正在写入请求的风场输出文件", " Writing requested wind output files")
#define L_PROG_GenBTS              TL("正在生成 AeroDyn/TurbSim 二进制全场文件", " Generating AeroDyn/TurbSim binary full-field file")
#define L_PROG_GenBladed           TL("正在生成 Bladed 二进制全场文件", " Generating Bladed binary full-field file")
#define L_PROG_GenTurbSim          TL("正在生成 TurbSim 兼容 Bladed 风格文件", " Generating TurbSim-compatible Bladed-style file")
#define L_PROG_WritingSum          TL("正在将统计信息写入摘要文件", " Writing statistics to summary file")
#define L_PROG_ProcessingComplete  TL("处理完成", " Processing complete")

#define L_PROG_CompSkipped         TL("分量 %d/3 已跳过", "  component %d/3 skipped")
#define L_PROG_CompMatrices        TL("分量矩阵 (", "-component matrices (")
#define L_PROG_KroneckerActive     TL("Kronecker 分解已激活，覆盖 ", "Kronecker factorization active for ")
#define L_PROG_OfFreq              TL(" / ", " of ")
#define L_PROG_HigherDiag          TL(" 个正频率；更高频率使用对角合成", " positive frequencies; higher frequencies use diagonal synthesis")
#define L_PROG_FreqProgress0       TL("频率进度 0%% (首块完成前 ETA 未知)", "      frequency progress 0% (ETA unknown until first block completes)")
#define L_PROG_FreqProgress        TL("频率进度 ", "frequency progress ")
#define L_PROG_IFFTBatch           TL("正在对所有网格点执行 FFTW 批量逆变换", "      FFTW batch inverse transform for all grid points")
#define L_PROG_IFFTStart           TL("正在对所有网格点执行 FFTW 批量逆变换", "      FFTW batch inverse transform for all grid points")
#define L_PROG_IFFTDone            TL("FFTW 批量逆变换完成，耗时 ", "      FFTW batch inverse transform complete in ")
#define L_PROG_CopyProgress        TL("复制进度 ", "      copy-out progress ")
#define L_PROG_ComponentComplete   TL("分量完成", "    component complete")

#define L_PROG_LoadingUserWind     TL("正在加载用户风速时间序列", " Loading user wind-speed time series")
#define L_PROG_UniformWindOnly     TL("WindModel=UNIFORM；仅生成平均风廓线，不含湍流", " WindModel=UNIFORM; generating mean profile only without turbulence")
#define L_PROG_DeterministicEvent  TL("WindModel 为确定性 IEC 事件；生成事件风场，不含谱湍流", " WindModel is a deterministic IEC event; generating event field without spectral turbulence")
#define L_PROG_GenMannSeries       TL("正在为所有点生成 Mann 时间序列", " Generating Mann time series for all points")
#define L_PROG_ApplyReynolds       TL("正在施加雷诺应力缩放（在叠加平均风之前）", " Applying Reynolds-stress scaling before mean-wind addition")
#define L_PROG_ApplyMeanProfile    TL("正在叠加平均风廓线和 IEC 事件形状", " Applying mean wind profile and IEC event shape")
#define L_PROG_CalcSpectral        TL("正在计算互谱密度矩阵", " Calculating cross-spectral density matrices")
#define L_PROG_GenTimeSeries       TL("正在为所有点生成时间序列", " Generating time series for all points")
#define L_PROG_Mann3DIFFT          TL("正在执行 Mann 3D FFTW 逆变换", "      Mann 3D FFTW inverse transform")

// Progress — component matrices detail
#define L_PROG_LegacyKronecker     TL("经典 Kronecker 严格相干", "legacy Kronecker strict coherence")
#define L_PROG_StrictCoherence     TL("严格相干", "strict coherence")
#define L_PROG_UncorrelatedPhases  TL("非相关空间相位", "uncorrelated spatial phases")

// Progress — frequency / copy loop
#define L_PROG_Elapsed             TL("，已用 ", ", elapsed ")
#define L_PROG_ETA                 TL("，预计剩余 ", ", ETA ")
#define L_PROG_FreqUnknownETA      TL("频率进度 0%% (首块完成前 ETA 未知)", "frequency progress 0% (ETA unknown until first block completes)")

// Progress — Mann
#define L_PROG_GridPrepared        TL(" 网格/时间已准备: ", " Grid/time prepared: ")
#define L_PROG_MannCalcTensor      TL(" 正在计算 Mann 3D 谱张量场: ", " Calculating Mann 3D spectral tensor field: ")
#define L_PROG_MannProgress        TL("      Mann 张量进度 ", "      Mann tensor progress ")

// Progress — top level
#define L_PROG_CPUSeconds          TL(" CPU 秒", " CPU seconds used")

// Warning — dynamic Kronecker
#define L_WARN_KroneckerCompPrefix TL("分量 ", "Component ")
#define L_WARN_KroneckerCompUses   TL(" 使用了经典 WindL Kronecker 近似加速，覆盖前 ", " uses the legacy WindL Kronecker coherence acceleration for the first ")
#define L_WARN_KroneckerCompEnd    TL(" 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。", " positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.")

// ============================================================================
// SimWind — Sum output labels
// ============================================================================

#define L_SUM_Title              TL("Qahse WindL SimWind 摘要", "Qahse WindL SimWind Summary")
#define L_SUM_Separator          TL("===========================", "===========================")
#define L_SUM_Input              TL("输入", "Input")
#define L_SUM_Grid               TL("网格", "Grid")
#define L_SUM_GenCostEstimate    TL("生成成本估算", "Generation Cost Estimate")
#define L_SUM_DerivedIEC         TL("导出的 IEC 参数", "Derived IEC Parameters")
#define L_SUM_BladedExport       TL("Bladed 导出参数", "Bladed Export Parameters")
#define L_SUM_OutputFiles        TL("输出文件", "Output Files")
#define L_SUM_InputKeywordStatus TL("输入关键字状态", "Input Keyword Status")
#define L_SUM_Statistics         TL("统计量", "Statistics")
#define L_SUM_Warnings           TL("警告", "Warnings")

// ============================================================================
// WindLBatch — Parse error strings
// ============================================================================

#define L_BATCH_InvalidBool              TL("无效的布尔令牌", "Invalid boolean token")
#define L_BATCH_InvalidEnum              TL("无效的枚举令牌", "Invalid enum token")
#define L_BATCH_InvalidNumeric           TL("无效的数值令牌", "Invalid numeric token")
#define L_BATCH_UnsupportedLauncher      TL("不支持的 BatchLauncher", "Unsupported BatchLauncher")
#define L_BATCH_RequiresBatchExcel       TL("Mode=BATCH 需要 BatchExcel", "Mode=BATCH requires BatchExcel")
#define L_BATCH_SheetNotFound            TL("未找到批处理表单", "Batch sheet not found")
#define L_BATCH_SheetEmpty               TL("批处理工作簿表单为空", "Batch workbook sheet is empty")
#define L_BATCH_DuplicateHeader          TL("重复的批处理列头", "Duplicate batch header")
#define L_BATCH_UnsupportedColumn        TL("不支持的批处理列", "Unsupported batch column")
#define L_BATCH_UnsupportedOverrideCol   TL("不支持的覆盖列", "Unsupported Override column")
#define L_BATCH_RequiresCaseNameCol      TL("批处理工作簿需要 CaseName 列", "Batch workbook requires a CaseName column")
#define L_BATCH_RowNoCaseName            TL("批处理行 %s 有数据但缺少 CaseName", "Batch row %s has data but no CaseName")
#define L_BATCH_DuplicateCaseName        TL("批处理工作簿中 CaseName 重复", "Duplicate CaseName in batch workbook")
#define L_BATCH_UnsupportedOverrideKey   TL("不支持的覆盖键", "Unsupported override key")
#define L_BATCH_FailCreateLog            TL("无法创建批处理日志文件", "Failed to create batch log file")
#define L_BATCH_FailLaunchSubprocess     TL("无法为案例 qwd 启动子进程", "Failed to launch subprocess for case qwd")
#define L_BATCH_RequiresModeBatch        TL("WindL 批处理运行器要求输入 .qwd 中 Mode=BATCH", "WindL batch runner requires Mode=BATCH in the input .qwd")

// ============================================================================
// WindLBatch — Progress strings
// ============================================================================

#define L_BATCH_ReadingWorkbook  TL("正在读取批处理 Excel 工作簿 \"", " Reading batch Excel workbook \"")
#define L_BATCH_WorkbookLoaded   TL("批处理工作簿已加载", " Batch workbook loaded")
#define L_BATCH_StartCase        TL("正在启动批处理案例 \"", " Starting batch case \"")
#define L_BATCH_CaseFinished     TL("\" 已完成，状态为 ", "\" finished with status ")

// ============================================================================
// WindLBatch — Summary output strings
// ============================================================================

#define L_BATCH_SummaryValidation   TL("WindL 批处理验证摘要", "WindL batch validation summary")
#define L_BATCH_SummaryGeneration    TL("WindL 批处理生成摘要", "WindL batch generation summary")
#define L_BATCH_SummaryTotalCases    TL("总案例数", "Total cases")
#define L_BATCH_SummarySucceeded     TL("成功", "Succeeded")
#define L_BATCH_SummaryValidated     TL("已验证", "Validated")
#define L_BATCH_SummaryFailed        TL("失败", "Failed")
#define L_BATCH_SummaryInvalid       TL("无效", "Invalid")
#define L_BATCH_SummarySkipped       TL("已跳过", "Skipped")

// ============================================================================
// WindLBatch — Status / result strings (used as JSON/CSV field values)
// ============================================================================

#define L_STATUS_Success    TL("success", "success")
#define L_STATUS_Failed     TL("failed", "failed")
#define L_STATUS_Skipped    TL("skipped", "skipped")
#define L_STATUS_Validated  TL("validated", "validated")
#define L_STATUS_Invalid    TL("invalid", "invalid")

// ============================================================================
// WindLBatch — Case result message strings
// ============================================================================

#define L_CASE_PreValidationPassed       TL("预校验通过", "Pre-validation passed")
#define L_CASE_DisabledByEnabled          TL("案例被 Enabled=false 禁用", "Case disabled by Enabled=false")
#define L_CASE_WindFieldGenerated        TL("风场生成成功", "Wind field generated successfully")
#define L_CASE_RunningInproc             TL("正在运行进程内 WindL 批处理案例", "Running in-process WindL batch case")
#define L_CASE_RunningSubprocess         TL("正在运行子进程 WindL 批处理案例", "Running subprocess WindL batch case")
#define L_CASE_SubprocessExitedWithCode  TL("子进程退出，代码为 ", "Subprocess exited with code ")
