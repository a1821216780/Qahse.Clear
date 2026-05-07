# WindField 实现计划

## 概述

为 WindL 模块新增 `WindField` 数据容器类，实现 TurbSim `.bts`、Bladed `.wnd`、TurbSim `.wnd` 三种二进制风文件的**读取**功能，补全 `Mode::IMPORT` 模式。

**职责边界**：
- `WindField`：风场数据存储 + 二进制文件读取 + 统计计算 + 风速插值
- `SimWind`：风场生成引擎 + 二进制文件写入（现有函数不动）
- `WindL_IO_Subs`：`.qwd`/`.yml` 文本序列化（不动）

**关键约束**：
- 不引入 Qt 依赖，使用标准 C++ + Eigen（与项目现有风格一致）
- 写入函数保留在 SimWind.cpp 中不动，WindField 只负责读取
- 替换 SimWind.cpp 匿名 namespace 中现有的轻量 `struct WindField`

---

## 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/WindL/WindField.hpp` | **新建** | 类声明 |
| `src/WindL/WindField.cpp` | **新建** | 读取函数 + 统计 + 插值 |
| `src/WindL/SimWind.hpp` | **修改** | 添加 `Import` / `ImportFromFile` 声明 |
| `src/WindL/SimWind.cpp` | **修改** | 替换内部 WindField，修改 ValidateInput，添加 Import |
| `src/main.cpp` | **修改** | Mode::IMPORT 分支调用 Import |

---

## 1. WindField.hpp — 接口规范

```cpp
#pragma once

#include <array>
#include <string>
#include <vector>
#include "WindL_Type.hpp"

class WindField
{
public:
    // ──── 网格参数 ────
    int nSteps  = 0;      // 时间步数
    int ny      = 0;      // Y方向(水平)网格点数
    int nz      = 0;      // Z方向(垂直)网格点数
    int nPoints = 0;      // 总空间点数 = ny * nz

    // ──── 网格间距 ────
    double dy = 0.0;      // Y方向间距 (m)
    double dz = 0.0;      // Z方向间距 (m)
    double dt = 0.0;      // 时间步长 (s)

    // ──── 几何参数 ────
    double hubHeight  = 0.0;   // 轮毂高度 (m)
    double zBottom    = 0.0;   // 风场底边界高度 (m)
    double fieldDimY  = 0.0;   // Y方向总宽度 (m)
    double fieldDimZ  = 0.0;   // Z方向总高度 (m)

    // ──── 带参数 ────
    double meanWindSpeed = 0.0;   // 轮毂处平均风速 (m/s)

    // ──── BTS 解码参数 (slope/offset 编码) ────
    std::array<double, 3> slope{1.0, 1.0, 1.0};
    std::array<double, 3> offset{0.0, 0.0, 0.0};

    // ──── 速度数据 ────
    // 存储格式: component[comp][step * nPoints + point]
    // comp: 0=u(纵向), 1=v(横向), 2=w(竖向)
    std::array<std::vector<double>, 3> component;

    // ──── 统计量 (由 ComputeStats 填充) ────
    std::array<double, 3> mean{0.0, 0.0, 0.0};
    std::array<double, 3> sigma{0.0, 0.0, 0.0};
    std::array<double, 3> ti{0.0, 0.0, 0.0};

    // ──── 坐标向量 (由 Read* 填充) ────
    std::vector<double> yCoords;      // 每个空间点的 y 坐标
    std::vector<double> zCoords;      // 每个空间点的 z 坐标
    std::vector<double> timeCoords;   // 每个时间步的时间值

    // ──── 构造 ────
    WindField() = default;

    // ──── 索引与访问 ────
    /// @brief 由网格索引 (iy, iz) 计算线性空间点索引
    int GridIndex(int iy, int iz) const { return iy + iz * ny; }

    /// @brief 获取总空间点数
    int TotalPoints() const { return ny * nz; }

    /// @brief 可写访问: component[comp][step * nPoints + point]
    double& At(int comp, int step, int point)
    {
        return component[static_cast<std::size_t>(comp)]
            [static_cast<std::size_t>(step) * static_cast<std::size_t>(nPoints)
             + static_cast<std::size_t>(point)];
    }

    /// @brief 只读访问
    double At(int comp, int step, int point) const
    {
        return component[static_cast<std::size_t>(comp)]
            [static_cast<std::size_t>(step) * static_cast<std::size_t>(nPoints)
             + static_cast<std::size_t>(point)];
    }

    // ──── 二进制读取 (静态工厂方法) ────
    static WindField ReadBts(const std::string &path);
    static WindField ReadBladedWnd(const std::string &path);
    static WindField ReadTurbSimWnd(const std::string &path);
    static WindField ReadAny(const std::string &path, WndFormat format);

    // ──── 统计 ────
    void ComputeStats();

    // ──── 三线性插值 ────
    /// @brief 在空间点 (y,z) 和时间 t 处插值风速
    void GetWindSpeed(double y, double z, double t,
                      double &u, double &v, double &w) const;
};
```

