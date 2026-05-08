# AeroL 模块重构计划与输入文件设计

## 一、概述

AeroL (Aerodynamics Library) 负责风力机叶片气动力计算，支持 BEM (Blade Element Momentum)、FVW (Free Vortex Wake) 和 VPML (Vortex Particle Meshless) 三种气动模型，以及 OYE/Gormont-Berg/ATEFlap 动态失速修正。

**源参考**: `QBlade/src/QTurbine/QTurbineSimulationData.cpp` + `src/VortexObjects/` + `src/VPML/`

---

## 二、AeroL 输入文件格式 (`.qad`)

```ini
-- =============================================================================
-- Qahse.AeroL NREL 5MW 气动计算输入文件
-- =============================================================================

[AeroL]
NREL_5MW_Aero       OBJECTNAME
HAWT                TURBTYPE           - HAWT / VAWT
UPWIND              ROTORCONFIG        - UPWIND / DOWNWIND
STANDARD            ROTATIONALDIR      - STANDARD / REVERSED
3                   NUMBLADES
5.019               OVERHANG           - m
5.0                 SHAFTTILT          - deg
-2.5                ROTORCONE          - deg
90.0                HUBHEIGHT          - m
1.5                 HUBRADIUS          - m
1.225               AIRDENSITY         - kg/m^3
1.647e-05           KINVISCOSITY       - m^2/s

-- ============================ 气动模型选择 ============================

[AeroModel]
VORTEX              WAKETYPE            - VORTEX(FVW) / U_BEM(非定常BEM) / VPML
ATEFLAP             DYNSTALL_TYPE       - NONE / OYE / GORMONT / ATEFLAP
ON                  BEMTIPLOSS          - Prandtl 叶尖损失
ON                  HIMMELSKAMP         - Himmelskamp 旋转失速延迟
ON                  TWO_POINT_LIFTDRAG  - 2点升阻力评估
PC2B                WAKEINTEGRATION     - EULER / PC / PC2B
HHMEAN              CONVECTIONTYPE      - LOCALMEAN / HHMEAN(轮毂高度平均) / LOCALTURB(当地湍流)
ON                  TOWERSHADOW         - 塔影效应
1.2                 TOWERDRAGCOEFF      - 塔筒阻力系数

-- ============================ 动态失速参数 ============================

[DynamicStall]
3.0                 TF_OYE              - OYE 时间常数 Tf
1.0                 AM_GB               - Gormont-Berg Am 因子
3.0                 TF_ATE              - ATEFlap Tf
2.0                 TP_ATE              - ATEFlap Tp

-- ============================ FVW 尾迹参数 ============================

[WakeParams]
ON                  WAKE_ROLLUP         - 尾迹自诱导
ON                  TRAILING_VORT       - 尾随涡
ON                  SHED_VORT           - 脱落涡
OFF                 INCLUDE_STRAIN      - 涡量拉伸
0.5                 WAKERELAXATION      - 尾迹松弛因子 [0-1]
50000               MAXWAKE_ELEMENTS    - 最大尾迹元素数(硬限制)
10.0                MAXWAKE_DISTANCE    - 最大尾迹距离(/直径)
0.1                 FIRSTWAKE_ROWLEN    - 第一行尾迹长度(/chord)
0.25                NEARWAKE_LENGTH     - 近尾迹长度
1.5                 ZONE1_LENGTH        - Zone 1 长度
2.5                 ZONE2_LENGTH        - Zone 2 长度
5.0                 ZONE3_LENGTH        - Zone 3 长度
4                   ZONE1_FACTOR        - Zone 1 因子(整数)
2                   ZONE2_FACTOR
1                   ZONE3_FACTOR
2                   REDUCTION_FACTOR    - 尾迹压缩因子
5.0                 CONVERSION_LENGTH   - 尾迹转换长度
0.2                 WAKECORERADIUS      - 尾迹涡核半径(/chord)
0.05                BOUNDCORERADIUS     - 附着涡核半径(/chord)
1.0e-04             VORTEXVISCOSITY     - 湍流涡黏性
0.1                 MAXSTRAIN           - 最大应变(移除判据)

-- ============================ BEM 迭代参数 ============================

[BEMParams]
50                  MAXITERATIONS       - 最大 Gamma 迭代次数
0.5                 RELAXATION_FACTOR   - Gamma 松弛因子 [0-1]
0.001               CONVERGENCE_EPS     - 相对收敛判据
1.5                 BEM_SPEEDUP         - BEM 初值加速时间 (s)
20                  POLARDISC           - 非定常BEM极值离散数

-- ============================ 叶片几何文件 ============================

[BladeFile]
./blade/NREL_5MW.bld                    - QBlade .bld 叶片定义文件

-- ============================ 360°极值数据库 ============================

[PolarFiles]
-- relPos_begin  relPos_end   polar_file_path
0.000            0.050        ./polars/Cylinder1_360.plr
0.050            0.150        ./polars/Cylinder2_360.plr
0.150            0.250        ./polars/DU40_A17_360.plr
0.250            0.350        ./polars/DU35_A17_360.plr
0.350            0.450        ./polars/DU30_A17_360.plr
0.450            0.550        ./polars/DU25_A17_360.plr
0.550            0.650        ./polars/DU21_A17_360.plr
0.650            0.750        ./polars/NACA64_A17_360.plr
0.750            0.850        ./polars/NACA64_A17_360.plr
0.850            0.950        ./polars/NACA64_A17_360.plr
0.950            1.000        ./polars/NACA64_A17_360.plr

-- ============================ 主动流动控制（可选） ============================

[AFC]
-- AFC_ID  station_a  station_b  dyn_polar_set_file
AFC_1      0.70       0.85       ./polars/DU21_AFC.dps

-- ============================ 叶片损伤定义（可选） ============================

[BDamage]
-- BDAMAGE_ID  blade  station_a  station_b   polar_a   polar_b
BDamage_1      1      0.20        0.40        ./polars/DU35_Damage.plr  ./polars/DU30_Damage.plr

-- ============================ 离散化 ============================

[Discretization]
COSINE              DISCTYPE            - LINEAR / COSINE / STRUCT / AERO
20                  NUMPANELS           - 每叶片面板数
0                   NUMSTRUTPANELS      - 支杆面板数 (VAWT)
OFF                 STRUTLIFT           - 支杆是否升力 (VAWT)

-- ============================ OpenCL 加速参数（后续版本） ============================

[OpenCL]
OFF                 USE_OPENCL          - 是否使用 OpenCL
0                   DEVICE_ID           - 设备编号
1024                WORKGROUP_SIZE      - 工作组大小

-- ============================ VPML 参数（后续版本） ============================

[VPMLParams]
2                   DIMENSION           - 2D / 3D
GREEN               SOLVER_TYPE         - GREEN / POISSON
DIRECT              INTEGRATION_METHOD  - DIRECT / MULTILEVEL / OPENCL
SMG                 TURBULENCE_MODEL    - SMG / RVM
GAUSSIAN            REG_KERNEL          - 高斯核
3                   REMESH_SCHEME
```

