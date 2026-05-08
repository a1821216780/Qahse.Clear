# HydroL 模块重构计划与输入文件设计

## 一、概述

HydroL (Hydrodynamics Library) 负责风力机下部结构水动力载荷计算，包括 Morison 方程（适用于细长构件）和势流理论（适用于大体积浮体）。

**源参考**: `QBlade/src/StructModel/StrModel.cpp` (水动力部分) + `StrElem.cpp`

---

## 二、HydroL 输入文件格式 (`.qhd`)

```ini
-- =============================================================================
-- Qahse.HydroL OC4 半潜式浮式风机 水动力输入文件
-- =============================================================================

[HydroL]
OC4_Semi_Hydro      OBJECTNAME
MORISON_AND_POTFLOW HYDRO_METHOD        - MORISON_ONLY / POTFLOW_ONLY / MORISON_AND_POTFLOW
320.0               WATERDEPTH          - m
1025.0              WATERDENSITY        - kg/m^3
9.80665             GRAVITY             - m/s^2
WHEELER             STRETCHING          - NONE / VERTICAL / WHEELER / EXTRAPOLATION
LOCAL               WAVEKINEVAL         - LOCAL / LAGGED / REF
0.5                 WAVEKINTAU          - s, 运动学滤波时间常数

-- ============================ Morison 构件 ============================

[MorisonCoeffs]
-- id   Ca    Cd    Cp    diameter  displacedVolume  Cd_nG(粗糙度)
1      1.0   0.65  0.0   6.0       10.5             0.0
2      1.0   0.65  0.0   4.5       5.2              0.0
3      1.0   0.65  0.0   1.2       2.1              0.0

[MorisonMembers]
-- id   body_type   body_id_or_name   element_range_begin  element_range_end  coeff_id
1      SUB          Brace_Leg1        0                    5                  1
2      SUB          Brace_Leg2        0                    5                  1
3      SUB          Brace_Diag1       0                    3                  3
4      TOWER        Tower             0                    10                 2
5      MOORING      Mooring1          0                    20                 3

-- ============================ 势流系数文件 ============================

[PotFlowFiles]
WAMIT               POTFLOW_TYPE        - BEMUSE / NEMOH / WAMIT
./hydro/OC4_Radiation.dat    POT_RAD_FILE
./hydro/OC4_Excitation.dat   POT_EXC_FILE
./hydro/OC4_Diff_QTF.dat     POT_DIFF_FILE
./hydro/OC4_Sum_QTF.dat      POT_SUM_FILE

[PotFlowSettings]
ON                  USE_RADIATION
ON                  USE_EXCITATION
OFF                 USE_DIFF_FREQS
OFF                 USE_SUM_FREQS
ON                  USE_MEANDRIFT
ON                  NEWMAN_APPROX
0.02                DELTA_FREQ_RAD       - Hz, 辐射插值步长
0.02                DELTA_FREQ_DIFF      - Hz, 绕射插值步长
5.0                 DELTA_DIR_DIFF       - deg, 绕射方向步长
60.0                TRUNC_TIME_RAD       - s, 辐射IRF截断
60.0                TRUNC_TIME_DIFF      - s, 绕射IRF截断
0.05                DT_IRF               - s, IRF时间步
0.0                 DIFFRACTION_OFFSET

-- ============================ 浮体参数 ============================

[FloaterBody]
-- body_id   cog_x  cog_y  cog_z  hydro_x  hydro_y  hydro_z  tp_x  tp_y  tp_z
Floater1     0.0    0.0    -14.0  0.0      0.0      -14.0    0.0   0.0   10.0

[FloaterMassMatrix]
-- 6x6 质量矩阵 (rows of 6)
Row1   1.40e7   0.0      0.0      0.0      0.0      0.0
Row2   0.0      1.40e7   0.0      0.0      0.0      0.0
Row3   0.0      0.0      1.40e7   0.0      0.0      0.0
Row4   0.0      0.0      0.0      4.2e9    0.0      0.0
Row5   0.0      0.0      0.0      0.0      4.2e9    0.0
Row6   0.0      0.0      0.0      0.0      0.0      1.0e9

[FloaterAddedMassMatrix]  -- 可选，WAMIT 数据中已有则不需要
-- 6x6 ...

[FloaterHydroStiffness]
-- 6x6 ...
Row1   0.0      0.0      0.0      0.0      0.0      0.0
Row2   0.0      0.0      0.0      0.0      0.0      0.0
Row3   0.0      0.0      3.4e5   0.0      0.0      0.0
Row4   0.0      0.0      0.0      1.5e8    0.0      0.0
Row5   0.0      0.0      0.0      0.0      1.5e8    0.0
Row6   0.0      0.0      0.0      0.0      0.0      0.0

[FloaterHydroDamping]   -- 可选，辐射IRF卷积会自动处理
-- 6x6 ...

[FloaterQuadraticDamping]
-- 6x6 ...

[FloaterConstantForce]
-- 1x6 (静水回复力外的常力分量)
0.0   0.0   0.0   0.0   0.0   0.0

-- ============================ 海流廓线 ============================

[CurrentProfile]
0.5                 SURF_CURRENT_U       - m/s, 表层流速
0.0                 SURF_CURRENT_DIR     - deg
0.0                 SURF_CURRENT_DEPTH   - m, 表层参考深度
0.2                 SUB_CURRENT_U        - m/s, 下层流速
45.0                SUB_CURRENT_DIR      - deg
2.0                 SUB_CURRENT_EXP      - 幂指数
0.3                 NEAR_SHORE_U         - m/s, 近岸流
90.0                NEAR_SHORE_DIR       - deg

-- ============================ 海底接触 ============================

[SeabedContact]
1.0e6               SEABED_STIFFNESS     - N/m^3
0.1                 SEABED_DAMPING       - [0-1]
0.5                 SEABED_SHEAR         - [0-1]
0.5                 SEABED_DISC          - m, 海底接触离散

-- ============================ 水动力节点/体映射 ============================

[HydroMapping]
-- 将 MBDL 中的 Node/Body 映射到水动力计算
-- type   mbd_id       hydro_role   coeff_id
BODY     PLATFORM     FLOATER      1
BEAM     Tower        TOWER        2
BEAM     Brace_Leg1   SUBMEMBER    1
BEAM     Brace_Leg2   SUBMEMBER    1
CABLE    Mooring1     MOORING      3
CABLE    Mooring2     MOORING      3
```

