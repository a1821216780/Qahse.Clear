# MBDL 模块重构计划与输入文件设计

## 一、概述

MBDL (Multi-Body Dynamics Library) 基于 Project Chrono 引擎，负责风力机结构多体动力学模型的构建、时域推进和模态分析。

**源参考**: `QBlade/src/StructModel/StrModel.cpp` (~14000行)

---

## 二、MBDL 输入文件格式 (`.qmd`)

`.qmd` 文件采用分段式 INI 风格（列值+注释），与现有 `demo/MBDL/Qahse_MBDL_Main_DEMO.qmd` 风格一致。

### 完整示例

```ini
-- =============================================================================
-- Qahse.MBDL NREL 5MW 陆上风机 结构输入文件
-- =============================================================================

[Simulation]
0.0           StartTime            - s, 仿真起始时间
660.0         Duration             - s, 仿真时长
0.01          TimeStep             - s, 结构积分步长
9.80665       Gravity              - m/s^2, 全局重力加速度 -Z
1             OutputEvery          - 每 N 步输出一次
1             ChronoThreads        - Chrono 并行线程数
true          AutomaticGravity     - Chrono FEA 单元自动施加重力
SPARSE_LU     Solver               - 线性求解器: SPARSE_LU / SPARSE_QR
HHT           Timestepper           - 时间积分器: HHT / NEWMARK / TRAPEZOIDAL / EULER_IMPLICIT_LINEARIZED / EULER_IMPLICIT_PROJECTED / EULER_IMPLICIT
1             IntegrationIters     - 隐式积分器每次迭代次数
./result      OutputDir            - 结果输出目录
NREL5MW       OutputName           - 输出文件前缀

-- ============================ 风机全局参数 ============================

[GlobalParams]
HAWT          TurbineType           - HAWT / VAWT
3             NumBlades             - 叶片数
5.019         OverHang              - m, 轮毂悬伸
-2.5          PreCone               - deg, 锥角
5.0           ShftTilt              - deg, 轴倾角
1.9           Twr2Shft              - m, 塔筒中心到轴距离
90.0          HubHeight             - m, 轮毂高度
0.001         GlobalGeometryEps     - m, 全局几何容差

-- ============================ 节点定义 ============================

[Nodes]
-- id              x      y      z         fixed   nodal_mass  Ixx   Iyy   Izz
TowerBase          0.0    0.0    0.0       true    0.0         0.0   0.0   0.0
TowerTop           0.0    0.0    87.6      false   0.0         0.0   0.0   0.0
YawFixed           0.0    0.0    87.6      false   0.0         0.0   0.0   0.0
YawFree            0.0    0.0    87.6      false   0.0         0.0   0.0   0.0
ShaftFixed         5.019  0.0    90.0      false   0.0         0.0   0.0   0.0
ShaftToHub         5.019  0.0    90.0      false   0.0         0.0   0.0   0.0
HubFixed           5.019  0.0    90.0      false   0.0         0.0   0.0   0.0
Blade1PitchFixed   5.019  0.0    90.0      false   0.0         0.0   0.0   0.0
Blade1PitchFree    5.019  0.0    90.0      false   0.0         0.0   0.0   0.0
-- Blade2/3 类似...

-- ============================ 梁截面属性 ============================

[BeamSections]
-- id         area   Iyy    Izz    J      E          G          density   rayleigh_alpha  rayleigh_beta
TowerBaseSec  2.3    12.0   12.0   8.0   2.10e11    8.08e10    7850.0    0.0             1.0e-4
TowerMidSec   0.8    3.0    3.0    2.0   2.10e11    8.08e10    7850.0    0.0             1.0e-4
BladeRootSec  0.15   0.08   0.06   0.04  1.39e10    4.50e09    2500.0    0.0             1.0e-4
-- ... more sections

-- ============================ 梁构件定义 ============================

[Beams]
-- id      nodeA      nodeB      section       elements  ydir_x  ydir_y  ydir_z
Tower     TowerBase  TowerTop   TowerBaseSec   20        1.0     0.0     0.0
Blade1    Blade1Root Blade1Tip  BladeRootSec   15        1.0     0.0     0.0
Blade2    Blade2Root Blade2Tip  BladeRootSec   15        1.0     0.0     0.0
Blade3    Blade3Root Blade3Tip  BladeRootSec   15        1.0     0.0     0.0

-- ============================ 刚体定义 ============================

[RigidBodies]
-- id        x     y     z       rx   ry   rz   mass       Ixx       Iyy       Izz       fixed
Nacelle     0.0   0.0   90.0    0    0    0    240000.0   4.0e6     2.5e6     2.5e6     false
Hub         0.0   0.0   90.0    0    0    0    56780.0    115926.0  115926.0  5340.0    false
YawBearing  0.0   0.0   87.6    0    0    0    53800.0    100000.0  100000.0  100000.0  false

-- ============================ 传动链 ============================

[DriveTrain]
-- id          LSS_node    HSS_node    gearbox_ratio   efficiency   gen_inertia   stiffness     damping
DriveTrain     ShaftFixed  GenHSS      97.0            0.95         534.116       8.676e8       6.215e6
AxiallyFreeHub  false       -- 轮毂是否轴向自由
DrivetrainDOF   true        -- 是否建模传动链扭转自由度

-- ============================ 制动器 ============================

[Brake]
-- max_torque    deploy_time   delay_time
20000.0          0.01           0.05

-- ============================ 变桨驱动 ============================

[PitchDrives]
-- blade   fixed_node          free_node           max_rate    max_torque    error_angle
Blade1    Blade1PitchFixed    Blade1PitchFree     8.0         50000.0        0.0
Blade2    Blade2PitchFixed    Blade2PitchFree     8.0         50000.0        0.0
Blade3    Blade3PitchFixed    Blade3PitchFree     8.0         50000.0        0.0

-- ============================ 偏航驱动 ============================

[YawDrive]
-- fixed_node  free_node  max_rate   max_torque     initial_yaw    error_yaw
YawFixed      YawFree     0.5        100000.0        0.0            0.0

-- ============================ 约束 ============================

[Constraints]
-- id               type       detail...
BaseFixed           FIX_NODE   TowerBase
YawBearingCon       MATE       NODE YawFree BODY YawBearing true true true true true true
TowerTopToYaw       MATE       NODE TowerTop BODY YawBearing true true true true true true
NacelleToShaft      MATE       NODE ShaftFixed BODY Nacelle true true true true true true
ShaftToHub          MATE       NODE ShaftToHub BODY Hub true true true true true true
HubToBlade1         ROT_MOTOR  NODE HubFixed NODE Blade1PitchFixed Z
HubToBlade2         ROT_MOTOR  NODE HubFixed NODE Blade2PitchFixed Z
HubToBlade3         ROT_MOTOR  NODE HubFixed NODE Blade3PitchFixed Z
YawMotorCon         ROT_MOTOR  NODE YawFixed NODE YawFree Z
LSSMotorCon         ROT_MOTOR  NODE ShaftFixed NODE ShaftToHub X

-- ============================ 叶片结构属性文件引用 ============================
-- 兼容 QBlade .str 结构文件格式

[BladePropertyFiles]
-- blade_id   file_path
Blade1        ./structure/NREL5MW_Blade.str
Blade2        ./structure/NREL5MW_Blade.str
Blade3        ./structure/NREL5MW_Blade.str

[BladePropertyTables]  -- 或内联定义
-- blade_id   NumElements   RayleighAlpha   RayleighBeta   StiffTuner   MassTuner
Blade1        15            0.0             1.0e-4          1.0           1.0
-- LENFRACT  MASSD  EIxx  EIyy   EA     GJ     GA     STRPIT  KSX KSY  RGX RGY  XCM YCM  XCE YCE  XCS YCS
  0.000      678.3  1.81e10  1.81e10  1.76e9  5.69e9  1.27e9  13.31   0.5  0.5  0.25 0.25  0.0  0.0  0.25 0.0  0.25 0.0
  0.066      678.3  1.81e10  1.76e10  1.59e9  5.26e9  1.18e9  13.31   0.5  0.5  0.25 0.25  0.0  0.0  0.25 0.0  0.25 0.0
  ...

-- ============================ 塔筒结构属性 ============================

[TowerPropertyTable]
-- NumElements   RayleighAlpha   RayleighBeta   StiffTuner   MassTuner
20               0.0             1.0e-4          1.0           1.0
-- LENFRACT  MASSD  EIxx  EIyy   EA     GJ     GA     STRPIT  KSX KSY  RGX RGY  XCM YCM  XCE YCE  XCS YCS  DIA
  0.000      5590.0  6.14e11  6.14e11  9.41e10  4.57e11  3.63e10  0.0   0.5  0.5  0.5  0.5   0.0  0.0  0.25 0.0  0.25 0.0  6.0
  0.050      5232.0  5.34e11  5.34e11  8.12e10  3.98e11  3.15e10  0.0   0.5  0.5  0.5  0.5   0.0  0.0  0.25 0.0  0.25 0.0  5.8
  ...

-- ============================ 下部结构（可选） ============================
-- 或引用独立 .str 文件: SUBFILE ./structure/OC4_Semi.str

[SubStructure]
IS_OFFSHORE      true
IS_FLOATING      true
DESIGN_DEPTH     320.0         - m
DESIGN_DENSITY   1025.0        - kg/m^3
WAVE_KINEVAL_MOR LOCAL         - LOCAL / LAGGED / REF
WAVE_KINEVAL_POT LOCAL
WAVE_KINTAU      0.5           - s
BUOYANCY_TUNER   1.0
MASSTUNER        1.0
STIFFTUNER       1.0
SPRINGDAMPK      1.0

-- 下部结构构件
[SubElements]
-- ID  Density  Iyy  Izz  E      G       D_int  D_out  CaAx  CdAx  CpAx  CaTg  CdTg  CpTg  CdTg2  ScF  ScM  CdF  CdM  CdF_nG
1      7850.0   1.0  1.0  2.1e11 8.0e10  0.05   1.2    1.0   0.65  0.0   1.0   0.65  0.0   0.0    1.0  1.0  1.0  1.0  0.0

[SubJoints]
-- ID   DisplacedVolume   DragCoeffF   DragCoeffM
1       0.0               1.0          1.0

[SubMembers]
-- ID  JointA  JointB  Type  MaterialID  HydroCoeffID  JointACoeffID  JointBCoeffID  GrowthCoeffID  DiscPerM
1     1       2       1     1           1             1              1              0              0.5

-- 下部结构节点...
[SubCoordinateSystems]
-- id x y z
...

-- 系泊缆
[MoorElements]
-- ID  Density  Area  Iyy  E      Damping  Diameter
1     7850.0   0.01  1.0  2.1e11  0.0      0.2

[MoorMembers]
-- ID  AnchorNode  PlatformNode  Type(1=fixed anchor)  MaterialID  Tension  Drag   NumNodes
1      1           2             1                      1           1.0e6    1.0    10

-- 势流数据文件引用（在 SubStructure section 内）
[PotFlowFiles]
POT_RAD_FILE     ./hydro/OC4_Radiation.dat
POT_EXC_FILE     ./hydro/OC4_Excitation.dat
POT_DIFF_FILE    ./hydro/OC4_Diff_QTF.dat
POT_SUM_FILE     ./hydro/OC4_Sum_QTF.dat
POTFLOW_TYPE     WAMIT          - BEMUSE / NEMOH / WAMIT
USE_RADIATION    true
USE_EXCITATION   true
USE_DIFF_FREQS   false
USE_SUM_FREQS    false
USE_MEANDRIFT    true
USE_NEWMAN       true
DELTA_FREQ_RAD   0.02
DELTA_FREQ_DIFF  0.02
TRUNC_TIME_RAD   60.0
TRUNC_TIME_DIFF  60.0
DT_IRF           0.05

-- ============================ 附加质量点 ============================

[AddedMasses]
-- body_type   body_id   rel_position   mass
BLADE         1         0.25           50.0
BLADE         1         0.75           30.0

-- ============================ 事件文件（可选） ============================
-- 故障事件: 变桨卡死、电网丢失、制动等

[Events]
EVENT_FILE      ./events/pitch_fault.evt

-- ============================ 外力加载（可选） ============================
[ExternalLoading]
LOADING_FILE    ./loads/extreme.evl

-- ============================ 输出控制 ============================

[Outputs]
-- id               type   detail
-- 叶片节点输出
BLD_1_0.0           NODE   BLADE 1 POS 0.0
BLD_1_0.25          NODE   BLADE 1 POS 0.25
BLD_1_0.50          NODE   BLADE 1 POS 0.50
BLD_1_0.75          NODE   BLADE 1 POS 0.75
BLD_1_1.0           NODE   BLADE 1 POS 1.0
-- 塔筒输出
TWR_0.00            NODE   TOWER POS 0.00
TWR_0.50            NODE   TOWER POS 0.50
TWR_1.00            NODE   TOWER POS 1.00
-- 关键节点输出
HUB                 NODE   HUB
NACELLE             BODY   NACELLE
PLATFORM            BODY   PLATFORM
-- 输出内容控制
STORE_FORCES        true
STORE_MOMENTS       true
STORE_DEFLECTIONS   true
STORE_POSITIONS     true
STORE_VELOCITIES    true
STORE_ACCELERATIONS true
STORE_ROTATIONS     true
STORE_GLOBAL_VEL    true
STORE_GLOBAL_ACC    true
```

