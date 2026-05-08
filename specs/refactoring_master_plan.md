# QBlade → Qahse 计算核心重构计划

## 一、模块架构总览

```
┌───────────────────────────────────────────────────────────────────┐
│                        SimL (仿真调度层)                            │
│  时域推进 · 多模块耦合 · 状态存储 · 结果输出                         │
└────┬───────────┬───────────┬───────────┬───────────┬──────────────┘
     │           │           │           │           │
 ┌───▼───┐  ┌───▼───┐  ┌───▼───┐  ┌───▼───┐  ┌───▼───┐
 │ AeroL │  │ MBDL  │  │ HydroL│  │ WindL │  │ WaveL │
 │ 气动力│  │ 结构  │  │ 水动力│  │ 风场  │  │波浪场 │
 └───┬───┘  └───┬───┘  └───┬───┘  └───────┘  └───────┘
     │          │          │
     │     ┌────▼────┐     │
     │     │ Chrono  │     │
     │     └─────────┘     │
     └──────────┬──────────┘
                │
           载荷返还(力/力矩)
```

### 模块职责边界

| 模块 | 职责 | 输入 | 输出 |
|------|------|------|------|
| **WindL** | 湍流风场生成与时空采样 | `.qwd` 定义文件 | 3D风场速度场 `V(x,y,z,t)` |
| **WaveL** | 波浪谱生成与运动学计算 | `.qod`/`.qoe` 定义文件 | 波面高程、水质点速度/加速度/动压 |
| **MBDL** | Chrono 多体动力学建模与时域推进 | `.qmd` 结构定义 + AeroL/HydroL 载荷 | 结构节点位移/速度/加速度/坐标系统 |
| **AeroL** | BEM/FVW/VPML 气动力计算 | WindL 风场 + MBDL 结构状态 | 气动力/力矩载荷 → MBDL |
| **HydroL** | Morison+势流理论水动力计算 | WaveL 波浪 + MBDL 结构状态 | 水动力/力矩载荷 → MBDL |
| **SimL** | 多模块耦合时域推进 + 结果输出 | 所有模块输入文件 + 控制器DLL | 仿真结果数据文件 |

### 数据流方向

```
WindL ──(Vwind)──► AeroL ◄──(pos,vel,acc)──► MBDL
                                          ▲
WaveL ──(η,Vw,aw)──► HydroL ──(Fh,Mh)─────┘
```

---

## 二、各模块函数归类（从 QBlade 源码提取）

### 2.1 WindL — ✅ 已实现

**源目录**: `Qahse.Clear/src/WindL/`  
**QBlade 参考**: `src/Windfield/WindField.cpp` (Veers 方法, TurbSim I/O)  
**状态**: 完成，支持 Kaimal/Mann/von Karman 等模型

---

### 2.2 WaveL — ✅ 基本实现，需清理

**源目录**: `Qahse.Clear/src/WaveL/`  
**QBlade 参考**: `src/Waves/LinearWave.cpp` (1222行)  
**状态**: 基本完成（JONSWAP/Torsethaugen/Ochi-Hubble 谱生成 + 运动学），需统一格式

---

### 2.3 MBDL — 待实现

**源文件**: `QBlade/src/StructModel/StrModel.cpp` (~14000行) + `StrObjects.h/cpp` + `StrElem.h/cpp` + `StrNode.h/cpp`

