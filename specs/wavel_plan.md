# WaveL 波浪模块实现计划

## 概述

WaveL 模块负责**随机波浪谱生成、波浪运动学计算**，是 Qahse 平台的海洋水动力学核心组件。本模块对标 OpenFAST 的 SeaState 模块和 QBlade 的 Waves 模块，提供从波浪谱到时空运动学的完整计算管线。

### 职责边界

| 组件 | 职责 |
|------|------|
| **WaveL**（本模块） | 波浪谱生成、波浪运动学（波面高程、速度、加速度、动水压力）、波浪文件读取/写入 |
| **未来 HydroL** | 水动力载荷计算（Morison 方程、势流理论、衍射/辐射） |
| **主程序** | CLI 集成、模式派发 |

### 技术约束

- **语言**：C++20，MSVC v145（Visual Studio 2022）
- **线性代数**：Eigen 3.4.1（`Eigen/Dense`、`Eigen/FFT`）
- **FFT**：优先使用 Eigen 内置 FFT（`Eigen::FFT<double>`），备选 FFTW3
- **文件 I/O**：沿用现有 `IO/` 模块（`ZFile`、`ZPath`、`Serializer`）
- **无 Qt**：模块核心无 GUI 依赖
- **国际化**：使用现有 `TL()` 宏定义双语字符串
- **测试**：Google Test，沿用 `Test/WindL/` 模式

### 参考代码

- **QBlade 2.0.4**：`F:\Qahse\qblade_2.0.4_source\src\Waves\LinearWave.cpp`（1222 行，波浪谱 + 运动学 + 渲染）
- **OpenFAST SeaState**：`modules/seastate/src/Waves.f90`（148KB，1 阶运动学 + 拉伸），`Waves2.f90`（126KB，2 阶 QTF），`SeaSt_WaveField.f90`（44KB，4D 时空插值）

---

## 文件清单

| 文件 | 操作 | 描述 |
|------|------|------|
| `src/WaveL/WaveL_Type.hpp` | **新建** | 波浪相关枚举、WaveLInput 输入结构体、WaveSpectrumResult 结果结构体 |
| `src/WaveL/WaveSpectrum.hpp` | **新建** | WaveSpectrum 类声明 + WaveTrain 结构体 |
| `src/WaveL/WaveSpectrum.cpp` | **新建** | 波浪谱生成、离散化、运动学计算 |
| `src/WaveL/IO/WaveL_IO_Subs.hpp` | **新建** | 波浪输入文件解析（`.qwd` 扩展）和序列化 |
| `src/WaveL/IO/LocaleString_WaveL.hpp` | **新建** | WaveL 专用双语字符串宏 |
| `src/WaveL/WaveKinematics.hpp` | **新建** | WaveKinematics 类声明（时空采样） |
| `src/WaveL/WaveKinematics.cpp` | **新建** | 波浪运动学时空插值 + IFFT 重建 |
| `src/IO/LocaleString.hpp` | **修改** | 新增 `L_CLI_WaveL_*` CLI 字符串常量 |
| `src/main.cpp` | **修改** | 新增 `--wave` CLI 模式和派发逻辑 |
| `src/Params.h` | **修改** | 确认/扩展水动力学常量 |
| `Test/WaveL/WaveSpectrumTests.cpp` | **新建** | 波浪谱生成测试 |
| `Test/WaveL/WaveKinematicsTests.cpp` | **新建** | 波浪运动学测试 |
| `Test/WaveL/data/` | **新建** | 测试用波浪输入文件 |
| `vsstudio/Qahse.vcxproj` | **修改** | 添加新源文件 |
| `vsstudio/UnitTests.vcxproj` | **修改** | 添加新测试文件 |

---

## 1. 类型与数据结构

### 1.1 波浪谱枚举（`WaveL_Type.hpp`）

```cpp
/// 频率谱类型
enum class WaveFreqSpectrum
{
    NONE = 0,           ///< 静水（无波浪）
    REGULAR = 1,        ///< 规则波（单色波）—— 冲击谱 Tp/Hs
    JONSWAP = 2,        ///< JONSWAP 谱（含 ISSC/Pierson-Moskowitz γ=1 特例）
    TORSETHAUGEN = 3,   ///< Torsethaugen 双峰谱（风浪 + 涌浪分解）
    OCHI_HUBBLE = 4,    ///< Ochi-Hubble 六参数双峰谱
    USER_SPECTRUM = 5,  ///< 用户自定义频谱文件导入
    USER_TIMESERIES = 6 ///< 用户水面时程文件导入
};

/// 方向分布类型
enum class WaveDirSpectrum
{
    UNIDIRECTIONAL = 0, ///< 单向
    COSINE_SPREAD = 1   ///< Cos^(2s) 方向散布
};

/// 频率离散化方法
enum class WaveDiscretization
{
    EQUAL_ENERGY = 0,   ///< 等能量分 bin（自适应带宽约束）
    EQUAL_FREQUENCY = 1 ///< 等频率步长
};

/// 波浪运动学拉伸方法
enum class WaveStretching
{
    NONE = 0,           ///< 不拉伸（z>0 运动学归零）
    VERTICAL = 1,       ///< 垂直拉伸（z>0 处使用 z=0 运动学）
    EXTRAPOLATION = 2,  ///< 线性外推（一阶泰勒展开）[推荐]
    WHEELER = 3         ///< Wheeler 拉伸 z' = d(z+d)/(η+d) - d
};

/// 波浪生成模式
enum class WaveMode
{
    GENERATE = 0,       ///< 生成波浪谱及运动学文件
    IMPORT = 1,         ///< 导入已有波浪结果
    BATCH = 2           ///< 批量波浪生成
};
```

### 1.2 波浪输入结构体（`WaveLInput`）

