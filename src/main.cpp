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
// 该文件是项目的主入口文件，包含了程序的核心逻辑和执行流程。它负责初始化程序环境，处理
// 命令行参数，调用其他模块的功能，并最终启动应用程序的主循环。通过合理的模块划分和清晰
// 的代码结构，确保了程序的可维护性和扩展性。该文件还包含了必要的头文件引用和命名空间使用，以支持程序的正常编译和运行。
//
// ──────────────────────────────────────────────────────────────────────────────
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <clocale>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <csignal>
#include <algorithm>
#include <cctype>

#include "io/ZConsole.hpp"
#include "io/LogHelper.h"
#include "io/ZString.hpp"
#include "WindL/SimWind.hpp"

#ifdef _WIN32

#endif

int main(int argc, char *argv[])

{

    // 设置控制台输入为 UTF-8
    SetConsoleOutputCP(CP_UTF8); // 设置控制台输出为 UTF-8
    SetConsoleCP(CP_UTF8);
    // 初始化Log系统，防止log输出崩溃时无法记录日志
    LogHelper::DisplayInformation();
    try
    {
        ZConsole::SetTitle("OpenWECD.Qahse CLI");
    }
    catch(const std::exception& e)
    {

    }
    



    // 如果没有命令行参数，则显示帮助信息并进入交互式命令输入模式
    if (argc == 1)
    {

        LogHelper::WriteLogO("Qahse Command Line Interface (CLI) - Version 1.0");
        LogHelper::WriteLogO("Usage: Qahse [options]");
        LogHelper::WriteLogO("Options:");
        LogHelper::WriteLogO("  --test                 Run QFEM tests and exit");
        LogHelper::WriteLogO("  --linearize <file.lin> Run linearization from .lin file, no GUI");
        LogHelper::WriteLogO("  --qwd <file.qwd>      Run standalone wind mode from .qwd file");
        LogHelper::WriteLogO("                         (Mode=0: generate, Mode=1: import, Mode=2: batch from Excel)");
        LogHelper::WriteLogO("  --mbdl <file.qmd>     Run standalone MBDL structural dynamics from .qmd file");
        LogHelper::WriteLogO("  --windl-models        Print WindL OOP model catalogs and route IDs");
        LogHelper::WriteLogO("  --windl-batch <excel> <template.qwd> [output_dir] [sheet_name] [threads] [launcher]");
        LogHelper::WriteLogO("                         Batch wind generation from Excel parameter table");
        LogHelper::WriteLogO("                         launcher: inproc(default) / cmd / powershell");
        LogHelper::WriteLogO("  --windl-batch-validate <excel> <template.qwd> [output_dir] [sheet_name]");
        LogHelper::WriteLogO("                         Batch parameter validation only (no wind generation)");
        LogHelper::WriteLogO("  --qod <file.qoe>      Run standalone ocean mode from .qod file");
        LogHelper::WriteLogO("  --pcsl <input_file>   Run PCSL cross-section analysis from input file");
        LogHelper::WriteLogO("  --run <file.trb|file.sim> [options]  Run simulation from definition file, no GUI");

        // 对argc, char *argv进行赋值
        ZConsole::Write(" >");
        std::string line;
        std::getline(std::cin, line);
        auto cmds = ZString::Split(line, ' ', true);
        argc = cmds.size() + 1;
        argv = new char *[argc];
        for (size_t i = 1; i < argc; i++)
        {
            argv[i] = new char[cmds[i - 1].size() + 1];
            strcpy(argv[i], cmds[i - 1].c_str());
        }
    }


    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--qwd")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "--qwd requires a .qwd file path\n";
                return 2;
            }

            try
            {
                std::cout << " Running WindL SimWind with input file \"" << argv[i + 1] << "\".\n" << std::flush;
                const auto result = SimWind::GenerateFromFile(argv[i + 1], [](const std::string &message) {
                    std::cout << message << std::endl;
                });
                std::cout << "SimWind generated wind files:\n";
                if (!result.btsPath.empty())
                    std::cout << "  BTS: " << result.btsPath << "\n";
                if (!result.bladedWndPath.empty())
                    std::cout << "  Bladed WND: " << result.bladedWndPath << "\n";
                if (!result.turbsimWndPath.empty())
                    std::cout << "  TurbSim-compatible WND: " << result.turbsimWndPath << "\n";
                if (!result.sumPath.empty())
                    std::cout << "  SUM: " << result.sumPath << "\n";
                return 0;
            }
            catch (const std::exception &ex)
            {
                std::cerr << "SimWind failed: " << ex.what() << "\n";
                return 1;
            }
        }
    }

    return 0;
}