| 功能域 | QBlade 函数 | 源行号 |
|--------|------------|--------|
| **模型组装** | | |
| 主输入文件读取 | `ReadMainInputFile` | `StrModel.cpp:7799` |
| 叶片结构数据读取 | `ReadBladeData` | `StrModel.cpp:9337` |
| 塔筒结构数据读取 | `ReadTowerData` | `StrModel.cpp:9980` |
| 下部结构数据读取 | `SUBSTRUCTURE_ReadSubStructureData` | `StrModel.cpp:10254` |
| 系泊缆数据读取 | `ReadCableData` | `StrModel.cpp:9500` |
| 扭力管数据读取 (VAWT) | `ReadTorquetubeData` | `StrModel.cpp:10888` |
| 坐标系统建立 | `CreateTurbineCoordinates` + `CreateCoordinateSystemsHAWT/VAWT` | `StrModel.cpp` |
| 模型组装 | `AssembleModel` → `InitializeModel` | `StrModel.cpp` |
| **Chrono 组件创建** | | |
| HAWT 体创建 | `CreateBodiesHAWT` | `StrModel.cpp` |
| VAWT 体创建 | `CreateBodiesVAWT` | `StrModel.cpp` |
| 传动链 + 变桨 | `CreateDriveTrainAndPitchDrives` | `StrModel.cpp` |
| FEA 梁单元 | `CreateStrNodesAndBeamElements` | `StrModel.cpp` |
| ANCF 缆绳 | `CreateStrNodesAndCableElements` | `StrModel.cpp` |
| 下部结构刚体 | `SUBSTRUCTURE_CreateChBody` | `StrModel.cpp` |
| 系泊系统 | `SUBSTRUCTURE_CreateMoorings` | `StrModel.cpp` |
| **载荷传递** | | |
| 面板级气动力 | `AddAtomicAeroLoads` | `StrModel.cpp:3872` |
| 分布式气动力 | `AddDistributedAeroLoads` | `StrModel.cpp:3826` |
| 综合外力 | `ApplyExternalForcesAndMoments` | `StrModel.cpp:3941` |
| 气动 Chrono 载荷容器 | `CreateAerodynamicChLoads` | `StrModel.cpp` |
| **时域推进** | | |
| 推进到目标时间 | `AdvanceToTime` | `StrModel.cpp` |
| 单步推进 | `AdvanceSingleStep` | `StrModel.cpp` |
| 单步前/后处理 | `PreAdvanceSingleStep` / `PostAdvanceSingleStep` | `StrModel.cpp` |
| Chrono 求解器 | `CalculateChronoDynamics` | `StrModel.cpp` |
| 边界条件 | `SetBoundaryConditionsAndControl` | `StrModel.cpp` |
| 模型松弛 | `RelaxModel` | `StrModel.cpp` |
| **模态分析** | | |
| 特征值求解 | `SolveEigenvalueProblem` | `StrModel.h:129` |
| 模态排序 | `SortModalFrequencies` | `StrModel.h:169` |
| 模态变换到局部坐标 | `ConvertModesToLocalCoords` | `StrModel.h:170` |
| **状态查询** | | |
| 变形后叶片坐标 | `GetDeformedBeamCoordSystem` | `StrModel.h:226` |
| 变形后轮毂坐标 | `GetDeformedHubCoordSystem` | `StrModel.h:232` |
| 变形后塔筒坐标 | `GetDeformedTowerCoordSystem` | `StrModel.h:229` |
| **辅助** | | |
| 叶片交叉截面插值 | `InterpolateProperties` | `StrModel.h:194` |
| 质量/惯量计算 | `CalcMassAndInertiaInfo` | `StrModel.h:159` |
| 结果计算 | `CalcResults` | `StrModel.h:160` |

---

### 2.4 AeroL — 待实现

**源文件**: `QBlade/src/QTurbine/QTurbineSimulationData.cpp` + `src/VortexObjects/` + `src/VPML/`