```cpp
struct WaveLInput
{
    // ── 基础模式 ──
    WaveMode mode = WaveMode::GENERATE;   ///< 生成/导入/批量

    // ── 谱参数 ──
    WaveFreqSpectrum freqSpectrum = WaveFreqSpectrum::JONSWAP; ///< 频率谱类型
    WaveDirSpectrum dirSpectrum = WaveDirSpectrum::UNIDIRECTIONAL; ///< 方向谱类型
    WaveDiscretization discretization = WaveDiscretization::EQUAL_ENERGY; ///< 离散化方法
    WaveStretching stretching = WaveStretching::EXTRAPOLATION; ///< 拉伸方法

    double Hs = 3.0;           ///< 有效波高 [m]
    double Tp = 10.0;          ///< 谱峰周期 [s]
    double dirMean = 0.0;      ///< 主波向 [deg]
    double dirMax = 30.0;      ///< 方向散布全宽 [deg]
    double dirSpreadExp = 2.0; ///< 方向散布指数 s

    // ── 频率域离散化 ──
    double fCutIn = 0.0;       ///< 截止频率下限 [Hz]（0=自动）
    double fCutOut = 0.0;      ///< 截止频率上限 [Hz]（0=自动）
    double dfMax = 0.0;        ///< 最大频率 bin 宽度 [Hz]（0=自动）
    int numFreqBins = 200;     ///< 频率 bin 数量
    int numDirBins = 24;       ///< 方向 bin 数量
    int randomSeed = 0;        ///< 随机种子（0=自动基于时间）

    // ── JONSWAP / Torsethaugen 参数 ──
    double gamma = 0.0;        ///< JONSWAP 峰增强因子（0=自动 DNV 参数化）
    double sigma1 = 0.0;       ///< 谱宽 σ₁（f≤fp）（0=自动 0.07）
    double sigma2 = 0.0;       ///< 谱宽 σ₂（f>fp）（0=自动 0.09）
    bool autoGamma = true;     ///< 自动计算 γ
    bool autoSigma = true;     ///< 自动计算 σ₁/σ₂
    bool autoFreqRange = true; ///< 自动计算频率截断范围

    // ── Ochi-Hubble 六参数 ──
    double Hs1 = 2.0;          ///< 低频系统有效波高 [m]
    double Hs2 = 1.5;          ///< 高频系统有效波高 [m]
    double f1 = 0.1;           ///< 低频系统峰频 [Hz]
    double f2 = 0.2;           ///< 高频系统峰频 [Hz]
    double lambda1 = 2.0;      ///< 低频系统形状参数 λ₁
    double lambda2 = 2.0;      ///< 高频系统形状参数 λ₂
    bool autoOchi = false;     ///< 根据 Hs/Tp 自动推导六参数

    // ── Torsethaugen 双峰 ──
    bool doublePeak = false;   ///< 是否启用双峰（Torsethaugen）

    // ── 规则波 ──
    double regularPhase = 0.0; ///< 规则波初始相位 [rad]

    // ── 时域参数 ──
    double timeStep = 0.1;     ///< 时间步长 [s]
    double simDuration = 600.0;///< 仿真总时长 [s]

    // ── 水深 ──
    double waterDepth = 100.0; ///< 水深 [m]（>100 为深水近似）

    // ── MacCamy-Fuchs 柱体衍射 ──
    double mcfDiameter = 0.0;  ///< 柱体直径 [m]（0=不启用MFC）

    // ── 输出控制 ──
    bool outputComponents = false;   ///< 输出波浪成分文件
    bool outputTimeSeries = false;   ///< 输出水面时程文件
    bool outputKinematicsGrid = false; ///< 输出空间运动学场文件
    std::string componentsPath;     ///< 波浪成分输出路径
    std::string timeSeriesPath;     ///< 水面时程输出路径
    std::string kinematicsPath;     ///< 空间运动学场输出路径
    std::string summaryPath;        ///< 摘要文件路径

    // ── 空间场参数 ──
    int gridNX = 1;            ///< X 向空间点数
    int gridNY = 1;            ///< Y 向空间点数
    int gridNZ = 10;           ///< Z 向空间点数
    double gridDX = 0.0;       ///< X 向间距 [m]
    double gridDY = 0.0;       ///< Y 向间距 [m]
    double gridDZ = 0.0;       ///< Z 向间距 [m]

    // ── 导入模式 ──
    std::string importedSpectrumPath; ///< 导入频谱文件路径
    std::string importedTimeSeriesPath; ///< 导入时程文件路径
    std::string importedComponentsPath; ///< 导入波浪成分文件路径
};
```

### 1.3 波浪成分结构体（`WaveTrain`）

```cpp
/// 单个波浪成分（对应线性 Airy 波的一个频率-方向分量）
struct WaveTrain
{
    double amplitude = 0.0;   ///< 波幅 a [m]
    double phase = 0.0;       ///< 随机相位 φ [rad]
    double omega = 0.0;       ///< 角频率 ω = 2πf [rad/s]
    double wavenumber = 0.0;  ///< 波数 k [rad/m]
    double direction = 0.0;   ///< 传播方向 θ [rad]
    double cosDir = 0.0;      ///< cos(θ) 预存
    double sinDir = 0.0;      ///< sin(θ) 预存
    double A_omega = 0.0;     ///< a·ω 预存（速度计算用）
    double A_omega2 = 0.0;    ///< a·ω² 预存（加速度计算用）
};
```

### 1.4 结果结构体（`WaveSpectrumResult`）

```cpp
struct WaveSpectrumResult
{
    // ── 波浪成分 ──
    std::vector<WaveTrain> waveTrains;  ///< 生成的全部波浪成分

    // ── 统计 ──
    double significantHeight = 0.0;     ///< Hs [m]
    double peakPeriod = 0.0;            ///< Tp [s]
    double peakFrequency = 0.0;         ///< fp [Hz]
    double zeroMoment = 0.0;            ///< m0 = Hs²/16
    int numComponents = 0;              ///< 总成分数
    double fMin = 0.0;                  ///< 实际最小频率 [Hz]
    double fMax = 0.0;                  ///< 实际最大频率 [Hz]
    double spectralArea = 0.0;          ///< ∫S(f)df（归一化前）

    // ── 输出路径 ──
    std::string componentsFilePath;     ///< 波浪成分文件路径
    std::string timeSeriesFilePath;     ///< 水面时程文件路径
    std::string summaryFilePath;        ///< 摘要文件路径

    // ── 警告 ──
    std::vector<std::string> warnings;  ///< 警告信息
};
```

---

## 2. 波浪谱数学公式

### 2.1 JONSWAP 谱 — `S_JONSWAP(f)`

**DNV-ST-0437 / IEC 61400-3 标准 JONSWAP 参数化**