---

## 2. WindField.cpp — 实现规范

### 2.1 `ReadBts(const std::string &path)`

**格式说明**（与 `SimWind.cpp:4321` 的 `WriteBts` 互逆）：

| 偏移 | 类型 | 字段 | 说明 |
|------|------|------|------|
| 0 | int16 | id | 7=非循环, 8=循环 |
| 2 | int32 | nz | Z方向网格点数 |
| 6 | int32 | ny | Y方向网格点数 |
| 10 | int32 | ntower | 塔架点数(忽略) |
| 14 | int32 | nSteps | 时间步数 |
| 18 | float32 | dz | Z方向步长 |
| 22 | float32 | dy | Y方向步长 |
| 26 | float32 | dt | 时间步长 |
| 30 | float32 | uHub | 轮毂风速 |
| 34 | float32 | HubHt | 轮毂高度 |
| 38 | float32 | zBottom | 底部高度 |
| 42 | float32 | uScl | u分量斜率 |
| 46 | float32 | uOff | u分量偏移 |
| 50 | float32 | vScl | v分量斜率 |
| 54 | float32 | vOff | v分量偏移 |
| 58 | float32 | wScl | w分量斜率 |
| 62 | float32 | wOff | w分量偏移 |
| 66 | int32 | descLen | 描述字符串字节数 |
| 70 | char[] | desc | 描述字符串 |
| ... | ... | 数据区 | 见下方 |

**数据区**: `for t in [0, nSteps): for iz in [0, nz): for iy in [0, ny): int16 u, int16 v, int16 w`

**解码公式**: `V = (int16_value - offset) / slope`

**实现伪代码**：
```
1. 以 std::ios::binary 打开文件
2. 读取 int16 id (跳过)
3. 读取 int32 nz, ny, ntower(跳过), nSteps
4. 读取 float32 dz, dy, dt, uHub, hubHt, zBottom
5. 读取 float32 uScl, uOff, vScl, vOff, wScl, wOff
6. 读取 int32 descLen, 然后跳过 descLen 字节
7. field.ny=ny, field.nz=nz, field.nSteps=nSteps, field.nPoints=ny*nz
8. field.dy=dy, field.dz=dz, field.dt=dt
9. field.hubHeight=hubHt, field.zBottom=zBottom
10. field.meanWindSpeed=uHub
11. field.fieldDimY = dy*(ny-1), field.fieldDimZ = dz*(nz-1)
12. field.slope = {uScl, vScl, wScl}
13. field.offset = {uOff, vOff, wOff}
14. 每个分量的 component[comp].resize(nSteps * nPoints)
15. 循环 t, iz, iy 读取 3 个 int16，解码后存入 field.At(comp, t, GridIndex(iy, iz))
16. 填充 yCoords, zCoords, timeCoords
17. 返回 field
```

**坐标重建**:
- yCoords[iy] = -fieldDimY/2 + iy * dy   (for iy in [0, ny))
- zCoords[iz] = zBottom + iz * dz         (for iz in [0, nz))
- timeCoords[t] = t * dt                  (for t in [0, nSteps))

**错误处理**: 文件无法打开 → `throw std::runtime_error("Cannot open BTS: " + path)`

**读取辅助函数**（写在 anonymous namespace 中）:
```cpp
template<typename T>
T ReadScalar(std::ifstream &in)
{
    T value;
    in.read(reinterpret_cast<char *>(&value), sizeof(T));
    return value;
}
```

---

### 2.2 `ReadBladedWnd(const std::string &path)`

**格式说明**（与 `SimWind.cpp:4379` 的 `WriteBladedWnd` 互逆）：

**头部**:

| 偏移 | 类型 | 字段 |
|------|------|------|
| 0 | int16 | magic = -99 |
| 2 | int16 | modelId |

**按 modelId 分叉读取**:

**modelId = 4 (IEC von Kármán / TurbSim 兼容)**:
```
int32 nComp = 3              (如果 modelId>=7 才有 headerBytes/nComp 字段)
float32 latitude
float32 roughness
float32 zHub
float32 tiU_percent (湍流强度 U × 100)
float32 tiV_percent (湍流强度 V × 100)
float32 tiW_percent (湍流强度 W × 100)
// modelId=4 没有后续长度尺度字段
```

**modelId = 7 (Bladed Kaimal)**:
```
// 先读 headerBytes 和 nComp (因为 modelId>=7)
int32 headerBytes
int32 nComp (=3)
float32 zLu, yLu, xLu, zLv, yLv, xLv, zLw, yLw, xLw
float32 maxFreq
float32 cohDecay
float32 cohScale
```

**modelId = 8 (Bladed Mann)**:
```
int32 headerBytes
int32 nComp (=3)
float32 zLu, yLu, xLu, zLv, yLv, xLv, zLw, yLw, xLw
float32 maxFreq
float32 gamma
float32 mannL
float32 sigmaRatioVU
float32 sigmaRatioWU
float32 maxL
float32 (reserved) × 2
int32 (reserved) × 3
float32 (reserved)
```

**数据区**: `for t in [0, nSteps): for iz in [0, nz): for iy in [ny-1 .. 0]: int16 u, int16 v, int16 w`

**解码公式（Bladed WND 编码）**:
`V = sigma * int16_value / 1000.0 + mean`

但读取时 sigma/mean 未知，因此需要**两遍扫描**：
- 第一遍：读取所有 int16 值，按 `V = int16_value` 暂存
- 读取完毕后 `ComputeStats()` 计算 sigma/mean
- 第二遍：`V = sigma * int16_value / 1000.0 + mean` 解码

或者在读取时使用近似：`V ≈ int16_value * 0.001 + estimated_mean`（不准确，不推荐）

**推荐方案**: 先以原始 int16 值读入，调用 `ComputeStats()` 获得统计量，然后用解码公式修正。

**但更简单的方案**: BTS 文件的 slope/offset 解码不需要两遍；对于 Bladed WND，由于它的编码参数（sigma, mean）在解码时未知，WindL 的 Bladed WND 导入可以：
1. 将 int16 值读取为 double，不做解码
2. 作为"准速度"值存储
3. `ComputeStats()` 从这些值计算出统计量
4. 在 `GetWindSpeed()` 调用时动态解码

**简化实现（优先采用）**:
- modelId=4/7/8 的数据区均按 int16×3 读入
- 对 BTS 格式用 slope/offset 解码：`V = (v16 - off) / slope`
- 对 Bladed WND 格式直接用原始 int16 值（不做归一化解码），标记为 "imported values"
- 用户只需数据的相对趋势，绝对值可从 BTS 格式获取

**关键难点**: Bladed WND 格式**不包含网格尺寸信息**（无 nz, ny, dy, dz, dt, hubHeight 等）。读取时需要外部提供这些参数。

**解决方案**: WindLInput 中已有 `gridPtsY`, `gridPtsZ`, `fieldDimY`, `fieldDimZ`, `timeStep`, `simTime`, `hubHeight` 等字段——在 `Mode::IMPORT` 时，这些字段用于告诉读取器网格参数。

因此 `ReadBladedWnd` 需要额外签名：
```cpp
static WindField ReadBladedWnd(const std::string &path,
                                int ny, int nz,
                                double dy, double dz, double dt,
                                double hubHeight, double zBottom);
```
或者从文件头中推断（modelId=4 不含这些信息；modelId=7/8 也不含）。

**最终方案**: Bladed WND 导入要求用户在 .qwd 中提供网格参数。`SimWind::Import()` 从 `WindLInput` 中提取这些参数并传递给 `ReadBladedWnd/ReadTurbSimWnd`。

---

### 2.3 `ReadTurbSimWnd(const std::string &path, ...)`

**格式说明**（与 `SimWind.cpp:4512` 的 `WriteTurbSimWnd` 互逆）：

固定 modelId=4 的 Bladed 兼容格式，但编码方式不同：