| 功能域 | QBlade 函数 | 源行号 |
|--------|------------|--------|
| **BEM 求解器** | | |
| Gamma 迭代主体 | `gammaBoundFixedPointIteration` | `QTurbineSimulationData.cpp:4112` |
| 面板速度计算 | `calcBladePanelVelocities` | `QTurbineSimulationData.cpp:4695` |
| Cl/Cd 读取 | `calcSteadyBladePanelCoefficients` | `QTurbineSimulationData.cpp:4754` |
| 环量计算 | `calcBladeCirculation` | `QTurbineSimulationData.cpp:4781` |
| 非定常BEM诱导 | `calcUnsteadyBEMInduction` | `QTurbineSimulationData.cpp` |
| 收敛判断 | `gammaConvergenceCheck` | `QTurbineSimulationData.cpp` |
| BEM 加速 | `m_BEMspeedUp` 初值推进 | — |
| 塔影效应 | `calcTowerInfluence` | 调用自 `calcBladePanelVelocities` |
| **动态失速** | | |
| OYE 模型 | `calcDynamicBladeCoefficients` (DS=OYE) | `QTurbineSimulationData.cpp` |
| Gormont-Berg 模型 | `calcDynamicBladeCoefficients` (DS=GORMONT) | 同上 |
| ATEFlap 模型 | `calcDynamicBladeCoefficients` (DS=ATEFLAP) | 同上 |
| **FVW 尾迹演化** | | |
| 尾迹计算调度 | `wakeCalculations` | `QSimulation.cpp` |
| 尾迹诱导速度 | `calculateWakeInduction` (Biot-Savart) | `QTurbineSimulationData.cpp` |
| 叶片间诱导 | `calculateBladeInduction` | `QTurbineSimulationData.cpp` |
| 生成尾迹行 | `addWakeElements` | `QSimulation.cpp` |
| 尾迹状态积分 | `calculateNewWakeState` (Euler/PC/PC2B) | `QSimulation.cpp` |
| 线元→粒子 | `convertLinesToParticles` | `QSimulation.cpp` |
| 尾迹截断/压缩 | `truncateWake` / `reduceWake` | `QSimulation.cpp` |
| **VPML** | | |
| 全套VPML求解器 | `VPML/` + `VortexObjects/` (15+ files) | — |
| **力矢量合成** | | |
| 气动力矢量 | `VortexPanel::CalcAerodynamicVectors` | `VortexPanel.cpp:153` |
| 攻角计算 | `VortexPanel::CalcAoA` | `VortexPanel.cpp` |
| 极值查询 | `CBlade::getBladeParameters` | `Blade.cpp` |
| 风场采样 | `getFreeStream` | `QTurbineSimulationData.cpp:2483` |

---

### 2.5 HydroL — 待实现

**源文件**: `QBlade/src/StructModel/StrModel.cpp` (水动力部分) + `StrElem.cpp`

| 功能域 | QBlade 函数 | 位置 |
|--------|------------|------|
| **Morison 方程** | | |
| 缆绳 Morison 力 | `StrElem::AddCableMorisonForces` | `StrElem.cpp` |
| 梁单元 Morison 力 | `StrElem::AddMorisonForces` | `StrElem.cpp` |
| 浮力 | `StrElem::AddBuoyancy` | `StrElem.cpp` |
| 单元海况评估 | `StrElem::EvaluateSeastateElementQuantities` | `StrElem.cpp` |
| 波浪运动学位置更新 | `SUBSTRUCTURE_UpdateWaveKinPositions` | `StrModel.cpp` |
| 单元海况分配 | `SUBSTRUCTURE_AssignElementSeaState` | `StrModel.cpp` |
| 水动力系数分配 | `SUBSTRUCTURE_AssignHydrodynamicCoefficients` | `StrModel.cpp` |
| **势流理论** | | |
| 辐射 IRF 卷积 | `POTFLOW_Radiation_IRF` | `StrModel.cpp` |
| BEMuse 数据读取 | `POTFLOW_ReadBEMuse` | `StrModel.cpp` |
| NEMOH 数据读取 | `POTFLOW_ReadNemoh` | `StrModel.cpp` |
| WAMIT 数据读取 | `POTFLOW_ReadWamit` | `StrModel.cpp` |
| WAMIT DIFF QTF 读取 | `POTFLOW_ReadWamit_DIFF_QTF` | `StrModel.cpp` |
| WAMIT SUM QTF 读取 | `POTFLOW_ReadWamit_SUM_QTF` | `StrModel.cpp` |
| 辐射力计算 | `POTFLOW_CalcRadiationForces` | `StrModel.cpp` |
| 绕射力计算 | `POTFLOW_CalcDiffractionForces` | `StrModel.cpp` |
| 二阶力 (QTF) | `POTFLOW_CalcSecondOrder_Forces` | `StrModel.cpp` |
| 平均漂移力 | `POTFLOW_CalculateMeanDriftForces` | `StrModel.cpp` |
| 阻尼系数插值 | `POTFLOW_InterpolateDampingCoefficients` | `StrModel.cpp` |
| 激励系数插值 | `POTFLOW_InterpolateExcitationCoefficients` | `StrModel.cpp` |
| 二阶系数插值 | `POTFLOW_Interpolate2ndOrderCoefficients` | `StrModel.cpp` |

---

### 2.6 SimL — 待实现

**源文件**: `QBlade/src/QSimulation/QSimulation.cpp` + `QTurbineSimulationData.cpp`