```
给定:
  Hs    — 有效波高 [m]
  Tp    — 谱峰周期 [s]
  fp = 1/Tp  — 谱峰频率 [Hz]

自动 γ（DNV-RP-C205）:
  TpOvrSqrtHs = Tp / √(Hs)
  IF   TpOvrSqrtHs ≤ 3.6:  γ = 5.0
  ELSE IF TpOvrSqrtHs > 3.6 AND ≤ 5.0:
      γ = exp(5.75 - 1.15 * TpOvrSqrtHs)
  ELSE:  γ = 1.0

自动 σ:
  IF f ≤ fp:  σ = 0.07
  ELSE:        σ = 0.09

频率比:  r = f / fp

峰形因子:  α = exp( -0.5 * ((r - 1) / σ)² )

Pierson-Moskowitz 基谱:
  S_PM(f) = 0.3125 * Hs² * Tp * r⁻⁵ * exp( -1.25 * r⁻⁴ )

归一化因子:  C = 1.0 - 0.287 * ln(γ)

谱密度:
  S_JONSWAP(f) = C * S_PM(f) * γ^α
```

> **Pierson-Moskowitz 特例**：令 γ = 1.0，则 C = 1.0，α = 0（γ^α = 1），退化为 ISSC/PM 谱。

**参考实现**：`LinearWave.cpp:593-617`（QBlade），`Waves.f90:JONSWAP()`（OpenFAST）

### 2.2 Torsethaugen 双峰谱 — `S_TORSETHAUGEN(f)`

基于 Torsethaugen & Haver (2004)，SINTEF 报告。将海况分解为风浪系统（1）和涌浪系统（2）。

```
常量:
  a_f  = 6.6      (fetch parameter)
  a_e  = 2.0
  a_u  = 25.0
  a_10 = 0.7
  a_1  = 0.5
  k_g  = 35.0
  b_1  = 2.0
  a_20 = 0.6
  a_2  = 0.3
  a_3  = 6.0
  G_0  = 3.26     (归一化常数)

Step 1 — 波龄参数:
  Tpf = a_f * Hs^(1/3)      [风浪谱峰周期]
  Tl  = a_e * Hs^(0.5)       [下过渡周期]
  Tu  = a_u                  [上过渡周期 = 25s]
  e_l = (Tpf - Tp) / (Tpf - Tl)   (clamped: 若 Tl < Tpf → min(1, max(0, e_l)))
  e_u = (Tp - Tpf) / (Tu - Tpf)   (clamped: 若 Tu > Tpf → min(1, max(0, e_u)))

Step 2 — 能量分解:

  若 Tp ≤ Tpf（涌浪主导）:
    R  = (1 - a_10) * exp(-(e_l/a_1)²) + a_10
    Hs1 = R * Hs              → 风浪部分
    Tp1 = Tp
    Γ₁  = k_g * (2π·Hs1/(g·Tp1²))^(6/7)
    Hs2 = √(1 - R²) * Hs      → 涌浪部分
    Tp2 = Tpf + b_1

  若 Tp > Tpf（风浪主导）:
    R  = (1 - a_20) * exp(-(e_u/a_2)²) + a_20
    Hs1 = R * Hs
    Tp1 = Tp
    Γ₁  = k_g * (2π·Hs/(g·Tpf²))^(6/7) * (1 + a_3·e_u)
    Hs2 = √(1 - R²) * Hs
    Tp2 = a_f * Hs2^(1/3)

Step 3 — 形状参数:
  Ay = (1 + 1.1 * (ln(Γ₁))^1.19) / Γ₁

Step 4 — 分量谱（使用 JONSWAP 函数形式）:
  f1n = f * Tp1
  f2n = f * Tp2

  S₁(f) = G₀ * Ay * (f1n)⁻⁴ * exp(-(f1n)⁻⁴) * Γ₁^{ exp(-(f1n-1)² / (2σ²)) }
  S₂(f) = G₀ * (f2n)⁻⁴ * exp(-(f2n)⁻⁴)

Step 5 — 总谱:
  若 doublePeak:  S(f) = S₁(f) + S₂(f)
  否则:            S(f) = S₁(f)
```

**参考实现**：`LinearWave.cpp:645-716`

### 2.3 Ochi-Hubble 六参数双峰谱 — `S_OCHI_HUBBLE(ω)`

Ochi & Hubble (1976) 六参数谱，两个 Gamma 分布分量叠加。

```
参数:
  Hs1, f1, λ1  — 低频系统（涌浪）：有效波高、峰频、形状因子
  Hs2, f2, λ2  — 高频系统（风浪）：有效波高、峰频、形状因子

角频率:  ω₁ = 2π·f₁,  ω₂ = 2π·f₂,  ω = 2π·f

公式:
  S₁(ω) = (1/4) · [((4λ₁+1)/4)·ω₁⁴]^λ₁ / Γ(λ₁) · Hs₁² / ω^(4λ₁+1)
         · exp( -(4λ₁+1)/4 · (ω₁/ω)⁴ )

  S₂(ω) = (1/4) · [((4λ₂+1)/4)·ω₂⁴]^λ₂ / Γ(λ₂) · Hs₂² / ω^(4λ₂+1)
         · exp( -(4λ₂+1)/4 · (ω₂/ω)⁴ )

  S(ω) = S₁(ω) + S₂(ω)

其中 Γ(·) 为 Euler Gamma 函数 (std::tgamma)。
```

**参考实现**：`LinearWave.cpp:632-643`

### 2.4 规则波（单色波）— `S_REGULAR`

```
S(f) = 0.5 * (Hs/2)² / ΔΩ     (冲击函数，仅作用于 fp = 1/Tp 处)
所有其他频率: S(f) = 0
波幅: a = Hs / 2
```

**参考实现**：`Waves.f90:RegularWave_Init()`

### 2.5 用户自定义频谱 — `S_USER(f)`

从两列文件（频率 [Hz]，谱密度 [m²/Hz]）解析数据点，线性插值求值。

```
若 f < f_min 或 f > f_max: 返回 0
在区间 [f_i, f_{i+1}] 中:
  S(f) = S_i + (S_{i+1} - S_i) * (f - f_i) / (f_{i+1} - f_i)
```

**参考实现**：`LinearWave.cpp:619-630`

### 2.6 方向散布函数 — `S_DIRECTIONAL(θ)`

Cos^(2s) 散布函数，用于 3D 方向谱：

