//**********************************************************************************************************************************
//许可证说明
//版权所有(C) 2021, 2025  赵子祯
//
//根据 Boost 软件许可证 - 版本 1.0 - 2003年8月17日
//您不得使用此文件，除非符合许可证。
//您可以在以下网址获得许可证副本
//
//     http://www.hawtc.cn/licenses.txt
//
//该软件按“原样”提供，不提供任何明示或暗示的保证，包括但不限于适销性、特定用途的适用性、所有权和非侵权。在任何情况下，版权持有人或任何分
// 发软件的人都不对任何索赔、损害或其他责任负责，无论是在合同诉讼、侵权诉讼或其他诉讼中，还是在软件使用或其他交易中产生的。
//
//**********************************************************************************************************************************
#pragma once

// ───────────────────────────────── File Info ─────────────────────────────────
//
//该文件定义了物理常数、空气动力学分析中使用的离散化参数、控制器数组大小以及一个用于导出函数的宏。
//
//──────────────────────────────────────────────────────────────────────────────





// Physical constants
#define M_PI                    3.14159265358979323846
#define KINVISCAIR              1.647e-05
#define DENSITYAIR              1.225
#define KINVISCWATER            1.307e-6
#define DENSITYWATER            1025
#define GRAVITY                 9.80665
#define TINYVAL                 1.0e-10
#define ZERO_MASS               1e-5      // 原值 1e-5 导致 KKT 矩阵条件数 ~7e15, SparseQR 无法正确执行约束

// Airfoil discretization parameters
#define IQX                     302
#define IBX                     604

// Blade analysis parameters
#define MAXBLADESTATIONS        200

// Controller array sizes
#define arraySizeTUB            550
#define arraySizeBLADED         550
#define arraySizeDTU            100

typedef double Real;//c语言当中使用typeof定义一个新的类型，Real就是double类型的别名,在代码中使用Real就相当于使用double类型，这样做的好处是如果以后需要更改数据类型，只需要修改typedef语句即可，而不需要修改代码中所有使用该类型的地方。

#define QAHSE_API

#ifdef _WIN32
    #ifdef QAHSE_API
        #define QAPI __declspec(dllexport)
    #else
        #define QAPI __declspec(dllimport)
    #endif
#else
    #define QAPI __attribute__((visibility("default")))
#endif

#define QAPI_C extern "C" QAPI