#pragma once

#include <string>

#include "IO/Serializer.hpp"

/**
 * @brief WindL 输入参数的序列化数据对象。
 *
 * 这个类把 WindL 计算所需的输入项封装为可读写的数据成员，并基于 Serializer
 * 统一支持文本、YAML 和二进制格式的读取、保存与格式转换。
 *
 * 典型使用方式：在派生对象中填写或加载参数，然后调用对应的保存接口输出文件。
 *
 * \code
 * WindL_InputPar par;
 * par.LoadText("WindL.qwd");
 * par.SaveYaml("WindL.yaml");
 *
 * WindL_InputPar::ConvertYamlToBinary("WindL.yaml", "WindL.bin");
 * \endcode
 */
class WindL_InputPar : public Serializer
{
public:

#pragma region 输入参数
    /// @brief 运行模式编号，决定后续输入参数的解释方式。
    int Mode = 0;

    /// @brief 是否计算 u 向湍流分量。
    bool CalWu = true;
    /// @brief 是否计算 v 向湍流分量。
    bool CalWv = true;
    /// @brief 是否计算 w 向湍流分量。
    bool CalWw = true;
    /// @brief 是否输出基础风场文件。
    bool WrBlwnd = true;
    /// @brief 是否输出湍流时间序列文件。
    bool WrTrbts = true;
    /// @brief 是否输出风场结果文件。
    bool WrTrwnd = true;

    /// @brief 竖直方向流向角。
    double VFlowAng = 0.0;
    /// @brief 水平方向流向角。
    double HFlowAng = 0.0;
    /// @brief 是否启用循环风场。
    bool CycleWind = true;
    /// @brief 湍流风场输入文件路径。
    std::string TurWindFile;
    /// @brief 插值方法编号。
    int InterpMethod = 0;

    /// @brief 随机数种子。
    int RandSeed = 0;
    /// @brief Y 方向网格点数。
    int NumPointY = 0;
    /// @brief Z 方向网格点数。
    int NumPointZ = 0;
    /// @brief 风场宽度。
    double LenWidthY = 0.0;
    /// @brief 风场高度。
    double LenHeightZ = 0.0;
    /// @brief 风场持续时间。
    double WindDuration = 0.0;
    /// @brief 时间步长。
    double TimeStep = 0.0;
    /// @brief 轮毂高度。
    double HubHt = 0.0;

    /// @brief 湍流模型编号。
    int TurbModel = 0;
    /// @brief 用户自定义湍流文件路径。
    std::string UserTurbFile;
    /// @brief 是否使用 IEC 模拟参数。
    bool UseIECSimmga = false;
    /// @brief IEC 标准编号。
    int IECStandard = 0;
    /// @brief IEC 等级编号。
    int IECClass = 0;
    /// @brief 湍流强度等级编号。
    int TurbulenceClass = 0;
    /// @brief 风模型编号。
    int WindModel = 0;
    /// @brief 风切变类型编号。
    int ShearType = 0;
    /// @brief 用户自定义切变文件路径。
    std::string UserShearFile;
    /// @brief 参考高度。
    double RefHt = 0.0;
    /// @brief 平均风速。
    double MeanWindSpeed = 0.0;
    /// @brief 幂律指数，默认值表示采用模型默认设置。
    std::string PLExp = "default";
    /// @brief 地表粗糙度长度，默认值表示采用模型默认设置。
    std::string Z0 = "default";

    /// @brief Mann 谱尺度长度，默认值表示沿用模型默认设置。
    std::string MannScalelength = "default";
    /// @brief Mann 谱 Gamma 参数，默认值表示沿用模型默认设置。
    std::string MannGamma = "default";
    /// @brief Mann 谱最大尺度，默认值表示沿用模型默认设置。
    std::string MannMaxL = "default";
    /// @brief x 向 u 分量长度尺度，默认值表示沿用模型默认设置。
    std::string VxLu = "default";
    /// @brief x 向 v 分量长度尺度，默认值表示沿用模型默认设置。
    std::string VxLv = "default";
    /// @brief x 向 w 分量长度尺度，默认值表示沿用模型默认设置。
    std::string VxLw = "default";
    /// @brief y 向 u 分量长度尺度，默认值表示沿用模型默认设置。
    std::string VyLu = "default";
    /// @brief y 向 v 分量长度尺度，默认值表示沿用模型默认设置。
    std::string VyLv = "default";
    /// @brief y 向 w 分量长度尺度，默认值表示沿用模型默认设置。
    std::string VyLw = "default";
    /// @brief z 向 u 分量长度尺度，默认值表示沿用模型默认设置。
    std::string VzLu = "default";
    /// @brief z 向 v 分量长度尺度，默认值表示沿用模型默认设置。
    std::string VzLv = "default";
    /// @brief z 向 w 分量长度尺度，默认值表示沿用模型默认设置。
    std::string VzLw = "default";
    /// @brief 地表粗糙度参数。
    double Roughness = 0.0;