```
归一化常数:  C = √π · Γ(s+1) / (2 · Δθ_max · Γ(s+0.5))

散布函数:    D(θ) = C · |cos( π·(θ - θ_mean) / (2·Δθ_max) )|^(2s)

其中:
  θ_mean   — 主波方向 [rad]
  Δθ_max   — 最大散布半宽 [rad]
  s        — 散布指数（s 越大，方向越集中）
  Γ(·)     — Euler Gamma 函数
```

**参考实现**：`LinearWave.cpp:718-724`；等效于 OpenFAST `Waves.f90:CalculateWaveSpreading()`

---

## 3. 频率离散化算法

### 3.1 自动频率范围

```
若 autoFreqRange = true:
  fCutIn  = 0.5 / Tp       [低频截断]
  fCutOut = 10.0 / Tp      [高频截断]
对 Ochi-Hubble: fCutIn = 0.5*f1, fCutOut = 10.0*f2
```

### 3.2 等能量分 bin（`EQUAL_ENERGY`）

这是默认的高精度离散化方法。每个 bin 包含等量的频谱能量，同时受 `dfMax` 带宽约束。

```
算法:

1. 精细积分:
   N_fine = 100000
   Δf_fine = (fCutOut - fCutIn) / N_fine
   For i = 0..N_fine:
     f_i = fCutIn + Δf_fine * i
     dS_i = S(f_i) * Δf_fine            ← 离散能量增量
     area += dS_i
     intS_i = area                       ← 累积能量

2. 归一化:
   σ² = Hs² / 16                         ← 零阶谱矩（目标总能量）
   normFactor = σ² / area                 ← 缩放因子
   E_bin = area / numFreqBins            ← 每 bin 目标能量

3. 均分累积能量:
   energy = E_bin / 2                     ← 从半 bin 能量开始
   f_range_prev = fCutIn

   For i = 0..numFreqBins-1:
     a. 在 intS 数组中插值找到 f_range_curr，使得 intS(f_range_curr) = energy
     b. bandwidth = f_range_curr - f_range_prev

     c. IF bandwidth > dfMax AND dfMax > 0:
          递推细分此 bin 为 N_sub 个子 bin（N_sub = ceil(bandwidth / dfMax)）
          每个子 bin:
            - 产生独立随机相位
            - 子 bin 中心频率 = 插值处频率
            - 子 bin 波幅 = √(2 * E_bin * normFactor / N_sub)

     d. ELSE:
          相位 = random(0, 2π)
          频率 = f_range_curr
          波幅 = √(2 * E_bin * normFactor)

     e. energy += E_bin
        f_range_prev = f_range_curr

4. 组装 WaveTrain:
   对每个 (频率, 波幅, 相位):
     ω = 2π * f
     a = 波幅
     train.omega = ω
     train.amplitude = a
     train.phase = 相位
     train.direction = dir_mean (暂定, 之后由方向离散化覆盖)
     train.A_omega = a * ω
     train.A_omega2 = a * ω²
```

**参考实现**：`LinearWave.cpp:727-903`

### 3.3 等频率步长（`EQUAL_FREQUENCY`）

简单均匀步长替代方法。

```
Δf = (fCutOut - fCutIn) / numFreqBins

For i = 0..numFreqBins-1:
  f_c = fCutIn + Δf·(i + 0.5)                            ← bin 中心频率
  E_bin = ∫_{f_c-Δf/2}^{f_c+Δf/2} S(f) df                ← bin 内能量
  相位 = random(0, 2π)
  波幅 = √(2 * E_bin * normFactor)
```

---

## 4. 方向离散化算法

### 4.1 单向（`UNIDIRECTIONAL`）

```
所有 WaveTrain 的 direction = dirMean [rad]
```

### 4.2 Cos^(2s) 散布（`COSINE_SPREAD`）

```
若 numDirBins ≤ 1 OR dirMax == 0:
  退化为单向

算法:

1. 精细方向网格积分:
   N_dir_fine = 100000
   Δθ_fine = 2 * dirMax / N_dir_fine

   For i = 0..N_dir_fine:
     θ_i = dirMean - dirMax + Δθ_fine * i
     dD_i = D(θ_i) * Δθ_fine
     intDir_i = 累积和 of dD_i

2. 归一化验证:
   确认 intDir 的终值 ≈ 1.0 (容差 1%)

3. 等能量方向分 bin:
   For i = 0..numDirBins-1:
     目标累积能量 = (i + 0.5) / numDirBins
     在 intDir 中插值得到 θ_i

   得到方向数组: {θ_0, θ_1, ..., θ_{numDirBins-1}}

4. 随机打乱方向数组:
   std::shuffle(directionArray, RNG)

5. 循环分配方向给 WaveTrain:
   For i = 0..waveTrains.size()-1:
     waveTrains[i].direction = directionArray[i % numDirBins]
     waveTrains[i].cosDir = cos(direction)
     waveTrains[i].sinDir = sin(direction)

6. 按 ω 重排序 waveTrains
```

**参考实现**：`LinearWave.cpp:905-959`；OpenFAST `Waves.f90:CalculateWaveDirection()`

---

## 5. 色散关系 — 波数计算

### 5.1 无流色散关系（Guo 2002 显式近似）

J. Guo, *Coastal Engineering* 45, pp. 71-74, 2002 的显式近似公式：

```
k(ω) = (ω²/g) · [ 1 - exp( -(ω·√(d/g))^(5/2) ) ]^(-2/5)

其中:
  ω — 角频率 [rad/s]
  g — 重力加速度 9.80665 [m/s²]
  d — 水深 [m]
```

**极限行为**:
- 深水（kd → ∞）：`k ≈ ω²/g`（指数项 → 0，方括号 → 1）
- 浅水（kd → 0）：`k ≈ ω/√(gd)`（双曲近似退化）

OpenMP 并行化：每个 WaveTrain 独立计算 k。

**参考实现**：`LinearWave.cpp:579-591`

### 5.2 精确色散关系（Newman 方法）

备选方案 — 使用 OpenFAST 的 J.N. Newman 二次 Newton 算法（约 1 次迭代到 7 位精度）。