---

## 三、MBDL C++ 类设计

### 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/MBDL/MBDL_Type.hpp` | **新建** | 数据类型、枚举、配置结构体 |
| `src/MBDL/MBDL_Config.hpp` | **新建** | `.qmd` 文件解析 |
| `src/MBDL/MBDL_Solver.hpp` | **新建** | MBDLSolver 类声明 |
| `src/MBDL/MBDL_Solver.cpp` | **新建** | 主实现 |
| `src/MBDL/MBDL_Components.cpp` | **新建** | Chrono 组件创建（梁/刚体/约束/传动链） |
| `src/MBDL/MBDL_SubStructure.cpp` | **新建** | 下部结构 + 系泊 |
| `src/MBDL/MBDL_Loads.cpp` | **新建** | 载荷施加 |
| `src/MBDL/MBDL_Modal.cpp` | **新建** | 模态分析 |
| `src/MBDL/IO/MBDL_IO.hpp` | **新建** | `.qmd` 序列化 |

### 核心类接口

```cpp
namespace MBDL {

enum class Timestepper { HHT, NEWMARK, TRAPEZOIDAL, EULER_IMPLICIT_LINEARIZED, EULER_IMPLICIT_PROJECTED, EULER_IMPLICIT };
enum class SolverType { SPARSE_LU, SPARSE_QR };

struct MBDLConfig {
    // [Simulation]
    double startTime = 0, duration = 0, timeStep = 0;
    double gravity = 9.80665;
    int outputEvery = 1, chronoThreads = 1;
    bool autoGravity = true;
    SolverType solver = SolverType::SPARSE_LU;
    Timestepper timestepper = Timestepper::HHT;
    int integrationIters = 1;
    std::string outputDir, outputName;
    
    // [GlobalParams]
    bool isVAWT = false;
    int numBlades = 3;
    double overHang, preCone, shaftTilt, twr2Shft, hubHeight;
    double globalGeomEps = 0.001;
    
    // Structural data containers (from .str files or inline tables)
    // ...
};

class MBDLSolver {
public:
    // ── 生命周期 ──
    MBDLSolver();
    ~MBDLSolver();
    
    bool ParseConfigFile(const std::string& qmdPath);
    bool AssembleModel();
    
    // ── 仿真控制 ──
    bool Initialize();
    bool Relax(double dt, int steps);      // 初始松弛
    bool Advance(double dt);               // 单步推进
    
    // ── 状态查询 ──
    MBDLState GetState() const;
    
    // ── 载荷施加 ──
    void ApplyAeroPanelLoads(const std::vector<AeroPanelLoad>& loads);
    void ApplyAeroDistributedLoads(const std::vector<AeroDistributedLoad>& loads);
    void ApplyHydroLoads(const std::vector<HydroLoad>& loads);
    void ApplyPotentialFlowLoads(const Vec6& force);
    void ClearElementForces();
    
    // ── 变形几何查询（供 AeroL 使用）──
    CoordSys GetDeformedBladeCoordSys(int bladeIdx, double normPos) const;
    CoordSys GetDeformedHubCoordSys() const;
    CoordSys GetDeformedTowerCoordSys(double normPos) const;
    CoordSys GetDeformedPlatformCoordSys() const;
    
    // ── 塔影效应查询 ──
    double GetTowerRadiusAt(const Vec3& pos) const;
    double GetTowerDragCoeffAt(const Vec3& pos) const;
    
    // ── 模态分析 ──
    struct ModalResult {
        std::vector<double> freqHz, dampingRatios;
        std::vector<std::vector<CoordSys>> modeShapes;
    };
    ModalResult SolveModes(int nModes, double minFreq = 0.01);
    
    // ── 结果输出 ──
    void CalcResults(double tStart);
    void WriteOutput();
    
private:
    // Chrono 系统
    std::unique_ptr<chrono::ChSystemNSC> m_system;
    std::shared_ptr<chrono::ChLoadContainer> m_loadContainer;
    std::shared_ptr<chrono::fea::ChMesh> m_mesh;
    
    // 构件列表
    std::vector<BeamBody> m_beams;       // 塔筒/叶片/扭力管
    std::vector<RigidBody> m_rigids;     // 机舱/轮毂/偏航轴承
    std::vector<CableBody> m_cables;     // 系泊缆
    std::vector<Connector> m_connectors; // 约束/铰链
    
    // 传动系统
    std::unique_ptr<DriveTrainAssembly> m_drivetrain;
    std::vector<std::unique_ptr<PitchDrive>> m_pitchDrives;
    std::unique_ptr<YawDrive> m_yawDrive;
    std::unique_ptr<Brake> m_brake;
    
    // 下部结构
    std::unique_ptr<SubStructure> m_subStructure;
    
    // 载荷容器
    std::vector<std::shared_ptr<ChLoadWrenchAero>> m_aeroLoads;
    
    // 状态缓存
    MBDLState m_cachedState;
    double m_omega = 0;
    double m_azimuth = 0;
    double m_azimuthIncrement = 0;
};
}
```

