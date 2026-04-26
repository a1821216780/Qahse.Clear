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
// 该文件用来定义executable的版本信息，包含主版本号、次版本号、修订号和构建号，以及一
// 些字符串形式的版本信息和版权信息。这些信息可以在程序中使用，也可以在生成的可执行文
// 件的属性中查看。
//
// WARNING!!!
// 定义的 VER_FILE_DESCRIPTION 不允许有中文！！！！
//
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#define VER_MAJOR 1
#define VER_MINOR 0
#define VER_PATCH 0
#define VER_BUILD 0

// Comma-separated for FILEVERSION / PRODUCTVERSION fields
#define VER_FILE_VERSION VER_MAJOR, VER_MINOR, VER_PATCH, VER_BUILD
#define VER_PRODUCT_VERSION VER_MAJOR, VER_MINOR, VER_PATCH, VER_BUILD

// String forms
#define VER_FILE_VERSION_STR "1.0.0.0"
#define VER_PRODUCT_VERSION_STR "1.0.0"

#define VER_COMPANY_NAME "Qahse by ZZZ"
#define VER_FILE_DESCRIPTION "Qahse www.openwecd.fun - Horizontal Axis Wind Turbine Aero-Hydro-Servo-Elastic Simulator"
#define VER_INTERNAL_NAME "Qahse"
#define VER_ORIGINAL_FILENAME "Qahse.exe"
#define VER_PRODUCT_NAME "Qahse"
#define VER_COPYRIGHT "Copyright (C) 2026 Qahse Team. All rights reserved."