---

## 三、HydroL C++ 类设计

### 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/HydroL/HydroL_Type.hpp` | **新建** | 枚举、数据结构 |
| `src/HydroL/HydroL_Config.hpp` | **新建** | `.qhd` 解析 |
| `src/HydroL/HydroL_Solver.hpp` | **新建** | HydroLSolver 声明 |
| `src/HydroL/HydroL_Solver.cpp` | **新建** | 主实现 |
| `src/HydroL/HydroL_Morison.cpp` | **新建** | Morison 方程 |
| `src/HydroL/HydroL_PotFlow.cpp` | **新建** | 势流理论 (IRF卷积) |
| `src/HydroL/HydroL_PotFlowIO.cpp` | **新建** | 势流系数文件读取 |
| `src/HydroL/IO/HydroL_IO.hpp` | **新建** | 序列化 |

### 核心类接口

```cpp
namespace HydroL {

enum class HydroMethod { MORISON_ONLY, POTFLOW_ONLY, MORISON_AND_POTFLOW };
enum class PotFlowType { BEMUSE, NEMOH, WAMIT };
enum class WaveStretching { NONE, VERTICAL, WHEELER, EXTRAPOLATION };
enum class WaveKinEval { LOCAL, LAGGED, REF };

struct HydroLConfig {
    std::string objectName;
    HydroMethod method;
    double waterDepth, waterDensity, gravity;
    WaveStretching stretching = WaveStretching::WHEELER;
    WaveKinEval kinEval = WaveKinEval::LOCAL;
    double kinTau = 0.5;
    
    // Morison 系数
    struct MorisonCoeff {
        int id;
        double Ca, Cd, Cp, diameter, displacedVolume, Cd_nG;
    };
    std::vector<MorisonCoeff> morisonCoeffs;
    
    struct MorisonMember {
        int id;
        int bodyType;       // SUB / TOWER / MOORING
        int bodyIdx;
        int elemBegin, elemEnd;
        int coeffId;
    };
    std::vector<MorisonMember> morisonMembers;
    
    // 势流设置
    PotFlowType potflowType = PotFlowType::WAMIT;
    std::string radFile, excFile, diffQtfFile, sumQtfFile;
    bool useRadiation = true, useExcitation = true;
    bool useSumFreqs = false, useDiffFreqs = false;
    bool useNewmanApprox = true, useMeanDrift = true;
    double deltaFreqRad = 0.02, deltaFreqDiff = 0.02;
    double deltaDirDiff = 5.0;
    double truncTimeRad = 60.0, truncTimeDiff = 60.0;
    double dtIrf = 0.05;
    double diffractionOffset = 0.0;
    
    // 浮体矩阵
    Eigen::Matrix<double, 6, 6> M_HYDRO, K_HYDRO, R_HYDRO, A_HYDRO, R2_HYDRO;
    Eigen::Matrix<double, 6, 1> F_CONST;
    
    // 海流
    double surfCurrentU = 0, surfCurrentDir = 0, surfCurrentDepth = 0;
    double subCurrentU = 0, subCurrentDir = 0, subCurrentExp = 2.0;
    double nearShoreU = 0, nearShoreDir = 0;
    
    // 海底
    double seabedStiffness = 0, seabedDamping = 0, seabedShear = 0;
    double seabedDisc = 0.5;
};

struct HydroLInput {
    // WaveL 采样器
    std::function<double(const Vec3&, double)> elevationSampler;
    std::function<void(const Vec3&, double, double, double, int, Vec3*, Vec3*, double*)> kinematicsSampler;
    
    // MBDL 结构状态
    double time;
    // --- Morison 评估点 ---
    std::vector<Vec3> structurePositions;  // 单元中点
    std::vector<Vec3> structureVelocities;
    std::vector<Vec3> structureAccelerations;
    std::vector<Vec3> structureOrientations; // 单元方向
    
    // --- 势流浮体运动 ---
    Vec3 platformPosition;
    Vec3 platformVelocity;    // 6-DOF: [x,y,z,rotx,roty,rotz]
    Vec3 platformAcceleration;
    Vec3 platformOrientation; // Euler angles
    Vec3 platformOmega;       // angular velocity
    
    // 映射表 (structurePositions 中每个点的类型)
    std::vector<int> elementTypes;  // TOWER/SUB/MOORING
    std::vector<int> elementCoeffIds;
};

struct HydroLOutput {
    struct MorisonResult {
        int elementIdx;
        Vec3 force, moment;
        double Ca, Cd, Re, KC;  // 实际使用的系数
    };
    std::vector<MorisonResult> morisonResults;
    
    Eigen::Matrix<double, 6, 1> radiationForce;
    Eigen::Matrix<double, 6, 1> diffractionForce;
    Eigen::Matrix<double, 6, 1> sumForce;
    Eigen::Matrix<double, 6, 1> diffForce;
    Eigen::Matrix<double, 6, 1> meanDriftForce;
    
    Eigen::Matrix<double, 6, 1> buoyancyForce;
    Eigen::Matrix<double, 6, 1> hydrostaticRestoring;
    
    // 综合力
    Eigen::Matrix<double, 6, 1> totalHydroForce;
};

class HydroLSolver {
public:
    HydroLSolver();
    ~HydroLSolver();
    
    bool ParseConfigFile(const std::string& qhdPath);
    bool Initialize();
    
    // ── 单步计算 ──
    HydroLOutput Step(const HydroLInput& input);
    
private:
    // --- Morison ---
    void calcMorisonForces(const HydroLInput& input, HydroLOutput& output);
    
    // --- 势流 ---
    void calcPotFlowForces(const HydroLInput& input, HydroLOutput& output);
    void calcRadiationForces(const HydroLInput& input, HydroLOutput& output);
    void calcDiffractionForces(const HydroLInput& input, HydroLOutput& output);
    void calcSecondOrderForces(const HydroLInput& input, HydroLOutput& output);
    void calcMeanDriftForces(HydroLOutput& output);
    
    // --- 辅助 ---
    void calcBuoyancy(HydroLOutput& output);
    Vec3 calcCurrentVelocity(const Vec3& pos, double depth) const;
    
    HydroLConfig m_config;
    
    // 势流 IRF 缓存
    struct IRFData {
        Eigen::VectorXf k1, k2, k3, k4, k5, k6;   // 辐射 IRF 核
        std::vector<Eigen::MatrixXf> H_ij;          // 辐射 IRF 矩阵
        std::vector<Eigen::MatrixXcf> X_ij;         // 绕射 IRF
        std::vector<Eigen::MatrixXcf> QTF_s, QTF_d; // QTF
        std::vector<float> freq_rad, freq_diff, dir_diff;
        // 历史缓存
        std::vector<Eigen::Matrix<double, 6, 1>> velocityHistory;
    };
    IRFData m_irfData;
    
    bool m_potflowInitialized = false;
};
}
```