---

## 四、QBlade 源码映射

| MBDL 实现文件 | QBlade 源方法 |
|--------------|--------------|
| `MBDL_Config.hpp` | `StrModel::ReadMainInputFile` + `ReadStrModelMultiFiles` |
| `MBDL_Solver.cpp::AssembleModel` | `StrModel::AssembleModel` → `CreateTurbineCoordinates` + `CreateBodiesHAWT/VAWT` + `CreateDriveTrainAndPitchDrives` + `CreateStrNodesAndBeamElements` |
| `MBDL_Solver.cpp::Advance` | `StrModel::AdvanceSingleStep` → `PreAdvanceSingleStep` + `CalculateChronoDynamics` + `PostAdvanceSingleStep` |
| `MBDL_Loads.cpp` | `StrModel::ApplyExternalForcesAndMoments` → `AddAtomicAeroLoads` + `AddDistributedAeroLoads` |
| `MBDL_SubStructure.cpp` | `StrModel::SUBSTRUCTURE_*` 系列方法 |
| `MBDL_Modal.cpp` | `StrModel::SolveEigenvalueProblem` + `SortModalFrequencies` + `NormalizeModeshapes` |

---

## 五、实现步骤

1. **Step 1**: 实现 `MBDL_Type.hpp`（所有数据类型和枚举）
2. **Step 2**: 实现 `MBDL_Config.hpp`（`.qmd` 解析 + QBlade `.str` 文件兼容解析）
3. **Step 3**: 实现 Chrono 系统初始化（`ChSystemNSC` + 求解器配置）
4. **Step 4**: 实现梁/刚体/约束创建（塔筒 + 叶片 + 传动链）
5. **Step 5**: 实现载荷施加接口（`ApplyAeroPanelLoads` 等）
6. **Step 6**: 实现时域推进（`Advance`）
7. **Step 7**: 实现变形几何查询（供 AeroL 获取结构状态）
8. **Step 8**: 实现模态分析
9. **Step 9**: 实现下部结构 + 系泊（可选）
10. **Step 10**: 单元测试 + NREL 5MW 塔筒单机验证