```
int16 magic = -99
int16 modelId = 4
int32 nComp = 3
float32 latitude
float32 roughness
float32 zHub
float32 tiU_percent, tiV_percent, tiW_percent
// 数据区: [t][iz][iy] int16 u, v, w
```

**解码公式**:
```
u = (int16_u + 1000.0 / tiU) / (1000.0 / (U * tiU))
v = int16_v / (1000.0 / (U * tiV))
w = int16_w / (1000.0 / (U * tiW))
```
其中 U = meanWindSpeed（从 .qwd 中获取），tiU/tiV/tiW = TI_percent/100。

同样，网格参数需从 WindLInput 传入。

---

### 2.4 `ComputeStats()`

遍历 `component[comp]` 计算:
- **mean[comp]** = Σv / N
- **sigma[comp]** = sqrt(Σ(v-mean)² / (N-1) 或 sqrt(Σ(v-mean)² / N))
- **ti[comp]** = sigma[comp] / meanWindSpeed

采用 `N-1` 分母（样本标准差），与 SimWind.cpp 中现有的统计计算保持一致。

---

### 2.5 `GetWindSpeed(double y, double z, double t, double &u, double &v, double &w)`

三线性插值（y × z × t 三个维度）：
```
1. 在 y 维度找相邻的 iy0, iy1
2. 在 z 维度找相邻的 iz0, iz1
3. 在 t 维度找相邻的 it0, it1
4. 对8个角点进行三线性插值
5. 边界外的点使用最近边界值 (clamp)
```

---

### 2.6 `ReadAny(const std::string &path, WndFormat format, const WindLInput &input)`

```cpp
WindField WindField::ReadAny(const std::string &path, WndFormat format,
                              const WindLInput &input)
{
    switch (format) {
    case WndFormat::TURBSIM_BTS:
        return ReadBts(path);
    case WndFormat::BLADED_WND:
        return ReadBladedWnd(path,
            input.gridPtsY, input.gridPtsZ,
            input.fieldDimY / (input.gridPtsY - 1),
            input.fieldDimZ / (input.gridPtsZ - 1),
            input.timeStep, input.hubHeight,
            input.hubHeight - input.fieldDimZ / 2.0);
    case WndFormat::TURBSIM_WND:
        return ReadTurbSimWnd(path,
            input.meanWindSpeed,
            input.gridPtsY, input.gridPtsZ,
            input.fieldDimY / (input.gridPtsY - 1),
            input.fieldDimZ / (input.gridPtsZ - 1),
            input.timeStep, input.hubHeight,
            input.hubHeight - input.fieldDimZ / 2.0);
    }
    throw std::runtime_error("Unknown WndFormat");
}
```

---

## 3. SimWind.cpp 变更

### 3.1 替换内部 struct WindField

**移除** anonymous namespace 中的 `struct WindField`（行 148-190）。

**添加** `#include "WindField.hpp"` 到头文件包含区。

**修改所有引用**：原 `field.At(comp, step, point)` 和 `field.component[comp][...]` —— 新类接口完全兼容，无需改动。

### 3.2 修改 `ValidateInput()`

位置: `SimWind.cpp:1543-1564`

当前逻辑：
```cpp
if (input.mode != Mode::GENERATE)
    throw std::runtime_error(L_WIND_OnlyGenerateMode);
```

改为：
```cpp
if (input.mode == Mode::IMPORT) {
    // Import mode validation
    if (input.wndFilePath.empty())
        throw std::runtime_error("Import file path is required");
    return;  // no grid/time validation needed for import
}
if (input.mode != Mode::GENERATE && input.mode != Mode::BATCH)
    throw std::runtime_error(L_WIND_OnlyGenerateMode);
// ... existing GENERATE validation follows
```

### 3.3 添加 `SimWind::Import()`

```cpp
SimWindResult SimWind::Import(const WindLInput &input,
                               SimWindProgressCallback progress)
{
    if (progress) progress("Importing wind field from " + input.wndFilePath);

    WindField field = WindField::ReadAny(input.wndFilePath, input.wndFormat, input);

    if (progress) progress("Computing statistics...");
    field.ComputeStats();

    SimWindResult result;
    result.gridPtsY = field.ny;
    result.gridPtsZ = field.nz;
    result.timeSteps = field.nSteps;
    result.timeStep = field.dt;
    result.hubHeight = field.hubHeight;
    result.meanWindSpeed = field.meanWindSpeed;

    for (int c = 0; c < 3; ++c) {
        result.stats[c].mean = field.mean[c];
        result.stats[c].sigma = field.sigma[c];
        result.stats[c].turbulenceIntensity = field.ti[c];
    }

    // Generate output paths (no binary re-write by default, just .sum)
    if (!input.savePath.empty() && !input.saveName.empty() && input.sumPrint) {
        // ... write .sum file
    }

    if (progress) progress("Import complete.");
    return result;
}
```