---

## 四、Morison 方程实现

```cpp
void HydroLSolver::calcMorisonForces(const HydroLInput& input, HydroLOutput& output) {
    double rho = m_config.waterDensity;
    double depth = m_config.waterDepth;
    int stretchType = static_cast<int>(m_config.stretching);
    
    for (size_t i = 0; i < input.structurePositions.size(); i++) {
        const Vec3& pos = input.structurePositions[i];
        const Vec3& vel_structure = input.structureVelocities[i];
        const Vec3& acc_structure = input.structureAccelerations[i];
        
        int coeffId = input.elementCoeffIds[i];
        const auto& coeff = m_config.morisonCoeffs[coeffId];
        
        // 1. 获取波浪运动学
        double elevation = input.elevationSampler(pos, input.time);
        Vec3 waterVel, waterAcc;
        double dynP;
        input.kinematicsSampler(pos, input.time, elevation, depth, stretchType,
                                 &waterVel, &waterAcc, &dynP);
        
        // 2. 叠加海流
        waterVel += calcCurrentVelocity(pos, depth);
        
        // 3. 相对速度/加速度
        Vec3 relVel = waterVel - vel_structure;
        Vec3 relAcc = waterAcc - acc_structure;
        
        // 4. 分解为法向/切向
        Vec3 dir = input.structureOrientations[i];  // 单元轴向
        Vec3 relVel_n = relVel - dir * (relVel.dot(dir));
        Vec3 relAcc_n = relAcc - dir * (relAcc.dot(dir));
        
        double vel_n_mag = relVel_n.norm();
        
        // 5. Morison 力 = 惯性项 + 阻力项
        // F = rho * V * (1+Ca) * a_n + 0.5 * rho * Cd * D * |v_n| * v_n
        double Ca = coeff.Ca;
        double Cd = coeff.Cd;
        double diam = coeff.diameter;
        double volume = coeff.displacedVolume;
        double Cd_nG = coeff.Cd_nG;  // 粗糙度修正(用于缆绳)
        
        Vec3 inertiaForce = rho * volume * (1.0 + Ca) * relAcc_n;
        Vec3 dragForce = 0.5 * rho * Cd * diam * vel_n_mag * relVel_n;
        
        // 6. 浮力修正 (Archimedes)
        Vec3 buoyancyForce(0, 0, rho * m_config.gravity * volume);
        // 当构件在水面附近时需要修正浸没体积
        
        HydroLOutput::MorisonResult result;
        result.elementIdx = static_cast<int>(i);
        result.force = inertiaForce + dragForce;
        result.moment.setZero();  // Morison按集中力施加, 力矩由MBDL计算
        result.Ca = Ca;
        result.Cd = Cd;
        output.morisonResults.push_back(result);
    }
}
```