| 功能域 | QBlade 函数 | 源行号 |
|--------|------------|--------|
| 主仿真循环 | `onStartAnalysis` | `QSimulation.cpp:697` |
| 边界条件 | `setBoundaryConditions` | `QSimulation.cpp` |
| 转子几何更新 | `updateRotorGeometry` | `QSimulation.cpp` |
| 单步推进 | `advanceSimulation` | `QSimulation.cpp:526` |
| 预计算仿真 | `precomTurbineSimulation` | `QSimulation.cpp` |
| 控制器调用 | `CallTurbineController` | `QTurbineSimulationData.cpp` |
| 执行器输入 | `CalcActuatorInput` | `QTurbineSimulationData.cpp` |
| 数据存储 | `storeSimulationData` | `QSimulation.cpp` |
| 仿真初始化 | `initializeStructuralModels` | `QSimulation.cpp` |
| GUI 更新 | `updateGUI` | `QSimulation.cpp` |

---

## 三、模块间 C++ 接口设计

### 3.1 WindL 接口 (已有)

```cpp
namespace WindL {
    struct WindField {
        int ny, nz, nSteps;
        double dy, dz, dt;
        double hubHeight, meanWindSpeed;
        std::array<double, 3> mean, sigma, turbulenceIntensity;
        // 查询接口
        Vec3 getWindSpeed(const Vec3& pos, double time) const;
    };
    
    SimWindResult Generate(const WindLInput& input, ProgressCB progress);
    WindField Import(const WindLInput& input, ProgressCB progress);
}
```

### 3.2 WaveL 接口 (已有)

```cpp
namespace WaveL {
    struct WaveField {
        // 查询接口
        double GetElevation(const Vec3& pos, double time) const;
        void GetVelocityAndAcceleration(const Vec3& pos, double time,
              double elevation, double depth, int stretchType,
              Vec3* vel, Vec3* acc, double* dynP) const;
        double GetDepth() const;
        const WaveSpectrumResult& GetResult() const;
    };
}
```

### 3.3 MBDL 接口 (新)

```cpp
namespace MBDL {
    struct MBDLState {
        // 叶片 (per panel radial position)
        QVector<CoordSys> bladeCoords;     // 每个 panel 位置
        QVector<Vec3>     bladeVelocities;
        QVector<Vec3>     bladeAccelerations;
        QVector<Vec3>     bladePanelPositions;
        // 塔筒
        QVector<CoordSys> towerCoords;
        QVector<Vec3>     towerVelocities;
        QVector<Vec3>     towerAccelerations;
        // 关键节点
        CoordSys hubCoords, nacelleCoords, platformCoords;
        Vec3     hubVel, hubAcc;
        // 标量
        double azimuth, omega, omega_dot;
        std::array<double, MAX_BLADES> pitch_angle, pitch_rate;
        double yaw_angle, yaw_rate;
        double genTorque, brakeTorque;
    };

    struct MBDLLoads {
        struct BladePanelLoad {
            int bladeIdx, panelIdx;
            Vec3 force;       // 全局坐标系力
            Vec3 moment;       // 全局坐标系力矩
            Vec3 forceDerivAlpha;  // 对攻角的导数(切线刚度)
        };
        QVector<BladePanelLoad> aeroPanelLoads;
        
        struct DistributedLoad {
            int bodyType, bodyIdx, elementIdx;
            Vec3 forcePerLengthA, forcePerLengthB;  // 端部力/长度
            Vec3 momentPerLengthA, momentPerLengthB;
            Vec3 dForcePerLengthA_dAlpha, dForcePerLengthB_dAlpha;
            Vec3 dMomentPerLengthA_dAlpha, dMomentPerLengthB_dAlpha;
        };
        QVector<DistributedLoad> aeroDistributedLoads;
        
        struct HydroLoad {
            int bodyType, bodyIdx, elementIdx;
            Vec3 force, moment;
        };
        QVector<HydroLoad> hydroLoads;
        
        Vec3 potentialFlowForce;  // 6-DOF势流力
        Vec3 potentialFlowMoment;
    };

    class MBDLSolver {
    public:
        bool ParseConfig(const std::string& qmdPath);
        bool AssembleModel();
        
        bool Relax(double dt, int steps);
        bool Advance(double dt);
        
        MBDLState GetState() const;
        void ApplyLoads(const MBDLLoads& loads);
        
        // 模态分析
        struct ModalResult {
            QVector<double> freqHz, dampingRatio;
            QVector<QVector<CoordSys>> modeShapes;
        };
        ModalResult SolveModes(int nModes, double minFreq);
        
        // 塔影效应查询
        double GetTowerRadius(const Vec3& pos) const;
        double GetTowerDragCoeff(const Vec3& pos) const;
    };
}
```