---

## 三、AeroL C++ 类设计

### 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/AeroL/AeroL_Type.hpp` | **新建** | 枚举、配置结构体、输入输出结构体 |
| `src/AeroL/AeroL_Config.hpp` | **新建** | `.qad` 解析 + `.bld` 叶片文件解析 |
| `src/AeroL/AeroL_Solver.hpp` | **新建** | AeroLSolver 声明 |
| `src/AeroL/AeroL_Solver.cpp` | **新建** | 主 BEM 求解器 + 动态失速 |
| `src/AeroL/AeroL_PanelVelocity.cpp` | **新建** | 面板速度计算(风场采样+诱导+塔影) |
| `src/AeroL/AeroL_Wake.cpp` | **新建** | FVW 尾迹演化(Biot-Savart, PC2B) |
| `src/AeroL/AeroL_VPML.cpp` | **新建** | VPML 求解器(后续) |
| `src/AeroL/AeroL_PolarDB.hpp` | **新建** | 360°极值数据库类 |
| `src/AeroL/AeroL_PolarDB.cpp` | **新建** | 极值插值 + 分离流分解 |
| `src/AeroL/IO/AeroL_IO.hpp` | **新建** | 序列化 |

### 核心类接口

```cpp
namespace AeroL {

enum class WakeType { VORTEX, U_BEM, VPML };
enum class DynamicStallType { NONE, OYE, GORMONT, ATEFLAP };
enum class WakeIntegration { EULER, PC, PC2B };
enum class WakeConvection { LOCALMEAN, HHMEAN, LOCALTURB };
enum class DiscType { LINEAR, COSINE, STRUCT, AERO };

struct AeroLConfig {
    // 基本
    std::string objectName;
    bool isVAWT = false, isUpwind = true, isReversed = false;
    int numBlades = 3;
    double overHang, shaftTilt, rotorCone;
    
    // 模型选择
    WakeType wakeType = WakeType::VORTEX;
    DynamicStallType dynStallType = DynamicStallType::NONE;
    WakeIntegration wakeInteg = WakeIntegration::PC2B;
    WakeConvection convectionType = WakeConvection::HHMEAN;
    
    // 动态失速参数
    double tfOye = 3.0, amGb = 1.0, tfAte = 3.0, tpAte = 2.0;
    
    // BEM 参数
    int maxIterations = 50;
    double gammaRelaxation = 0.5;
    double convergenceEps = 0.001;
    double bemSpeedUp = 1.5;
    int polarDisc = 20;
    bool bemTipLoss = true, himmelskamp = true;
    bool twoPointLiftDrag = true, towerShadow = true;
    
    // 尾迹参数
    bool wakeRollup = true, trailingVort = true, shedVort = true;
    bool includeStrain = false;
    double wakeRelaxation = 0.5;
    int maxWakeElements = 50000;
    double maxWakeDist = 10.0;
    double firstWakeRowLen = 0.1;
    double nearWakeLen, zone1Len, zone2Len, zone3Len;
    int zone1Factor, zone2Factor, zone3Factor, reductionFactor;
    double conversionLen, wakeCoreRadius, boundCoreRadius;
    double vortexViscosity = 1e-4, maxStrain = 0.1;
    
    // 离散化
    DiscType discType = DiscType::COSINE;
    int numPanels = 20, numStrutPanels = 0;
    bool strutLift = false;
    
    // 叶片几何
    BladeGeometry blade;          // 从 .bld 解析
    // 极值数据库
    std::vector<PolarAssignment> polarAssignments;  // 沿展向的极值映射
    
    // 流体
    double airDensity = 1.225, kinVisc = 1.647e-05;
};

struct AeroLInput {
    // WindL 风场采样器
    std::function<Vec3(const Vec3& pos, double time)> windSampler;
    
    // MBDL 结构状态（每步更新）
    double time, dt;
    std::vector<CoordSys> bladeCoords;       // per panel
    std::vector<Vec3>     bladeVelocities;   // per panel
    std::vector<Vec3>     bladeAccelerations; // per panel
    std::vector<CoordSys> hubCoords;         // per blade
    double rotorSpeed;                        // rad/s
    double azimuth;                           // rad
    
    // 塔影查询
    std::function<double(const Vec3&)> towerRadiusFunc;
    std::function<double(const Vec3&)> towerDragFunc;
};

struct AeroLOutput {
    struct PanelData {
        int bladeIdx, panelIdx;
        double relPos;          // 归一化展向位置
        Vec3 ctrlPt;            // 控制点全局坐标
        Vec3 force;             // 全局力
        Vec3 moment;            // 全局力矩
        Vec3 force_dAlpha;      // 力对攻角导数(切线刚度)
        Vec3 moment_dAlpha;
        double CL, CD, CM;      // 气动系数
        double AoA, Re;         // 攻角, 雷诺数
        double gamma;           // 环量
        Vec3 V_total;            // 总相对速度
    };
    std::vector<PanelData> panels;
    
    // 用于 MBDL 分布式载荷的结构化数据
    struct DistributedLoad {
        int bodyIdx, elementIdx;
        double lA, lB;          // 单元端部展向位置
        Vec3 forcePerLength;    // Cn, Ct 分量
        Vec3 dForcePerLength_dAlpha;
        Vec3 momentPerLength;
        Vec3 dMomentPerLength_dAlpha;
    };
    std::vector<DistributedLoad> distributedLoads;
    
    // 转子性能
    double thrust, torque, power;
    double Ct, Cp, TSR;
    
    // 尾迹可视化数据
    std::vector<VortexSegment> wakeSegments;
    
    // 控制器反馈
    double genTorque, rotThrust;
};

class AeroLSolver {
public:
    AeroLSolver();
    ~AeroLSolver();
    
    bool ParseConfigFile(const std::string& qadPath);
    bool Initialize();
    
    // ── 单步计算 ──
    AeroLOutput Step(const AeroLInput& input);
    
    // ── 极值查询 (供外部) ──
    BladeParameters GetBladeParameters(double relPos, double aoA75, 
                                        double windspeed) const;
    
    // ── 状态查询 ──
    double GetRotorThrust() const;
    double GetRotorTorque() const;
    double GetRotorPower() const;
    double GetRotorOmega() const;
    double GetRotorAzimuth() const;
    
    // ── 尾迹可视化 ──
    std::vector<VortexSegment> GetWakeGeometry() const;
    
private:
    // --- BEM 求解器 ---
    void gammaBoundFixedPointIteration();
    void calcBladePanelVelocities();
    void calcSteadyBladePanelCoefficients();
    void calcBladeCirculation();
    bool gammaConvergenceCheck();
    Vec3 getFreeStream(const Vec3& pos) const;
    
    // --- 动态失速 ---
    void calcDynamicBladeCoefficients();
    void applyOYE();
    void applyGormont();
    void applyATEFlap();
    
    // --- FVW 尾迹 ---
    void wakeCalculations(double dt);
    Vec3 calculateWakeInduction(const Vec3& pt) const;
    Vec3 calculateBladeInduction(int panelIdx) const;
    void addWakeElements();
    void calculateNewWakeState(double dt);
    void convertLinesToParticles();
    void truncateWake();
    void reduceWake();
    
    // --- 辅助 ---
    void calcTowerInfluence();
    Vec3 calcUnsteadyBEMInduction(int panelIdx) const;
    
    // --- 状态 ---
    AeroLConfig m_config;
    std::vector<BladePanel> m_panels;          // 面板列表
    std::vector<WakeVortexLine> m_wakeLines;   // 尾迹线元
    std::vector<WakeVortexParticle> m_wakeParticles; // VPML粒子
    
    // Polar 数据库
    std::unique_ptr<PolarDatabase> m_polarDB;
    
    // 上一时间步缓存
    std::vector<double> m_gammaPrev;
    double m_prevAzimuth = 0;
    int m_currentWakeRev = 0;
    
    // 统计
    double m_rotorThrust = 0, m_rotorTorque = 0, m_rotorPower = 0;
};
}
```