```
定义:  C = ω² * d / g

初始猜测 X₀ = k·d:
  若 C ≤ 2.0:
    X₀ = √C * [1.0 + C·(0.169 + 0.031·C)]
  否则:
    E2 = exp(-2·C)
    X₀ = C * [1.0 + E2·(2.0 - 12.0·E2)]

修正:
  若 C ≤ 4.8:
    k = [X₀ - B·(C² - X₀²)·(1 + A·B·C·X₀)] / d
    其中 A = 1/(C - C² + X₀²)
        B = A * [0.5·ln((X₀+C)/(X₀-C)) - X₀]
  否则:
    k = X₀ / d
```

**参考实现**：OpenFAST `Waves.f90:WaveNumber()`

> **建议**: 首选 Guo 2002 显式公式（实现简单、精度足够、适合大规模并行），Newman 方法作为精确校验/调试选项。

### 5.3 有流色散关系（未来扩展）

考虑 Doppler 频移的 Newton-Raphson 迭代：

```
Ω = Ωᵢ + k·V_current    (绝对频率 = 固有频率 + Doppler 频移)

迭代求解 Ωᵢ:
  F(Ωᵢ) = Ω - Ωᵢ - k·V_current
  ∂F/∂Ωᵢ = -V_current / Vg - 1.0

  其中 Vg = ½(Ωᵢ/k)·(1 + 2kd/sinh(2kd))   (群速度)
```

**参考实现**：OpenFAST `Waves.f90:WaveDispRel()`

---

## 6. 线性 Airy 波浪运动学

### 6.1 水面高程 — `GetElevation(x, y, t)`

各成分线性叠加：

```
η(x, y, t) = Σᵢ aᵢ · sin( kᵢ·Xᵢ - ωᵢ·(t + t_offset) + φᵢ )

其中:  Xᵢ = x·cos(θᵢ) + y·sin(θᵢ)        ← 传播方向投影
```

**参考实现**：`LinearWave.cpp:165-179`

### 6.2 速度与加速度 — `GetVelocityAndAcceleration(x, y, z, t, depth)`

#### 6.2a 深水（depth > 100m）

```
深度衰减函数:  P(z) = e^(k·z)          (z ≤ 0, z=0 为静水面)

拉伸修正 (z > 0, 外推模式):
  P(z) = 1 + k·z                       (一阶 Taylor 展开)

水平速度:
  u_x = Σ aᵢ·ωᵢ · cos(θᵢ) · P(z) · sin( kᵢ·Xᵢ - ωᵢ·(t+t_offset) + φᵢ )
  u_y = Σ aᵢ·ωᵢ · sin(θᵢ) · P(z) · sin( kᵢ·Xᵢ - ωᵢ·(t+t_offset) + φᵢ )

垂向速度:
  u_z = Σ -aᵢ·ωᵢ · P(z) · cos( kᵢ·Xᵢ - ωᵢ·(t+t_offset) + φᵢ )

水平加速度:
  a_x = Σ -aᵢ·ωᵢ² · cos(θᵢ) · P(z) · cos( kᵢ·Xᵢ - ωᵢ·(t+t_offset) + φᵢ )
  a_y = Σ -aᵢ·ωᵢ² · sin(θᵢ) · P(z) · cos( kᵢ·Xᵢ - ωᵢ·(t+t_offset) + φᵢ )

垂向加速度:
  a_z = Σ -aᵢ·ωᵢ² · P(z) · sin( kᵢ·Xᵢ - ωᵢ·(t+t_offset) + φᵢ )
```

**参考实现**：`LinearWave.cpp:450-512`（深水分支）

#### 6.2b 有限水深（depth ≤ 100m）

```
S = sinh(k·d)

水平衰减:  P_XY(z) = cosh( k·(z+d) ) / S
垂向衰减:  P_Z(z)  = sinh( k·(z+d) ) / S

速度公式同上，将 P(z) 替换为对应的 P_XY 或 P_Z

拉伸修正 (z > 0, 外推模式):
  dP_XY/dz|z=0 = k
  P_XY(z) = cosh(k·d)/S + z·k

  dP_Z/dz|z=0 = k·cosh(k·d)/S
  P_Z(z) = 1 + z·k·cosh(k·d)/S
```

**参考实现**：`LinearWave.cpp:373-448`（浅水分支）；OpenFAST `Waves.f90:COSHNumOvrCOSHDen/COSHNumOvrSINHDen/SINHNumOvrSINHDen`

### 6.3 动水压力

```
深水:
  p_dyn = ρ·g · Σ aᵢ · e^(kᵢ·z) · sin( kᵢ·Xᵢ - ωᵢ·(t+t_offset) + φᵢ )

有限水深:
  p_dyn = ρ·g · Σ aᵢ · [cosh(kᵢ·(z+d)) / cosh(kᵢ·d)] · sin( kᵢ·Xᵢ - ωᵢ·(t+t_offset) + φᵢ )

其中 ρ = 1025 kg/m³（海水密度），g = 9.80665 m/s²
```

**参考实现**：`LinearWave.cpp:434-437`（深水）、`422-424`（浅水）

### 6.4 MacCamy-Fuchs 柱体衍射修正

当 `mcfDiameter > 0` 时，对圆柱体周围的衍射效应进行经验修正。

```
加速度缩减系数:
  β = min( 1.05 · tanh(d·k) / [ (|D·k/2 - 0.2|)^2.2 + 1 ]^0.85 ,  1.0 )

相位偏移:
  基于 x = 1/(D·k/2π) 在 95 点查找表中线性插值（数据源自 USFOS 理论手册）

应用:
  加速度 *= β
  相位 += Δφ_MFC
```

**参考实现**：`LinearWave.cpp:430-441, 484-497`

---

## 7. 波浪运动学空间场 — IFFT 重建

### 7.1 设计决策

WaveL 提供**两种运动学计算模式**：

| 模式 | 描述 | 适用场景 |
|------|------|---------|
| **实时叠加** | 每次调用 `GetKinematics()` 时在线求和所有 WaveTrain | 波浪成分少（<500）、实时耦合仿真 |
| **IFFT 预计算场** | 预先通过 IFFT 生成 4D 时空场，运行时插值 | 波浪成分多、固定空间网格、批量后处理 |

WaveL **第一阶段优先实现实时叠加模式**。IFFT 预计算场在第二阶段实现。

### 7.2 实时叠加模式