### 3.4 AeroL 接口 (新)

```cpp
namespace AeroL {
    struct AeroLConfig {
        std::string bladeFile;          // .bld 路径
        QVector<std::string> polarFiles; // 极值文件列表
        // 模型选择
        enum WakeType { FVW, U_BEM, VPML } wakeType;
        enum DSType  { NONE, OYE, GORMONT, ATEFLAP } dynStallType;
        enum WakeIntegration { EULER, PC, PC2B } wakeInteg;
        enum WakeConvection { LOCALMEAN, HHMEAN, LOCALTURB } convection;
        // 参数
        int numPanels, discType;
        double coreRadius, coreRadiusBound, vortexViscosity;
        double gammaRelaxation, gammaConvergence;
        int maxIterations;
        bool tipLoss, himmelskamp, towerShadow, twoPointLiftDrag;
        double towerDragCoeff;
        double tf_oye, am_gb, tf_ate, tp_ate;
        // 尾迹
        int maxWakeElements;
        double maxWakeDist, firstWakeRowLen;
        double nearWakeLength, zone1Len, zone2Len, zone3Len;
        int zone1Factor, zone2Factor, zone3Factor;
        // 流体
        double airDensity, kinViscAir;
    };

    struct AeroLInput {
        // 来自 WindL
        std::function<Vec3(const Vec3&, double)> windSampler;
        // 来自 MBDL
        QVector<CoordSys> bladeCoords;
        QVector<Vec3>     bladeVelocity;
        QVector<Vec3>     bladeAccel;
        double omega, azimuth;
        std::function<double(const Vec3&)> towerRadiusSampler;
        std::function<double(const Vec3&)> towerDragSampler;
    };

    struct AeroLOutput {
        struct PanelResult {
            Vec3 force, moment;
            Vec3 force_dAlpha, moment_dAlpha;
            double CL, CD, CM, AoA, Re, Gamma;
        };
        QVector<PanelResult> panelResults;
        
        // 分布式载荷（用于 MBDL 的 DistributedLoad）
        QVector<Vec3> forcePerLength;    // Cn, Ct 分量
        QVector<Vec3> dForcePerLength_dAlpha;
        
        double rotorThrust, rotorTorque, rotorPower;
        
        // 尾迹可视化
        struct VortexLine { Vec3 start, end; double gamma; };
        QVector<VortexLine> wakeLines;
    };

    class AeroLSolver {
    public:
        bool Initialize(const AeroLConfig& config);
        AeroLOutput Step(const AeroLInput& input, double dt);
        
        // 控制器调用 (BLADED/DISCON style)
        struct ControllerInput {
            double genSpeed, genTorque, genPower, rotorSpeed;
            double pitchAngle, yawError;
            QVector<double> bladeRootMoments;
        };
        ControllerInput GetControllerInput() const;
        
    private:
        // BEM
        void gammaBoundFixedPointIteration();
        void calcBladePanelVelocities();
        void calcSteadyBladePanelCoefficients();
        void calcBladeCirculation();
        void calcDynamicBladeCoefficients();
        bool gammaConvergenceCheck();
        // FVW
        void wakeCalculations(double dt);
        Vec3 calculateWakeInduction(const Vec3& pt);
        void calculateBladeInduction();
        // 极值查询
        QVector<double> getBladeParameters(double relPos, double AoA75, double windspeed);
    };
}
```

### 3.5 HydroL 接口 (新)