---

## 五、势流力计算流程

```
POTFLOW_Initialize()
├── ReadRadiationFile (BEMuse/NEMOH/WAMIT)
│   ├── A_ij (added mass) @ freq
│   ├── B_ij (radiation damping) @ freq
│   └── POTFLOW_Radiation_IRF() → k_1..k_6 (卷积核)
│       └── 逆FFT: B_ij(ω) → IRF(t)
├── ReadExcitationFile
│   ├── X_ij (excitation force coeff) @ freq, dir
│   └── POTFLOW_DiffractionIRF() → H_ij (绕射IRF)
├── ReadQTF_DiffFile → QTF_d(ωi, ωj, Δθ)
├── ReadQTF_SumFile → QTF_s(ωi, ωj, Δθ)

POTFLOW_ApplyForces()
├── POTFLOW_CalcRadiationForces()
│   └── Conv(k_1..k_6, velocity_history) → F_radiation(6DoF)
├── POTFLOW_CalcDiffractionForces()
│   └── Conv(H_ij, wave_elevation_history(direction)) → F_diffraction(6DoF)
├── POTFLOW_CalcSecondOrder_Forces()
│   └── QTF quadrature over wave spectrum → F_sum(6DoF) + F_diff(6DoF)
├── POTFLOW_CalculateMeanDriftForces()
│   └── Diag(QTF_d, wave_amplitude^2) → F_meanDrift(6DoF)
└── Hydrostatic restoring: -K_HYDRO * [x,y,z,rx,ry,rz]
```