---

## 四、360° Polar 数据库类

```cpp
class PolarDatabase {
public:
    struct PolarPoint {
        double aoa;     // deg
        double cl, cd, cm;
        // 分离流分解 (360° polar)
        double cl_att, cl_sep, f_st;
        double dcl_dalpha, dcd_dalpha, dcm_dalpha;
    };
    
    struct PolarCurve {
        double re;                           // 雷诺数
        double thickness;                    // 翼型相对厚度
        std::vector<PolarPoint> points;      // AoA 0→360
        bool isDecomposed = false;           // 是否已分解
        
        PolarPoint interpolate(double aoa) const;
    };
    
    struct BladeParameters {
        double cl, cd, cm;
        double dcl, dcd, dcm;
        double cl_att, cl_sep, f_st;
        double re;          // 实际雷诺数
        double aoa;         // 实际攻角
    };
    
    bool LoadPolar360File(const std::string& plrPath);
    BladeParameters GetParameters(double relPos, double aoA75,
                                   double freeStreamVel, double chord,
                                   double omega, double radius,
                                   bool himmelskamp, double tsr) const;
};
```

---

## 五、叶片几何类 (解析 .bld 文件)

```cpp
class BladeGeometry {
public:
    struct BladeStation {
        double pos;        // 径向位置 (m)
        double chord;      // 弦长 (m)
        double twist;      // 气动扭转 (deg)
        double offsetX;    // 预弯 X (m)
        double offsetY;    // 预弯 Y (m)
        double pitchAxis;  // 变桨轴位置 (0-1)
        std::string polarFile;
    };
    
    struct BladeDiscretization {
        double pos;           // 归一化径向位置
        double chord;
        double twist;
        Vec3 localPos;        // 本地坐标
        Vec3 chordwiseDir;    // 弦向方向
        Vec3 thicknessDir;    // 厚度方向
    };
    
    bool LoadFromBLD(const std::string& bldPath);
    std::vector<BladeDiscretization> Discretize(DiscType type, int nPanels);
    
    double GetChord(double normPos) const;
    double GetTwist(double normPos) const;
    double GetPitchAxis(double normPos) const;
    
    int m_numBlades;
    double m_radius, m_hubRadius;
    double m_sweptArea;
    bool m_isVAWT;
    
private:
    std::vector<BladeStation> m_stations;
};
```