```cpp
namespace HydroL {
    struct HydroLConfig {
        double waterDepth, waterDensity, gravity;
        enum WaveStretching { NONE, VERTICAL, WHEELER, EXTRAPOLATION } stretching;
        enum WaveKinEval { LOCAL, REF, LAGGED } kinEval;
        double kinTau;  // 滤波时间常数
        
        // Morison 系数
        struct MorisonCoeff {
            int elementId;
            double Ca, Cd, Cp, diameter;
            double displacedVolume;
            double Cd_nG;  // 法向阻力（用于粗糙度修正）
        };
        QVector<MorisonCoeff> morisonCoeffs;
        
        // 势流系数文件
        std::string radFile, excFile, diffQtfFile, sumQtfFile;
        double deltaFreqRad, deltaFreqDiff, deltaDirDiff;
        double truncTimeRad, truncTimeDiff, dtIrf;
        bool useRadiation, useExcitation, useSumFreq, useDiffFreq;
        bool useNewmanApprox, useMeanDrift;
    };

    struct HydroLInput {
        // 来自 WaveL
        std::function<double(const Vec3&, double)> elevationSampler;
        std::function<void(const Vec3&, double, double, double, int, Vec3*, Vec3*, double*)> kinematicsSampler;
        // 来自 MBDL 参考点
        QVector<Vec3> structurePositions;   // 单元中点位置
        QVector<Vec3> structureVelocities;
        QVector<Vec3> structureAccelerations;
        Vec3 platformPosition;              // 用于势流
        Vec3 platformVelocity;
        double time;
    };

    struct HydroLOutput {
        QVector<Vec3> morisonForces;
        QVector<Vec3> morisonMoments;
        QVector<Vec3> buoyancyForces;
        // 势流力（6-DOF）
        QVector<double> radiationForces;    // [surge,sway,heave,roll,pitch,yaw]
        QVector<double> diffractionForces;
        QVector<double> sumForces, diffForces;
        QVector<double> meanDriftForces;
    };

    class HydroLSolver {
    public:
        bool Initialize(const HydroLConfig& config);
        HydroLOutput Step(const HydroLInput& input);
    };
}
```

### 3.6 SimL 接口 (新)

```cpp
namespace SimL {
    struct SimLConfig {
        std::string windFile;      // .qwd
        std::string waveFile;      // .qod
        std::string turbineFile;   // .qmd
        std::string aeroFile;      // .qad
        std::string hydroFile;     // .qhd
        std::string controllerDll;
        std::string controllerParamFile;
        
        double timestep;
        int numTimesteps;
        double rampUpTime, addDampTime, addDampFactor;
        
        // 初始条件
        QVector<double> initialYaw, initialPitch, initialAzimuth;
        QVector<Vec3> turbinePositions;  // 多风机
        
        // 存储控制
        bool storeAero, storeStruct, storeController, storeHydro;
        double storeFrom;
        
        // 模态分析
        bool calcModal;
        double minFreq, deltaFreq;
    };

    struct SimulationResult {
        // 时间序列
        QVector<double> time;
        QVector<double> rotorSpeed, rotorThrust, rotorTorque, rotorPower;
        QVector<double> generatorSpeed, generatorPower, generatorTorque;
        QVector<double> pitchAngle, yawAngle, yawError;
        QVector<double> bladeRootMx, bladeRootMy;
        QVector<double> towerBaseMx, towerBaseMy, towerTopDx;
        // 波浪
        QVector<double> waveElevation;
        // 模态
        QVector<double> modalFreqs, modalDampingRatios;
    };

    class SimRunner {
    public:
        bool Initialize(const SimLConfig& config);
        SimulationResult Run();
        
    private:
        std::unique_ptr<MBDLSolver>  mbd;
        std::unique_ptr<AeroLSolver> aero;
        std::unique_ptr<HydroLSolver> hydro;
        WindL::WindField windField;
        WaveL::WaveField waveField;
        
        void simulationLoop();
        void storeStep(int step);
        void callController();
    };
}
```

---

## 四、重构顺序

### 依赖关系图

```
Phase 0: 基础设施 ─────────────────────────────────────────────┐
  (IO/Math/CoordSys/Vec3)                                      │
                                                               │
Phase 1: WindL ✅                                              │
Phase 2: WaveL (清理) ──┐                                      │
                         │                                     │
Phase 3: MBDL ──────────┤  (可并行 3 & 4)                     │
Phase 4: AeroL           │                                     │
                         │                                     │
Phase 5: HydroL ────────┘  (依赖 WaveL)                        │
                                                               │
Phase 6: SimL ─── 依赖 MBDL + AeroL + HydroL                  │
                                                               │
Phase 7: 集成测试 ─── 端到端 NREL 5MW 验证                     │
```

### 详细阶段计划