---

## 六、QBlade 源码映射

| HydroL 实现文件 | QBlade 源方法 |
|----------------|--------------|
| `HydroL_Solver.cpp::Step` | `StrModel::ApplyExternalForcesAndMoments` (Hydro部分) |
| `HydroL_Morison.cpp` | `StrElem::AddMorisonForces` + `StrElem::AddBuoyancy` + `StrElem::EvaluateSeastateElementQuantities` |
| `HydroL_PotFlow.cpp` | `StrModel::POTFLOW_ApplyForces` → `POTFLOW_CalcRadiationForces` + `POTFLOW_CalcDiffractionForces` + `POTFLOW_CalcSecondOrder_Forces` |
| `HydroL_PotFlowIO.cpp` | `StrModel::POTFLOW_ReadBEMuse` / `POTFLOW_ReadNemoh` / `POTFLOW_ReadWamit` |
| `HydroL_PotFlow.cpp::calcIRF` | `StrModel::POTFLOW_Radiation_IRF` + `POTFLOW_DiffractionIRF` |

---

## 七、势流系数文件格式兼容

HydroL 需支持读取以下三种势流求解器的输出格式：

| 格式 | 文件类型 | 读取函数 |
|------|---------|---------|
| **BEMuse** (HAWC2) | `.dat` (辐射+激励) | `POTFLOW_ReadBEMuse` |
| **NEMOH** | `RadiationCoefficients.tec`, `ExcitationForce.tec` | `POTFLOW_ReadNemoh` |
| **WAMIT** | `.1`/`.2`/`.3`/`.hst`/`.12d`/`.12s` | `POTFLOW_ReadWamit` |

---

## 八、实现步骤

1. **Step 1**: `HydroL_Type.hpp` + `HydroL_Config.hpp` — 数据结构和 `.qhd` 解析
2. **Step 2**: `HydroL_Morison.cpp` — Morison 方程（梁单元 + 缆绳）
3. **Step 3**: `HydroL_Morison.cpp` — 浮力 + 静水回复力
4. **Step 4**: `HydroL_PotFlowIO.cpp` — BEMuse/NEMOH/WAMIT 系数文件读取
5. **Step 5**: `HydroL_PotFlow.cpp` — 辐射 IRF 卷积 + 绕射力
6. **Step 6**: `HydroL_PotFlow.cpp` — 二阶 QTF 力 + 平均漂移力
7. **Step 7**: `HydroL_Solver.cpp` — 整合 Morison + 势流
8. **Step 8**: 单元测试（固定圆柱 Morison 验证、OC3 Spar 水静力/水动力验证）