```cpp
// 计算指定位置和时刻的运动学
struct WaveKinematics
{
    double elevation = 0.0;           // η [m]
    std::array<double, 3> velocity{}; // (u_x, u_y, u_z) [m/s]
    std::array<double, 3> acceleration{}; // (a_x, a_y, a_z) [m/s²]
    double dynamicPressure = 0.0;     // p_dyn [Pa]
};

WaveKinematics GetKinematics(double x, double y, double z, double t) const;
```

### 7.3 IFFT 预计算场模式（第二阶段）

参照 OpenFAST 的 `SeaSt_WaveField` 架构：

```
初始化:
  1. 确定 FFT 参数:
     NStepWave = 2 * round(ceil(simDuration/timeStep) / 2)
     NStepWave2 = NStepWave / 2
     Ω_array[k] = k * 2π / (NStepWave * timeStep)

  2. 在频率域构造复数运动学系数:
     For 每个空间点 (xi, yj, zk):
       For 每个频率 k:
         WaveElevC0_ijk[k] = C_k * exp(-i * k_k * (xi*cosθ_k + yj*sinθ_k))
         WaveVelC0_ijk[k]  = C_k * [深/浅水分量] * exp(-i*k_k * ...)
         ...

  3. 通过 IFFT 转换到时域:
     WaveElev1[t, i, j]  = IFFT[ WaveElevC0_ij ]
     WaveVel[t, i, j, k] = IFFT[ WaveVelC0_ijk ]
     WaveAcc[t, i, j, k] = IFFT[ WaveAccC0_ijk ]

  4. 拉伸映射:
     根据 WaveStretching 模式映射 z 坐标
     Wheeler: z' = d * (d+z) / (d+η) - d

运行时:
  插值 WaveElev1, WaveVel, WaveAcc, WaveDynP 在 (t+t_shift, x, y, z) 处
```

**参考实现**：OpenFAST `Waves.f90:VariousWaves_Init()` 和 `SeaSt_WaveField.f90:WaveField_GetNodeWaveKin()`

---

## 8. 波浪拉伸方法

| 模式 | 公式（z > η 时） | 描述 |
|------|------------------|------|
| **NONE (0)** | 运动学 = 0 | z > 0 以上无波浪运动学 |
| **VERTICAL (1)** | 使用 z = 0 处的运动学 | 简单垂直延伸 |
| **EXTRAPOLATION (2)** | F(z) = F(0) + dF/dz\|₀ · z | 一阶 Taylor 外推（推荐） |
| **WHEELER (3)** | z' = d·(d+z)/(d+η) - d | 有效压缩水柱（映射到 [-d,0]） |

**拉伸应用流程**:
```
给定计算点 (x, y, z, t):
  1. 计算自由面 η = GetElevation(x, y, t)
  2. 若 z > η AND Stretching == NONE → 运动学全部归零
  3. 若 Stretching == VERTICAL 且 z > 0 → 使用 z=0
  4. 若 Stretching == EXTRAPOLATION:
       z_eval = min(z, 0)  (物理部分用实际 z)
       自由面上方: F(z>0) = F(0) + dF/dz|₀ * z
  5. 若 Stretching == WHEELER:
       z_eval = d * (d+z) / (d+η) - d   (映射后总在 [-d,0])
```

---

## 9. 类接口设计

### 9.1 `WaveSpectrum` 类（核心类）

```cpp
/// @brief 波浪谱生成器 - 从谱参数生成波浪成分集
class WaveSpectrum
{
public:
    // ── 构造/初始化 ──
    /// @brief 从 WaveLInput 构造，立即执行完整生成管线
    explicit WaveSpectrum(const WaveLInput &input);

    /// @brief 仅构造不含输入的空对象（用于反序列化等场景）
    WaveSpectrum() = default;

    // ── 谱生成 ──
    /// @brief 执行完整的波浪生成管线（验证→谱离散化→方向分配→波数计算）
    /// @return 生成结果（成分列表 + 统计）
    WaveSpectrumResult Generate(const WaveLInput &input);

    // ── 运动学查询 ──
    /// @brief 计算 (x,y,z,t) 处的线性 Airy 波运动学
    /// @param x 东坐标 [m]
    /// @param y 北坐标 [m]
    /// @param z 垂向坐标 [m]（z=0 为静水面，向上为正）
    /// @param t 时间 [s]
    /// @return 运动学（高程、速度、加速度、动压）
    WaveTrainKinematics GetKinematics(double x, double y, double z, double t) const;

    /// @brief 计算水面高程（不含拉伸）
    double GetElevation(double x, double y, double t) const;

    // ── 访问器 ──
    const std::vector<WaveTrain> &GetWaveTrains() const { return m_waveTrains; }
    const WaveLInput &GetInput() const { return m_input; }
    double GetHs() const;
    double GetTp() const;
    double GetWaterDepth() const { return m_input.waterDepth; }

    // ── 文件 I/O ──
    /// @brief 写入波浪成分文件（频率、波幅、相位、方向、波数）
    void WriteComponentsFile(const std::string &path) const;

    /// @brief 在指定点写入水面时程
    void WriteTimeSeriesFile(const std::string &path, double x, double y,
                             double dt, double duration) const;

    /// @brief 写入摘要文件
    void WriteSummaryFile(const std::string &path) const;

    // ── 工厂方法 ──
    static WaveSpectrum GenerateFromFile(const std::string &qwdPath);
    static WaveSpectrum ImportFromFile(const std::string &inputPath);

private:
    // ── 谱函数 ──
    double S_JONSWAP(double f) const;
    double S_TORSETHAUGEN(double f) const;
    double S_OCHI_HUBBLE(double omega) const;
    double S_USER(double f) const;
    double S_DIRECTIONAL(double dir) const;
    double EvaluateSpectrum(double f) const;

    // ── 管线步骤 ──
    void ValidateInput();
    void DetermineFrequencyRange();
    void DiscretizeFrequencySpectrum();
    void DiscretizeDirectionalSpectrum();
    void CalculateDispersion();
    WaveSpectrumResult AssembleResult();

    // ── 运动学辅助 ──
    double DepthDecayHorizontal(double k, double z, double depth) const;
    double DepthDecayVertical(double k, double z, double depth) const;
    double DepthDecayDynamicPressure(double k, double z, double depth) const;

    // ── 数据成员 ──
    WaveLInput m_input;                    ///< 输入参数（完整副本）
    std::vector<WaveTrain> m_waveTrains;   ///< 波浪成分
    std::mt19937 m_rng;                    ///< 随机数生成器
    double m_normFactor = 1.0;             ///< 谱归一化因子
    double m_spectralArea = 0.0;           ///< ∫S(f)df（归一化前）
    bool m_generated = false;              ///< 是否已执行 Generate()
};
```