---

## 六、QBlade 源码映射

| AeroL 实现文件 | QBlade 源方法 |
|---------------|--------------|
| `AeroL_Solver.cpp::Step` | `QTurbineSimulationData::gammaBoundFixedPointIteration` |
| `AeroL_PanelVelocity.cpp` | `QTurbineSimulationData::calcBladePanelVelocities` |
| `AeroL_Solver.cpp::calcSteadyCoeffs` | `QTurbineSimulationData::calcSteadyBladePanelCoefficients` |
| `AeroL_Solver.cpp::calcCirculation` | `QTurbineSimulationData::calcBladeCirculation` |
| `AeroL_Solver.cpp::calcDynamicDS` | `QTurbineSimulationData::calcDynamicBladeCoefficients` |
| `AeroL_PolarDB.cpp` | `CBlade::getBladeParameters` (Blade.cpp) |
| `AeroL_Wake.cpp` | `QSimulation::wakeCalculations` + `calculateWakeInduction` + `addWakeElements` + `calculateNewWakeState` |
| `AeroL_PanelVelocity.cpp::getFreeStream` | `QTurbineSimulationData::getFreeStream` |
| `AeroL_VPML.cpp` | `VPML/` 目录 (~15 files) |

---

## 七、实现步骤

1. **Step 1**: `AeroL_Type.hpp` — 所有枚举和数据结构
2. **Step 2**: `AeroL_PolarDB` — 360°极值数据库加载、插值、分离流分解
3. **Step 3**: `BladeGeometry` — `.bld` 解析 + 叶片离散化
4. **Step 4**: `AeroL_Config` — `.qad` 解析
5. **Step 5**: `AeroL_Solver` 骨架 — Initialize + Step 主循环
6. **Step 6**: `AeroL_PanelVelocity` — 面板速度计算（风场采样 + 诱导 + 塔影）
7. **Step 7**: 稳态 BEM 求解器（无尾迹）
8. **Step 8**: 动态失速模型（OYE → Gormont-Berg → ATEFlap）
9. **Step 9**: FVW 尾迹演化（Biot-Savart + PC2B + 截断/压缩）
10. **Step 10**: VPML 求解器（后续版本）
11. **Step 11**: 单元测试（NREL 5MW 稳态功率曲线 + 动态失速衰减）