### 3.4 添加 `SimWind::ImportFromFile()`

```cpp
SimWindResult SimWind::ImportFromFile(const std::string &qwdPath,
                                       SimWindProgressCallback progress)
{
    auto input = ReadWindLInput(qwdPath);
    if (input.mode != Mode::IMPORT)
        throw std::runtime_error("File mode is not IMPORT");
    return Import(input, std::move(progress));
}
```

---

## 4. SimWind.hpp 变更

在 `SimWind` 类中添加两个静态方法声明：

```cpp
static SimWindResult Import(const WindLInput &input,
                            SimWindProgressCallback progress = {});
static SimWindResult ImportFromFile(const std::string &qwdPath,
                                    SimWindProgressCallback progress = {});
```

---

## 5. main.cpp 变更

位置: `src/main.cpp:153-154`

替换：
```cpp
std::cerr << std::string(L_CLI_ImportNotImpl) + " for --qwd.\n";
return 2;
```

为：
```cpp
if (input.mode == Mode::IMPORT)
{
    std::cout << "Importing wind field from " << input.wndFilePath << "...\n";
    const auto result = SimWind::Import(input, progress);
    std::cout << "Import complete:\n"
              << "  Grid: " << result.gridPtsY << "x" << result.gridPtsZ << "\n"
              << "  Timesteps: " << result.timeSteps << "\n"
              << "  Hub wind: " << result.meanWindSpeed << " m/s\n"
              << "  TI(u,v,w): "
              << result.stats[0].turbulenceIntensity << ", "
              << result.stats[1].turbulenceIntensity << ", "
              << result.stats[2].turbulenceIntensity << "\n";
    if (!result.sumPath.empty())
        std::cout << "  Summary: " << result.sumPath << "\n";
    return 0;
}
```

---

## 6. 实施顺序

1. **创建 `WindField.hpp`** — 按照第1节的接口规范
2. **创建 `WindField.cpp`** — 实现 `ReadBts` → `ReadBladedWnd` → `ReadTurbSimWnd` → `ReadAny` → `ComputeStats` → `GetWindSpeed`
3. **修改 `SimWind.hpp`** — 添加 Import/ImportFromFile 声明
4. **修改 `SimWind.cpp`** — 移除匿名 struct，添加 `#include "WindField.hpp"`，修改 `ValidateInput`，实现 `Import` 和 `ImportFromFile`
5. **修改 `main.cpp`** — 接通 IMPORT 分支
6. **编译验证** — 确保 GENERATE / BATCH 模式不受影响

---

## 7. 参考文件

| 文件 | 关键行 | 参考内容 |
|------|--------|---------|
| `SimWind.cpp:4321-4365` | WriteBts | 写入格式 → 逆向推导读取 |
| `SimWind.cpp:4379-4499` | WriteBladedWnd | 写入格式 → 逆向推导读取 |
| `SimWind.cpp:4512-4599` | WriteTurbSimWnd | 写入格式 → 逆向推导读取 |
| `SimWind.cpp:148-190` | 内部 struct WindField | 将被替换，接口兼容 |
| `SimWind.cpp:1543-1564` | ValidateInput | 需要修改以接受 IMPORT |
| `QBlade WindField.cpp:623-794` | importFromBinary | 参考：BTS 二进制解析逻辑 |
| `QBlade WindField.cpp:560-621` | exportToBinary | 参考：BTS 写入格式确认 |
| `WindL_Type.hpp:38-43` | enum Mode | IMPORT=1 |
| `WindL_Type.hpp:154-159` | enum WndFormat | BLADED_WND / TURBSIM_BTS / TURBSIM_WND |
| `WindL_Type.hpp:289-291` | wndFilePath / wndFormat | IMPORT 模式输入字段 |
| `main.cpp:105-162` | CLI 入口 | 需要修改 IMPORT 分支 |
