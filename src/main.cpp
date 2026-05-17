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
#include <array>
#include <csignal>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>

#include "io/ZConsole.hpp"
#include "io/LogHelper.h"
#include "io/ZString.hpp"
#include "SiMwind/SimWind.hpp"
#include "SiMwind/Batch/SimWindBatch.hpp"
#include "SiMwind/IO/SimWind_IO_Subs.hpp"
#include "WindL/WindL.hpp"
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
        LogHelper::WriteLogO(TL("                         (Mode=0: SimWind 生成, Mode=1: SimWind 批量 Excel)", "                         (Mode=0: SimWind generate, Mode=1: SimWind batch from Excel)"));
        LogHelper::WriteLogO(TL("  --windl <文件.dat>    读取 WindL 风场输入并输出摘要", "  --windl <file.dat>   Load WindL wind input and print a summary"));
        LogHelper::WriteLogO(TL("                         可选: --check-series [--point-index iz iy] [--max-steps n]", "                         Optional: --check-series [--point-index iz iy] [--max-steps n]"));
        LogHelper::WriteLogO(TL("                         可选: --check-format-series [--format-tolerance tol]", "                         Optional: --check-format-series [--format-tolerance tol]"));
        LogHelper::WriteLogO(TL("  --mbdl <文件.qmd>      从 .qmd 文件运行独立 MBDL 结构动力学", "  --mbdl <file.qmd>     Run standalone MBDL structural dynamics from .qmd file"));
        LogHelper::WriteLogO(TL("  --windl-models        显示 WindL OOP 模型目录和路由 ID", "  --windl-models        Print WindL OOP model catalogs and route IDs"));
        LogHelper::WriteLogO(TL("  --qod <文件.qoe>       从 .qoe 文件运行独立海洋模式", "  --qod <file.qoe>      Run standalone ocean mode from .qoe file"));
        LogHelper::WriteLogO("  --qhd <file.qhd>      Run standalone HydroL hydrodynamics from .qhd file");
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
        if (arg == "--windl")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing WindL input path.\n";
                return 2;
            }

            try
            {
                const std::string windlPath = std::filesystem::absolute(argv[i + 1]).string();
                bool checkSeries = false;
                bool checkFormatSeries = false;
                int checkIz = -1;
                int checkIy = -1;
                int maxSteps = 0;
                double formatTolerance = 5.0e-2;
                for (int j = i + 2; j < argc; ++j)
                {
                    const std::string option = argv[j] ? argv[j] : "";
                    if (option == "--check-series")
                    {
                        checkSeries = true;
                    }
                    else if (option == "--check-format-series")
                    {
                        checkFormatSeries = true;
                    }
                    else if (option == "--point-index")
                    {
                        if (j + 2 >= argc)
                        {
                            std::cerr << "--point-index requires iz and iy.\n";
                            return 2;
                        }
                        checkIz = std::stoi(argv[++j]);
                        checkIy = std::stoi(argv[++j]);
                    }
                    else if (option == "--max-steps")
                    {
                        if (j + 1 >= argc)
                        {
                            std::cerr << "--max-steps requires a positive integer.\n";
                            return 2;
                        }
                        maxSteps = std::stoi(argv[++j]);
                    }
                    else if (option == "--format-tolerance")
                    {
                        if (j + 1 >= argc)
                        {
                            std::cerr << "--format-tolerance requires a numeric value.\n";
                            return 2;
                        }
                        formatTolerance = std::stod(argv[++j]);
                    }
                }

                const auto progress = [](const std::string &message) {
                    std::cout << message << std::endl;
                };
                const auto wind = WindL::LoadFromFile(windlPath, progress);
                const auto &input = wind.Input();
                std::cout << "WindL input: " << windlPath << "\n";
                std::cout << "WindType: " << static_cast<int>(input.windType) << "\n";
                std::cout << "CycleWind: " << (input.cycleWind ? "true" : "false") << "\n";
                if (wind.HasImportedField())
                {
                    const auto *field = wind.ImportedField();
                    std::cout << "Imported grid: " << field->ny << " x " << field->nz
                              << ", steps=" << field->nSteps << ", dt=" << field->dt << "\n";
                }
                else
                {
                    std::cout << "Runtime source: analytic wind\n";
                }
                if (checkSeries)
                {
                    if (!wind.HasImportedField())
                    {
                        std::cerr << "Series check requires an imported WindL wind file.\n";
                        return 2;
                    }

                    const auto *field = wind.ImportedField();
                    const int iz = checkIz >= 0 ? checkIz : field->nz / 2;
                    const int iy = checkIy >= 0 ? checkIy : field->ny / 2;
                    if (iz < 0 || iz >= field->nz || iy < 0 || iy >= field->ny)
                    {
                        std::cerr << "Point index is out of range: iz=" << iz << ", iy=" << iy << ".\n";
                        return 2;
                    }

                    const int stepsToCheck = maxSteps > 0 ? std::min(maxSteps, field->nSteps) : field->nSteps;
                    const double y = field->yCoords[static_cast<std::size_t>(iy)];
                    const double z = field->zCoords[static_cast<std::size_t>(iz)];
                    WindVelocityOptions options;
                    options.cycleWind = false;
                    options.autoFieldShift = false;
                    options.interpMethod = InterpMethod::TRILINEAR;

                    std::array<double, 3> maxAbsDiff{0.0, 0.0, 0.0};
                    std::array<double, 3> rawFirst{0.0, 0.0, 0.0};
                    std::array<double, 3> rawLast{0.0, 0.0, 0.0};
                    for (int step = 0; step < stepsToCheck; ++step)
                    {
                        const double time = field->timeCoords[static_cast<std::size_t>(step)];
                        const auto sampled = wind.VelocityAt(0.0, y, z, time, options);
                        for (int comp = 0; comp < 3; ++comp)
                        {
                            const double raw = field->At(comp, step, iz, iy);
                            if (step == 0)
                                rawFirst[static_cast<std::size_t>(comp)] = raw;
                            if (step == stepsToCheck - 1)
                                rawLast[static_cast<std::size_t>(comp)] = raw;
                            maxAbsDiff[static_cast<std::size_t>(comp)] =
                                std::max(maxAbsDiff[static_cast<std::size_t>(comp)], std::fabs(raw - sampled[static_cast<std::size_t>(comp)]));
                        }
                    }

                    const double tolerance = 1.0e-9;
                    const bool pass = maxAbsDiff[0] <= tolerance && maxAbsDiff[1] <= tolerance && maxAbsDiff[2] <= tolerance;
                    std::cout << std::setprecision(12);
                    std::cout << "SeriesCheck: " << (pass ? "PASS" : "FAIL") << "\n";
                    std::cout << "  PointIndex: iz=" << iz << ", iy=" << iy << "\n";
                    std::cout << "  PointCoord: y=" << y << ", z=" << z << "\n";
                    std::cout << "  ComparedSteps: " << stepsToCheck << " / " << field->nSteps << "\n";
                    std::cout << "  FirstUVW: " << rawFirst[0] << ", " << rawFirst[1] << ", " << rawFirst[2] << "\n";
                    std::cout << "  LastUVW: " << rawLast[0] << ", " << rawLast[1] << ", " << rawLast[2] << "\n";
                    std::cout << "  MaxAbsDiffUVW: " << maxAbsDiff[0] << ", " << maxAbsDiff[1] << ", " << maxAbsDiff[2] << "\n";
                    return pass ? 0 : 1;
                }
                if (checkFormatSeries)
                {
                    const std::array<std::string, 3> labels{"TURBSIM_WND", "BLADED_WND", "TURBSIM_BTS"};
                    std::array<WindImportMetadata, 3> metadata{};
                    metadata[0].filePath = input.turWindFilePath;
                    metadata[0].format = WndFormat::TURBSIM_WND;
                    metadata[1].filePath = input.bldWindFilePath;
                    metadata[1].format = WndFormat::BLADED_WND;
                    metadata[2].filePath = input.iecWindFilePath;
                    metadata[2].format = WndFormat::TURBSIM_BTS;
                    for (auto &item : metadata)
                    {
                        item.hubHeight = input.refHeight;
                        item.refHeight = input.refHeight;
                        item.meanWindSpeed = input.hWindSpeed;
                    }

                    std::array<WindField, 3> fields{};
                    for (std::size_t idx = 0; idx < metadata.size(); ++idx)
                    {
                        if (metadata[idx].filePath.empty())
                        {
                            std::cerr << "Missing WindL path for " << labels[idx] << ".\n";
                            return 2;
                        }
                        fields[idx] = WindField::ReadAny(metadata[idx].filePath, metadata[idx].format, metadata[idx]);
                    }

                    const WindField &reference = fields[0];
                    const int iz = checkIz >= 0 ? checkIz : reference.nz / 2;
                    const int iy = checkIy >= 0 ? checkIy : reference.ny / 2;
                    if (iz < 0 || iz >= reference.nz || iy < 0 || iy >= reference.ny)
                    {
                        std::cerr << "Point index is out of range: iz=" << iz << ", iy=" << iy << ".\n";
                        return 2;
                    }

                    int commonSteps = reference.nSteps;
                    bool sameGrid = true;
                    for (std::size_t idx = 1; idx < fields.size(); ++idx)
                    {
                        commonSteps = std::min(commonSteps, fields[idx].nSteps);
                        sameGrid = sameGrid &&
                                   fields[idx].ny == reference.ny &&
                                   fields[idx].nz == reference.nz &&
                                   std::fabs(fields[idx].dt - reference.dt) <= 1.0e-9;
                    }
                    const int stepsToCheck = maxSteps > 0 ? std::min(maxSteps, commonSteps) : commonSteps;
                    if (stepsToCheck <= 0)
                    {
                        std::cerr << "No common time steps available for format comparison.\n";
                        return 2;
                    }

                    std::array<std::array<double, 3>, 3> maxAbsDiff{};
                    for (int step = 0; step < stepsToCheck; ++step)
                    {
                        for (int pair = 0; pair < 3; ++pair)
                        {
                            const int a = pair == 0 ? 0 : (pair == 1 ? 0 : 1);
                            const int b = pair == 0 ? 1 : (pair == 1 ? 2 : 2);
                            for (int comp = 0; comp < 3; ++comp)
                            {
                                const double lhs = fields[static_cast<std::size_t>(a)].At(comp, step, iz, iy);
                                const double rhs = fields[static_cast<std::size_t>(b)].At(comp, step, iz, iy);
                                maxAbsDiff[static_cast<std::size_t>(pair)][static_cast<std::size_t>(comp)] =
                                    std::max(maxAbsDiff[static_cast<std::size_t>(pair)][static_cast<std::size_t>(comp)], std::fabs(lhs - rhs));
                            }
                        }
                    }

                    double maxOverall = 0.0;
                    for (const auto &pairDiff : maxAbsDiff)
                    {
                        for (double value : pairDiff)
                            maxOverall = std::max(maxOverall, value);
                    }
                    const bool pass = sameGrid && maxOverall <= formatTolerance;
                    std::cout << std::setprecision(12);
                    std::cout << "FormatSeriesCheck: " << (pass ? "PASS" : "FAIL") << "\n";
                    std::cout << "  PointIndex: iz=" << iz << ", iy=" << iy << "\n";
                    std::cout << "  PointCoord: y=" << reference.yCoords[static_cast<std::size_t>(iy)]
                              << ", z=" << reference.zCoords[static_cast<std::size_t>(iz)] << "\n";
                    std::cout << "  CommonGrid: " << (sameGrid ? "true" : "false") << "\n";
                    std::cout << "  ComparedSteps: " << stepsToCheck << " / " << commonSteps << "\n";
                    std::cout << "  Tolerance: " << formatTolerance << "\n";
                    std::cout << "  MaxAbsDiff " << labels[0] << " vs " << labels[1] << ": "
                              << maxAbsDiff[0][0] << ", " << maxAbsDiff[0][1] << ", " << maxAbsDiff[0][2] << "\n";
                    std::cout << "  MaxAbsDiff " << labels[0] << " vs " << labels[2] << ": "
                              << maxAbsDiff[1][0] << ", " << maxAbsDiff[1][1] << ", " << maxAbsDiff[1][2] << "\n";
                    std::cout << "  MaxAbsDiff " << labels[1] << " vs " << labels[2] << ": "
                              << maxAbsDiff[2][0] << ", " << maxAbsDiff[2][1] << ", " << maxAbsDiff[2][2] << "\n";
                    return pass ? 0 : 1;
                }
                return 0;
            }
            catch (const std::exception &ex)
            {
                std::cerr << "WindL failed: " << ex.what() << "\n";
                return 1;
            }
        }

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
                const auto input = ReadSimWindInput(qwdPath);
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
                    const auto batch = SimWindBatch::RunFromFile(qwdPath, std::filesystem::absolute(argv[0]).string(), progress);
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

                std::cerr << "Unsupported qwd Mode for SimWind.\n";
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