    /// @brief 相干模型参数 1，默认值表示沿用模型默认设置。
    std::string CohMod1 = "default";
    /// @brief 相干模型参数 2，默认值表示沿用模型默认设置。
    std::string CohMod2 = "default";
    /// @brief 相干模型参数 3，默认值表示沿用模型默认设置。
    std::string CohMod3 = "default";

    /// @brief 输出风场文件目录。
    std::string WrWndPath;
    /// @brief 输出风场文件名。
    std::string WrWndName;
    /// @brief 是否打印汇总信息。
    bool SumPrint = true;

#pragma endregion 输入参数

    /**
     * @brief 将当前对象的所有公开字段映射到序列化键值。
     *
     * 该函数由基类在读写文本、YAML 或二进制文件时调用。
     * 派生类仅需集中维护字段与键名的对应关系，文件格式转换由基类负责。
     */
    void SerializeFields() override
    {
        ReadOrWrite("Mode", Mode);

        ReadOrWrite("CalWu", CalWu);
        ReadOrWrite("CalWv", CalWv);
        ReadOrWrite("CalWw", CalWw);
        ReadOrWrite("WrBlwnd", WrBlwnd);
        ReadOrWrite("WrTrbts", WrTrbts);
        ReadOrWrite("WrTrwnd", WrTrwnd);

        ReadOrWrite("VFlowAng", VFlowAng);
        ReadOrWrite("HFlowAng", HFlowAng);
        ReadOrWrite("CycleWind", CycleWind);
        ReadOrWrite("TurWindFile", TurWindFile);
        ReadOrWrite("InterpMethod", InterpMethod);

        ReadOrWrite("RandSeed", RandSeed);
        ReadOrWrite("NumPointY", NumPointY);
        ReadOrWrite("NumPointZ", NumPointZ);
        ReadOrWrite("LenWidthY", LenWidthY);
        ReadOrWrite("LenHeightZ", LenHeightZ);
        ReadOrWrite("WindDuration", WindDuration);
        ReadOrWrite("TimeStep", TimeStep);
        ReadOrWrite("HubHt", HubHt);

        ReadOrWrite("TurbModel", TurbModel);
        ReadOrWrite("UserTurbFile", UserTurbFile);
        ReadOrWrite("UseIECSimmga", UseIECSimmga);
        ReadOrWrite("IECStandard", IECStandard);
        ReadOrWrite("IECClass", IECClass);
        ReadOrWrite("TurbulenceClass", TurbulenceClass);
        ReadOrWrite("WindModel", WindModel);
        ReadOrWrite("ShearType", ShearType);
        ReadOrWrite("UserShearFile", UserShearFile);
        ReadOrWrite("RefHt", RefHt);
        ReadOrWrite("MeanWindSpeed", MeanWindSpeed);
        ReadOrWrite("PLExp", PLExp);
        ReadOrWrite("Z0", Z0);

        ReadOrWrite("MannScalelength", MannScalelength);
        ReadOrWrite("MannGamma", MannGamma);
        ReadOrWrite("MannMaxL", MannMaxL);
        ReadOrWrite("VxLu", VxLu);
        ReadOrWrite("VxLv", VxLv);
        ReadOrWrite("VxLw", VxLw);
        ReadOrWrite("VyLu", VyLu);
        ReadOrWrite("VyLv", VyLv);
        ReadOrWrite("VyLw", VyLw);
        ReadOrWrite("VzLu", VzLu);
        ReadOrWrite("VzLv", VzLv);
        ReadOrWrite("VzLw", VzLw);
        ReadOrWrite("Roughness", Roughness);

        ReadOrWrite("CohMod1", CohMod1);
        ReadOrWrite("CohMod2", CohMod2);
        ReadOrWrite("CohMod3", CohMod3);

        ReadOrWrite("WrWndPath", WrWndPath);
        ReadOrWrite("WrWndName", WrWndName);
        ReadOrWrite("SumPrint", SumPrint);
    }

    /**
     * @brief 从文本格式读取 WindL 输入参数。
     * @param filePath 输入文件路径。
     * @param fallbackExtension 当路径不存在时尝试补充的后缀，默认值为 .qwd。
     */
    void LoadText(const std::string &filePath, const std::string &fallbackExtension = ".qwd")
    {
        BeginTextRead(filePath, fallbackExtension);
        SerializeFields();
    }

    /**
     * @brief 将 WindL 输入参数保存为文本格式。
     * @param filePath 输出文件路径。
     * @param templatePath 可选模板文件路径，用于保留原有布局。
     */
    void SaveText(const std::string &filePath, const std::string &templatePath = std::string{})
    {
        BeginTextWrite(templatePath);
        SerializeFields();
        EndTextWrite(filePath);
    }

