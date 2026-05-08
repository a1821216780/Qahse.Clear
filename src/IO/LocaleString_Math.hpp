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
// 该文件集中管理 Math 模块中所有可翻译的字符串常量，包括 MathHelper、FrequenceHelper、
// LinearAlgebraHelper、IntegrationHelper 和 InterpolateHelper 的双语错误消息定义。
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once
#include "LocaleText.hpp"

// MathHelper
#define L_MATH_StepZero         TL("step不能为0", "step cannot be zero")
#define L_MATH_IndexNotFound    TL("未找到索引!", "Index not found!")

// FrequenceHelper
#define L_FFT_AllocFail         TL("FFTW内存分配失败", "FFTW memory allocation failed")
#define L_FFT_PlanFail          TL("FFTW计划创建失败", "FFTW plan creation failed")
#define L_FFT_LengthMismatch    TL("实部和虚部数组长度不匹配", "Real and imaginary array lengths do not match")

// LinearAlgebraHelper
#define L_LINALG_SourceNotEmpty     TL("原始数组不为空", "Source array must be empty")
#define L_LINALG_TargetLonger       TL("目标长度必须大于原始长度", "Target length must be greater than source length")
#define L_LINALG_NullVector         TL("输入向量不能为空", "The input vector must not be null or empty")
#define L_LINALG_SizeMustBe1or2     TL("必须使用size a=1 or a=2", "Must use size a=1 or a=2")
#define L_LINALG_CannotOpen         TL("无法打开文件", "Cannot open file")
#define L_LINALG_NoData             TL("文件中未找到有效数据", "No valid data found in file")
#define L_LINALG_VcatMismatch       TL("a 与 b 的长度不一致，无法 vcat！", "a and b have different lengths, cannot vcat!")
#define L_LINALG_SkewSize           TL("向量必须恰好有3个元素用于斜对称矩阵", "Vector must have exactly 3 elements for skew matrix")
#define L_LINALG_MinTwoElements     TL("输入向量必须包含至少两个元素", "Input vector must contain at least two elements")
#define L_LINALG_NormType           TL("不支持的范数类型", "Unsupported norm type")
#define L_LINALG_RepetitionZero     TL("重复次数必须大于0", "Number of repetitions must be greater than 0")
#define L_LINALG_Dim1or2            TL("维度必须为1或2", "Dimension must be 1 or 2")

// IntegrationHelper
#define L_INTEG_SizeMismatch        TL("输入尺寸不匹配", "Input size mismatch")

// InterpolateHelper
#define L_INTERP_XYSameSize         TL("x and y 必须具有相同的大小", "x and y must have the same size")
#define L_INTERP_InputEmpty         TL("输入数据不能为空", "Input data cannot be empty")
#define L_INTERP_ValidXY            TL("必须提供有效的x和y数据，具有相同的非零长度", "Must provide valid x and y data with same non-zero length")
#define L_INTERP_UnknownType        TL("未知的插值类型", "Unknown interpolation type")