### 9.2 `WaveKinematics` 结构体

```cpp
/// @brief 单点波浪运动学结果
struct WaveKinematics
{
    double elevation = 0.0;                 ///< 波面高程 η [m]
    double velocityX = 0.0;                 ///< u_x [m/s]
    double velocityY = 0.0;                 ///< u_y [m/s]
    double velocityZ = 0.0;                 ///< u_z [m/s]
    double accelerationX = 0.0;             ///< a_x [m/s²]
    double accelerationY = 0.0;             ///< a_y [m/s²]
    double accelerationZ = 0.0;             ///< a_z [m/s²]
    double dynamicPressure = 0.0;           ///< p_dyn [Pa]
    bool nodeInWater = false;               ///< 该点是否在水下
};
```

---

## 10. CLI 集成

### 10.1 `main.cpp` 扩展（参照 Generate/Import 模式）

```cpp
// --wave <file.qwd> 处理
if (std::string(argv[i]) == "--wave")
{
    if (i + 1 >= argc) { /* 错误处理 */ }

    const std::string qwdPath = std::filesystem::absolute(argv[i + 1]).string();
    const auto input = ReadWaveLInput(qwdPath);
    const auto progress = [](const std::string &msg) {
        std::cout << msg << std::endl;
    };

    if (input.mode == WaveMode::GENERATE)
    {
        std::cout << std::string(L_CLI_WaveL_RunningGenerate) + qwdPath + "\".\n" << std::flush;
        WaveSpectrum spectrum(input);
        const auto result = spectrum.Generate(input);
        std::cout << L_CLI_WaveL_Generated << "\n";
        std::cout << L_CLI_WaveL_Hs << result.significantHeight << " m\n";
        std::cout << L_CLI_WaveL_Tp << result.peakPeriod << " s\n";
        std::cout << L_CLI_WaveL_Components << result.numComponents << "\n";
        if (!result.componentsFilePath.empty())
            std::cout << L_CLI_WaveL_ComponentFile << result.componentsFilePath << "\n";
        if (!result.timeSeriesFilePath.empty())
            std::cout << L_CLI_WaveL_TSFile << result.timeSeriesFilePath << "\n";
        if (!result.summaryFilePath.empty())
            std::cout << L_CLI_WaveL_Summary << result.summaryFilePath << "\n";
        for (const auto &w : result.warnings)
            std::cout << L_CLI_WaveL_Warning << w << "\n";
        return 0;
    }

    if (input.mode == WaveMode::IMPORT)
    {
        auto spectrum = WaveSpectrum::ImportFromFile(input.importedComponentsPath);
        std::cout << L_CLI_WaveL_Imported << "\n";
        std::cout << L_CLI_WaveL_Hs << spectrum.GetHs() << " m\n";
        std::cout << L_CLI_WaveL_Tp << spectrum.GetTp() << " m\n";
        std::cout << L_CLI_WaveL_Components << spectrum.GetWaveTrains().size() << "\n";
        std::cout << L_CLI_WaveL_Depth << spectrum.GetWaterDepth() << " m\n";
        return 0;
    }
}
```

---

## 11. 输出格式

### 11.1 波浪成分文件（`.wvc` — WaveL Wave Components）

```
# WaveL Wave Components
# Generated: YYYY-MM-DD HH:MM:SS
# Hs = X.XX m, Tp = X.XX s, Depth = X.XX m
# Columns: Frequency[Hz]  Amplitude[m]  Phase[deg]  Direction[deg]  Wavenumber[1/m]
0.0498  0.1234  45.67   15.30   0.0100
0.0501  0.1567  234.12  18.45   0.0101
...
```

### 11.2 水面时程文件（`.wts` — WaveL Time Series）

```
# WaveL Wave Elevation Time Series at (x=0.0, y=0.0)
# dt = 0.10 s, duration = 600.00 s, 6001 steps
# Columns: Time[s]  Elevation[m]
0.000   0.0000
0.100   0.0523
0.200   0.1047
...
```

### 11.3 摘要文件（`.wvm` — WaveL Summary）

```
Qahse WaveL Wave Summary
=========================
Significant wave height Hs: 3.00 m
Peak period Tp:            10.00 s
Peak frequency fp:          0.10 Hz
Mean wave direction:       15.0 deg
Directional spread:        ±30.0 deg, s = 2.0
Water depth:              100.0 m
Frequency spectrum:        JONSWAP (gamma=3.30)
Discretization:            Equal energy, 200 freq bins × 24 dir bins
Total components:          4800
Frequency range:           0.05 - 1.00 Hz
Spectral area:             0.5625 m² (= Hs²/16)
```

---

## 12. 测试计划

### 12.1 单元测试

| 测试组 | 测试用例 | 关键验证点 |
|--------|---------|-----------|
| **谱公式验证** | `JONSWAP_PeakCorrect` | 在 f=fp 处谱密度取极大值 |
| | `JONSWAP_Gamma1_IsPM` | γ=1 退化为 Pierson-Moskowitz |
| | `JONSWAP_IntegralMatchesHs` | ∫S(f)df = Hs²/16（数值积分） |
| | `OchiHubble_MomentSanityCheck` | Hs₁² + Hs₂² = 总能量约束 |
| | `Directional_IntegralIsUnity` | ∫D(θ)dθ = 1.0 |
| **离散化验证** | `EqualEnergy_BinsSameEnergy` | 所有 bin 能量偏差 < 0.1% |
| | `EqualFrequency_BinCount` | 产生 numFreqBins 个成分 |
| | `BandwidthConstrained_BelowDfMax` | 所有 bin Δf ≤ dfMax |
| | `DirectionalDiscretization_SumIsOne` | 方向 bin 积分正常 |
| **色散验证** | `DeepWater_GuoApproachesOmega2OverG` | d=1000m 时 k≈ω²/g |
| | `ShallowWater_GuoApproachesOmegaOverSqrtGd` | d=1m 时 k≈ω/√(gd) |
| | `DispersionTable_IEC61400` | 对标 IEC 61400-3 查表值 |
| **运动学验证** | `DeepWater_VerticalVelocityPhase` | u_z 比 η 相位领先 90° |
| | `StaticWater_SurfaceElevationZero` | η=0 时无波速 |
| | `HorizontalVelocityDecay` | z→-∞ 时速度指数衰减 |
| | `DynamicPressureAtBed` | z=-d 处 p_dyn = ρgη/cosh(kd) |
| **拉伸验证** | `Extrapolation_AboveSWL` | z > 0 处运动学非零且有界 |
| | `Wheeler_MappingToBottom` | z=-d 时 z' = -d |
| | `NoStretching_AboveSWL_Zero` | z > 0 处运动学 = 0 |
| **往返测试** | `GenerateWriteRead_Components` | 导出波浪成分 → 重新导入 → 参数一致 |
| | `RegularWave_ElevationIsSine` | 规则波水面时程是正弦波 |
| **随机种子** | `SameSeed_Reproducible` | 相同种子产生相同结果 |
| | `DifferentSeed_DifferentResults` | 不同种子产生不同结果 |
| **边界条件** | `ZeroHs_GeneratesZeroComponent` | Hs=0 无损 |
| | `ZeroTp_ThrowsException` | Tp=0 抛出异常 |
| | `NegativeWaterDepth_ThrowsException` | depth≤0 抛出异常 |
| | `VeryShallowWater_Warning` | 触发浅水警告 |