    /**
     * @brief 从 YAML 格式读取 WindL 输入参数。
     * @param filePath 输入 YAML 文件路径。
     */
    void LoadYaml(const std::string &filePath)
    {
        BeginYamlRead(filePath);
        SerializeFields();
    }

    /**
     * @brief 将 WindL 输入参数保存为 YAML 格式。
     * @param filePath 输出 YAML 文件路径。
     */
    void SaveYaml(const std::string &filePath)
    {
        BeginYamlWrite();
        SerializeFields();
        EndYamlWrite(filePath);
    }

    /**
     * @brief 从二进制格式读取 WindL 输入参数。
     * @param filePath 输入二进制文件路径。
     */
    void LoadBinary(const std::string &filePath)
    {
        BeginBinaryRead(filePath);
        SerializeFields();
    }

    /**
     * @brief 将 WindL 输入参数保存为二进制格式。
     * @param filePath 输出二进制文件路径。
     */
    void SaveBinary(const std::string &filePath)
    {
        BeginBinaryWrite();
        SerializeFields();
        EndBinaryWrite(filePath);
    }

    /**
     * @brief 获取 WrTrwnd 字段在文本输入中的行号。
     * @return 1-based 行号；未找到时返回 -1。
     */
    int WrTrwndLine() const
    {
        return FindKeywordLine("WrTrwnd");
    }

    /**
     * @brief 仅从文本文件读取 WrTrwnd 字段。
     *
     * 该函数适合只需要快速检查是否启用风场输出的场景，不必加载全部字段。
     *
     * @param filePath 输入文件路径。
     * @param fallbackExtension 当路径不存在时尝试补充的后缀，默认值为 .qwd。
     * @return 读取到的 WrTrwnd 值。
     */
    bool ReadWrTrwndFromText(const std::string &filePath, const std::string &fallbackExtension = ".qwd")
    {
        BeginTextRead(filePath, fallbackExtension);
        ReadOrWrite("WrTrwnd", WrTrwnd);
        return WrTrwnd;
    }

    /**
     * @brief 将文本格式转换为 YAML 格式。
     * @param textPath 源文本文件路径。
     * @param yamlPath 目标 YAML 文件路径。
     */
    static void ConvertTextToYaml(const std::string &textPath, const std::string &yamlPath)
    {
        WindL_InputPar io;
        io.LoadText(textPath);
        io.SaveYaml(yamlPath);
    }

    /**
     * @brief 将 YAML 格式转换为文本格式。
     * @param yamlPath 源 YAML 文件路径。
     * @param textPath 目标文本文件路径。
     * @param templatePath 可选模板文件路径，用于保留原有布局。
     */
    static void ConvertYamlToText(const std::string &yamlPath,
                                  const std::string &textPath,
                                  const std::string &templatePath = std::string{})
    {
        WindL_InputPar io;
        io.LoadYaml(yamlPath);
        io.SaveText(textPath, templatePath);
    }

    /**
     * @brief 将文本格式转换为二进制格式。
     * @param textPath 源文本文件路径。
     * @param binaryPath 目标二进制文件路径。
     */
    static void ConvertTextToBinary(const std::string &textPath, const std::string &binaryPath)
    {
        WindL_InputPar io;
        io.LoadText(textPath);
        io.SaveBinary(binaryPath);
    }

    /**
     * @brief 将二进制格式转换为文本格式。
     * @param binaryPath 源二进制文件路径。
     * @param textPath 目标文本文件路径。
     * @param templatePath 可选模板文件路径，用于保留原有布局。
     */
    static void ConvertBinaryToText(const std::string &binaryPath,
                                    const std::string &textPath,
                                    const std::string &templatePath = std::string{})
    {
        WindL_InputPar io;
        io.LoadBinary(binaryPath);
        io.SaveText(textPath, templatePath);
    }

    /**
     * @brief 将 YAML 格式转换为二进制格式。
     * @param yamlPath 源 YAML 文件路径。
     * @param binaryPath 目标二进制文件路径。
     */
    static void ConvertYamlToBinary(const std::string &yamlPath, const std::string &binaryPath)
    {
        WindL_InputPar io;
        io.LoadYaml(yamlPath);
        io.SaveBinary(binaryPath);
    }

    /**
     * @brief 将二进制格式转换为 YAML 格式。
     * @param binaryPath 源二进制文件路径。
     * @param yamlPath 目标 YAML 文件路径。
     */
    static void ConvertBinaryToYaml(const std::string &binaryPath, const std::string &yamlPath)
    {
        WindL_InputPar io;
        io.LoadBinary(binaryPath);
        io.SaveYaml(yamlPath);
    }
};