| 阶段 | 模块 | 工作内容 | 预估工作量 | 依赖 |
|------|------|---------|-----------|------|
| **Phase 0** | 基础设施 | 完善 `CoordSys` 类, `CoordSys` ↔ `Eigen::Matrix4d` 转换, 补充 `Math/` 插值工具 | 小 | 无 |
| **Phase 1** | WindL | ✅ 已完成 | — | — |
| **Phase 2** | WaveL | 清理现有 WaveL 代码，统一 `.qoe` 格式，补全 Wheeler 拉伸、深水/有限水深运动学 | 中 | — |
| **Phase 3** | MBDL | 从 `StrModel.cpp` 抽取核心：<br>① FEA梁/刚体/缆绳 Chrono 创建<br>② 载荷施加接口<br>③ 时域推进 + 预条件<br>④ 模态分析<br>⑤ 坐标系统查询<br>⑥ 兼容 QBlade `.str` 文件 | 大 | — |
| **Phase 4** | AeroL | 从 `QTurbineSimulationData.cpp` 抽取：<br>① BEM 求解器 + 动态失速<br>② FVW 尾迹演化<br>③ VPML 求解器<br>④ 极值数据库（360°Polar）<br>⑤ 气动力矢量合成 | 大 | — |
| **Phase 5** | HydroL | 从 `StrModel.cpp` + `StrElem.cpp` 抽取：<br>① Morison 方程<br>② 势流辐射/绕射力 (BEMuse/NEMOH/WAMIT)<br>③ 二阶 QTF 力<br>④ 浮力 | 中 | WaveL |
| **Phase 6** | SimL | 整合全部模块：<br>① 主仿真循环<br>② 模块间数据流<br>③ 控制器 DLL 加载<br>④ 多风机协调<br>⑤ 结果 I/O | 中 | MBDL+AeroL+HydroL |
| **Phase 7** | 集成测试 | NREL 5MW 基准 + OC3/OC4 浮式风机验证 | 中 | 全部 |

---

## 五、关键技术决策

### 5.1 Qt 剥离策略

QBlade 中的 Qt 类型统一替换：

| Qt 类型 | 替换为 |
|---------|--------|
| `QString` | `std::string` |
| `QList<T>` / `QVector<T>` | `std::vector<T>` 或 `Eigen::Matrix<T>` |
| `QStringList` | `std::vector<std::string>` |
| `QMap<K,V>` | `std::unordered_map<K,V>` |
| `Vec3` (QBlade 自定义) | `Qahse::Math::Vec3` (已有) |

### 5.2 Chrono 版本

- QBlade 使用 Chrono 6.x, Qahse.Clear `include/chrono/` 需确认版本匹配
- 使用 Chrono 的 `ChSystemNSC`, `ChElementBeamEuler`, `ChLoadWrench` 等

### 5.3 格式兼容

- `.bld` 叶片文件：**完全兼容** QBlade 格式
- `.plr` 极值文件：**完全兼容** QBlade 格式
- `.str` 结构文件：**完全兼容** QBlade 格式（MBDL 子文件）
- `.qmd` MBDL 主文件：**新格式**（扩展 QBlade 结构输入）
- `.qad` AeroL 文件：**新格式**
- `.qhd` HydroL 文件：**新格式**
- `.qsi` SimL 文件：**新格式**

### 5.4 并行优先级

- **Phase 3 (MBDL)** 和 **Phase 4 (AeroL)** 可完全并行开发（无数据依赖）
- Phase 5 (HydroL) 可与 Phase 4 并行，但依赖 WaveL 完成

---

## 六、风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Chrono 版本不匹配 | MBDL 无法运行 | 先验证 `include/chrono/` 版本与 QBlade 的 Chrono 兼容性 |
| 360°极值数据库逻辑复杂 | AeroL BEM 无法正确工作 | 先以 `Polar360::getBladeParameters` 为核心，完整迁移极值插值逻辑 |
| 控制器 DLL ABI 差异 | SimL 无法调用外部控制器 | 先实现 TUB/DTU 无外部 DLL 模式，再对接 DISCON |
| QBlade 代码量大(~14K行 StrModel) | 抽取耗时长 | 按功能域分步抽取，每次抽取后写单元测试 |
| 浮式风机 + 势流 QTF 实现复杂 | HydroL 势流功能不稳定 | 先实现 Morison 部分，势流留到 Phase 5 B |