### 12.2 对标测试

| 对标准则 | 参考 |
|----------|------|
| JONSWAP 谱值与 OpenFAST 对比（固定种子） | OpenFAST `Waves.f90:JONSWAP()` |
| 等能离散化结果与 QBlade 对比 | QBlade `LinearWave.cpp:DiscretizeFrequencySpectrum()` |
| 深水速度与解析解对比 | Airy 波理论 |

---

## 13. 实施顺序

| 阶段 | 内容 | 预估工作量 |
|------|------|-----------|
| **阶段 1** | 类型定义（`WaveL_Type.hpp`、`WaveLInput`、枚举） + 双语字符串 | 小 |
| **阶段 2** | `WaveSpectrum` 类骨架 + 输入验证 | 小 |
| **阶段 3** | JONSWAP / PM 谱公式实现 + 单元测试 | 中 |
| **阶段 4** | 等能量频率离散化 + 单元测试 | 中 |
| **阶段 5** | 方向离散化（单向 + Cos^(2s) 散布）+ 单元测试 | 中 |
| **阶段 6** | 色散关系计算（Guo 2002 显式）+ 单元测试 | 小 |
| **阶段 7** | 水面高程（实时叠加）+ 单元测试 | 中 |
| **阶段 8** | 速度/加速度/动水压力（深水 + 浅水 + 拉伸）+ 单元测试 | 大 |
| **阶段 9** | Ochi-Hubble + Torsethaugen 谱公式 | 中 |
| **阶段 10** | 用户导入（频谱文件、时程文件、成分文件） | 中 |
| **阶段 11** | 规则波 + 用户自定义谱 | 小 |
| **阶段 12** | CLI 集成（`main.cpp`）+ `--wave` 命令行 | 小 |
| **阶段 13** | 文件输出（`.wvc`、`.wts`、`.wvm`）+ I/O 模块 | 中 |
| **阶段 14** | MacCamy-Fuchs 衍射修正 | 中 |
| **阶段 15** | IFFT 空间场预计算（第二阶段） | 大 |
| **阶段 16** | 有流色散 + Doppler 频移（未来） | 大 |
| **阶段 17** | 批量波浪生成（类比 WindLBatch） | 中 |

---

## 14. 参考代码交叉索引

| 功能 | QBlade (`LinearWave.cpp`) | OpenFAST (`Waves.f90`) |
|------|--------------------------|------------------------|
| JONSWAP 谱 | `:593-617` (S_JONSWAP) | `JONSWAP()` |
| Torsethaugen 双峰 | `:645-716` (S_TORSETHAUGEN) | — (OpenFAST 无此谱) |
| Ochi-Hubble 六参数 | `:632-643` (S_ORCHIHUBBLE) | — (OpenFAST 无此谱) |
| 用户自定义谱 | `:619-630` (S_CUSTOM) | `UserWaveSpctrm()` |
| 方向散布 | `:718-724` (S_DIRECTIONAL) | `CalculateWaveSpreading()` |
| 等能量频率离散化 | `:727-903` (DiscretizeFrequencySpectrum) | `VariousWaves_Init()` |
| 方向离散化 | `:905-959` (DiscretizeDirectionalSpectrum) | `CalculateWaveDirection()` |
| 色散关系 | `:579-591` (Guo 2002) | `WaveNumber()` (Newman) |
| 水面高程 | `:165-179` (GetElevation) | `WaveElevTimeSeriesAtXY()` |
| 速度/加速度 (深水) | `:450-512` | `COSHNumOvrSINHDen` / `SINHNumOvrSINHDen` |
| 速度/加速度 (浅水) | `:373-448` | `COSHNumOvrCOSHDen` 家族 |
| 动水压力 | `:422-424, 434-437` | `WaveDynPC0` |
| MFC 衍射 | `:430-441, 484-497` | `MCFC` 计算 |
| 拉伸方法 | `:374-399` | `SeaSt_WaveField.f90:WaveField_GetNodeWaveKin()` |
| Wheeler 拉伸 | 无（QBlade 不实现） | `:WheelerStretching` |
| 方向波数组合 | `k_nm_comb = √(...)` | `k_{nm}^{-}` / `k_{nm}^{+}` |
| 波浪成分文件格式 | `:414-441 (Write)` | — (OpenFAST 无此格式) |
| 时程 DFT 分解 | `:135-163 (SampleTimeseries)` | `UserWaveElevations_Init()` |

---

## 15. 物理常量

| 符号 | 值 | 单位 | 来源 |
|------|-----|------|------|
| `GRAVITY (g)` | 9.80665 | m/s² | `Params.h` (±已有) |
| `DENSITYWATER (ρ)` | 1025.0 | kg/m³ | `Params.h` (±已有) |
| `KINVISCWATER (ν)` | 1.307e-6 | m²/s | `Params.h` (±已有) |
| `M_PI (π)` | 3.14159265358979323846 | — | `Params.h` (±已有) |
| `TINYVAL (ε)` | 1.0e-10 | — | `Params.h` (±已有) |
