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
#include <filesystem>

#include "io/ZConsole.hpp"
#include "io/LogHelper.h"
#include "io/ZString.hpp"
#include "WindL/SimWind.hpp"
#include "WindL/Batch/WindLBatch.hpp"
#include "WindL/IO/WindL_IO_Subs.hpp"
#include "IO/LocaleString.hpp"

#ifdef _WIN32

#endif

int main(int argc, char *argv[])

{
    // 初始化国际化语言设置
    LocaleText::SetLanguage(LocaleText::DetectSystemLanguage());

    // 设置控制台输入为 UTF-8
    SetConsoleOutputCP(CP_UTF8); // 设置控制台输出为 UTF-8
    SetConsoleCP(CP_UTF8);
    // 初始化Log系统，防止log输出崩溃时无法记录日志
    LogHelper::DisplayInformation();
    try
    {
        ZConsole::SetTitle(L_CLI_Title);
    }
    catch(const std::exception& e)
    {

    }
    



    // 如果没有命令行参数，则显示帮助信息并进入交互式命令输入模式
    if (argc == 1)
    {

        LogHelper::WriteLogO(L_CLI_Banner);
        LogHelper::WriteLogO(L_CLI_Usage);
        LogHelper::WriteLogO(L_CLI_Options);
        LogHelper::WriteLogO(L_CLI_OptionTest);
        LogHelper::WriteLogO(TL("  --linearize <文件.lin>    从 .lin 文件运行线性化，无 GUI", "  --linearize <file.lin> Run linearization from .lin file, no GUI"));
        LogHelper::WriteLogO(L_CLI_OptionQWD);
        LogHelper::WriteLogO(TL("                         (Mode=0: 生成, Mode=1: 导入, Mode=2: 批量 Excel)", "                         (Mode=0: generate, Mode=1: import, Mode=2: batch from Excel)"));
        LogHelper::WriteLogO(TL("  --mbdl <文件.qmd>      从 .qmd 文件运行独立 MBDL 结构动力学", "  --mbdl <file.qmd>     Run standalone MBDL structural dynamics from .qmd file"));
        LogHelper::WriteLogO(TL("  --windl-models        显示 WindL OOP 模型目录和路由 ID", "  --windl-models        Print WindL OOP model catalogs and route IDs"));
        LogHelper::WriteLogO(TL("  --qod <文件.qoe>       从 .qod 文件运行独立海洋模式", "  --qod <file.qoe>      Run standalone ocean mode from .qod file"));
        LogHelper::WriteLogO(TL("  --pcsl <输入文件>     从输入文件运行 PCSL 截面分析", "  --pcsl <input_file>   Run PCSL cross-section analysis from input file"));
        LogHelper::WriteLogO(TL("  --run <文件.trb|文件.sim> [选项]  从定义文件运行仿真，无 GUI", "  --run <file.trb|file.sim> [options]  Run simulation from definition file, no GUI"));

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
                std::cerr << std::string(L_CLI_ErrorQwdPath) + "\n";
                return 2;
            }

            try
            {
                const std::string qwdPath = std::filesystem::absolute(argv[i + 1]).string();
                const auto input = ReadWindLInput(qwdPath);
                const auto progress = [](const std::string &message) {
                    std::cout << message << std::endl;
                };

                if (input.mode == Mode::GENERATE)
                {
                    std::cout << std::string(L_CLI_RunningSimWind) << qwdPath << "\".\n" << std::flush;
                    const auto result = SimWind::Generate(input, progress);
                    std::cout << L_CLI_GeneratedFiles << "\n";
                    if (!result.btsPath.empty())
                        std::cout << L_CLI_OutputBTS << result.btsPath << "\n";
                    if (!result.bladedWndPath.empty())
                        std::cout << L_CLI_OutputBladedWND << result.bladedWndPath << "\n";
                    if (!result.turbsimWndPath.empty())
                        std::cout << L_CLI_OutputTurbSimWND << result.turbsimWndPath << "\n";
                    if (!result.sumPath.empty())
                        std::cout << L_CLI_OutputSUM << result.sumPath << "\n";
                    return 0;
                }

                if (input.mode == Mode::BATCH)
                {
                    std::cout << std::string(L_CLI_RunningBatch) + " with template \"" << qwdPath << "\".\n" << std::flush;
                    const auto batch = WindLBatch::RunFromFile(qwdPath, std::filesystem::absolute(argv[0]).string(), progress);
                    std::cout << L_CLI_BatchSummary << "\n";
                    std::cout << L_CLI_BatchManifest << batch.manifestPath << "\n";
                    std::cout << TL("  CSV：", "  CSV: ") << batch.csvPath << "\n";
                    std::cout << L_CLI_BatchStatus << batch.summaryPath << "\n";
                    std::cout << TL("  成功：", "  Succeeded: ") << batch.succeeded << "\n";
                    std::cout << TL("  失败：", "  Failed: ") << batch.failed << "\n";
                    std::cout << TL("  无效：", "  Invalid: ") << batch.invalid << "\n";
                    std::cout << TL("  跳过：", "  Skipped: ") << batch.skipped << "\n";
                    std::cout << TL("  已验证：", "  Validated: ") << batch.validated << "\n";
                    return (batch.failed == 0 && batch.invalid == 0) ? 0 : 1;
                }

                std::cerr << std::string(L_CLI_ImportNotImpl) + " for --qwd.\n";
                return 2;
            }
            catch (const std::exception &ex)
            {
                std::cerr << L_CLI_SimWindFailed << ex.what() << "\n";
                return 1;
            }
        }
    }

    return 0;
}
