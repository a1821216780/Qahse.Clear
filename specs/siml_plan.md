# SimL 模块重构计划与输入文件设计

## 一、概述

SimL (Simulation Library) 是 Qahse 的顶层调度模块，整合 WindL, WaveL, AeroL, MBDL, HydroL 五个模块，推进完整的风力机气动-水动-伺服-弹性时域仿真。

**源参考**: `QBlade/src/QSimulation/QSimulation.cpp` + `QTurbineSimulationData.cpp`

---

## 二、SimL 输入文件格式 (`.qsi`)

`.qsi` 文件采用分段式 INI 风格，引用各模块的输入文件路径，集中管理仿真配置。

```ini
-- =============================================================================
-- Qahse.SimL NREL 5MW DLC 1.1 (Normal Turbulence) 仿真定义
-- =============================================================================

[Simulation]
NREL5MW_DLC1_1      OBJECTNAME

-- 模块文件引用
./wind/NTM_11ms.qwd             WIND_FILE
./wave/Hs3.5_Tp10.5.qoe         WAVE_FILE
./nrel5mw/NREL5MW_Main.qmd      TURBINE_FILE
./nrel5mw/NREL5MW_Aero.qad      AERO_FILE
./hydro/hydro_semi.qhd          HYDRO_FILE
./control/discon.dll            CONTROLLER_DLL
./control/DISCON.IN             CONTROLLER_PARAM

-- 时间设置
0.01                TIMESTEP            - s
66000               NUM_TIMESTEPS       - 660s @ 0.01s
1                   SUBSTEPS_AERO       - 每结构步的气动子步数
30.0                RAMPUP_TIME         - s, 结构松弛斜坡
10.0                ADDDAMP_TIME        - s, 附加阻尼时间(后续)
2.0                 ADDDAMP_FACTOR      - 附加阻尼因子
4                   CHRONO_THREADS      - Chrono 并行线程数

-- 风场设置
WINDFIELD           WIND_TYPE           - UNIFORM / WINDFIELD / HUBHEIGHT
0.0                 HORIZONTAL_ANGLE   - deg, 水平入流角
0.0                 VERTICAL_ANGLE     - deg, 垂直入流角
MIRROR              STRETCH_STITCH     - 风场周期/Mirror 模式
ON                  AUTO_SHIFT         - 风场自动平移
0.0                 SHIFT_TIME         - s, 手动平移时间
0.0                 REFERENCE_HEIGHT   - m, 参考高度 (-1=hub height)
0.2                 SHEAR_EXP          - 剪切幂指数
0.0                 DIRECTIONAL_SHEAR  - deg/m

-- 波浪设置
OFF                 OFFSHORE           - 是否包含水动力
NONE                STITCH_TYPE        - 波浪拼接: NONE / PERIODIC

-- 控制器设置
BLADED              CONTROLLER_TYPE    - NONE / BLADED / DTU / TUB
./control/discon.dll  CONTROLLER_DLL_PATH
./control/DISCON.IN   CONTROLLER_PARAM_PATH

-- 流体属性
1.225               DENSITYAIR         - kg/m^3
1.647e-05           VISCOSITYAIR       - m^2/s
1025.0              DENSITYWATER       - kg/m^3
1.307e-06           VISCOSITYWATER     - m^2/s
9.80665             GRAVITY            - m/s^2

-- ============================ 初始条件 ============================

[InitialConditions]
-- TURBINE_1 的初始状态
0.0                 INITIAL_YAW         - deg
0.0                 INITIAL_PITCH       - deg (集体变桨)
0.0                 INITIAL_AZIMUTH     - deg
12.1                INITIAL_RPM         - rpm
0.0                 GLOBAL_X            - m
0.0                 GLOBAL_Y            - m
0.0                 GLOBAL_Z            - m

-- 浮式平台初始状态（仅 OFFSHORE=ON 时生效）
0.0                 FLOAT_SURGE         - m
0.0                 FLOAT_SWAY          - m
0.0                 FLOAT_HEAVE         - m
0.0                 FLOAT_ROLL          - deg
0.0                 FLOAT_PITCH         - deg
0.0                 FLOAT_YAW           - deg

-- RPM 控制模式
RAMPUP              PRESCRIBE_TYPE      - RAMPUP / WHOLE_SIM / NONE
12.1                RPMPRESCRIBED       - rpm
0                   STRSUBSTEPS         - 结构子步数(0=自动)
10                  RELAXSTEPS          - 松弛步数

-- 时间积分
HHT                 TINTEGRATOR         - HHT / EI_LIN / EI_PRO / EI
1                   STRITERATIONS       - 隐式积分迭代次数

-- ============================ 存储控制 ============================

[StoreControl]
ON                  STORE_REPLAY        - 启用回放数据存储
30.0                STORE_FROM          - s, 从此时开始存储
ON                  STORE_AERO          - 存储气动数据
ON                  STORE_STRUCT        - 存储结构数据
ON                  STORE_CONTROLLER    - 存储控制器数据
ON                  STORE_HYDRO         - 存储水动力数据
OFF                 STORE_WAKE          - 存储尾迹快照
10                  WAKE_STORE_EVERY    - 每N步存一次尾迹

-- ============================ 输出控制 ============================

[OutputControl]
./result            OUTPUT_DIR
NREL5MW_DLC1_1      OUTPUT_NAME
ON                  WRITE_SUMMARY       - 写摘要 .sum
ON                  WRITE_TIMESERIES    - 写时域结果 .csv
OFF                 WRITE_VTU           - 写 VTK 快照
ON                  WRITE_HAWC2        - 写 HAWC2 .sel 兼容格式

-- ============================ 模态分析（仿真后） ============================

[ModalAnalysis]
ON                  CALC_MODAL
0.01                MIN_FREQ            - Hz
0.01                DELTA_FREQ          - Hz, 频率分辨间隔
10                  NUM_MODES           - 提取模态数
OFF                 EXPORT_MODE_SHAPES  - 导出振型

-- ============================ 多风机配置（可选） ============================

[MultiTurbine]
2                   NUM_TURBINES
ON                  WAKE_INTERACTION    - 尾迹相互干扰
1.0                 INTERACTION_START   - s, 开始干扰计算时间

-- TURBINE_1 (上游, 主机)
[TURBINE_1]
./nrel5mw/NREL5MW_T1.qmd    STRUCT_FILE        - 可选覆盖主文件
./nrel5mw/NREL5MW_T1.qad    AERO_FILE
0.0                 GLOBAL_X
0.0                 GLOBAL_Y
0.0                 INITIAL_YAW

-- TURBINE_2 (下游, 从机)
[TURBINE_2]
./nrel5mw/NREL5MW_T2.qmd    STRUCT_FILE
./nrel5mw/NREL5MW_T2.qad    AERO_FILE
800.0               GLOBAL_X
0.0                 GLOBAL_Y
30.0                INITIAL_YAW

-- 共用水动力（下部结构）
[SHARED_HYDRO]
./hydro/hydro_semi.qhd      HYDRO_FILE

-- 事件文件
[Events]
./events/pitch_fault.evt
```

---

## 三、SimL C++ 类设计

### 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/SimL/SimL_Type.hpp` | **新建** | 枚举、配置结构体、结果结构体 |
| `src/SimL/SimL_Config.hpp` | **新建** | `.qsi` 解析 + `.sim` 兼容导入 |
| `src/SimL/SimL_Runner.hpp` | **新建** | SimRunner 声明 |
| `src/SimL/SimL_Runner.cpp` | **新建** | 主仿真循环 |
| `src/SimL/SimL_Control.cpp` | **新建** | 控制器 DLL 接口 |
| `src/SimL/SimL_Output.cpp` | **新建** | 结果输出 (CSV/HAWC2/VTU) |
| `src/SimL/IO/SimL_IO.hpp` | **新建** | 序列化 |

### 核心类接口

```cpp
namespace SimL {

struct TurbineInstance {
    std::string turbineFile;   // .qmd
    std::string aeroFile;      // .qad
    std::string hydroFile;     // .qhd (可能多个 turbine 共享)
    std::string controllerDll, controllerParam;
    
    // 初始条件
    double initialYaw, initialPitch, initialAzimuth, initialRpm;
    double globalX, globalY, globalZ;
    double floatSurge, floatSway, floatHeave;  // floater ICs
    double floatRoll, floatPitch, floatYaw;
    
    // 控制
    enum PrescribeType { RAMPUP, WHOLE_SIM, NONE } prescribeType = RAMPUP;
    double prescribedRpm = 0;
    int structuralRelaxationSteps = 10;
    
    // 运行时对象
    std::unique_ptr<MBDL::MBDLSolver> mbd;
    std::unique_ptr<AeroL::AeroLSolver> aero;
};

struct SimLConfig {
    std::string objectName;
    
    // 全局文件
    std::string windFile, waveFile;
    
    // 时间
    double timestep, rampUpTime, addDampTime, addDampFactor;
    int numTimesteps, substepsAero = 1;
    int chronoThreads = 4;
    
    // 风场
    bool offshore = false;
    WindL::WindType windType = WindL::WindType::WINDFIELD;
    double horizAngle = 0, vertAngle = 0;
    bool mirrorWindfield = true, autoShift = true;
    double shiftTime = 0;
    
    // 波浪
    WaveL::WaveStretching waveStretching;
    
    // 流体
    double airDensity = 1.225, kinViscAir = 1.647e-05;
    double waterDensity = 1025.0, kinViscWater = 1.307e-06;
    double gravity = 9.80665;
    
    // 存储
    bool storeReplay = true, storeAero = true, storeStruct = true;
    bool storeController = true, storeHydro = false, storeWake = false;
    double storeFrom = 0;
    int wakeStoreEvery = 10;
    
    // 输出
    std::string outputDir, outputName;
    bool writeSummary = true, writeTimeseries = true;
    bool writeVTU = false, writeHAWC2 = false;
    
    // 模态
    bool calcModal = true;
    double minFreq = 0.01, deltaFreq = 0.01;
    int numModes = 10;
    bool exportModeShapes = false;
    
    // 多风机
    std::vector<TurbineInstance> turbines;
    bool wakeInteraction = false;
    double interactionStart = 1.0;
    
    // 事件
    std::string eventFile;
};

struct SimulationResult {
    // 时间轴
    std::vector<double> time;
    
    // 每风机数据
    struct TurbineResult {
        // 转子
        std::vector<double> rotorSpeed, rotorThrust, rotorTorque, rotorPower;
        std::vector<double> Ct, Cp, TSR;
        // 发电机
        std::vector<double> genSpeed, genTorque, genPower;
        // 变桨/偏航
        std::vector<double> pitchAngle, pitchRate, yawAngle, yawRate;
        // 叶片根弯矩
        std::vector<double> blade1RootMx, blade1RootMy;
        std::vector<double> blade2RootMx, blade2RootMy;
        std::vector<double> blade3RootMx, blade3RootMy;
        // 塔筒
        std::vector<double> towerBaseMx, towerBaseMy, towerTopDx, towerTopDy;
        // 机舱
        std::vector<double> nacelleAccX, nacelleAccY;
        // 浮体 (offshore)
        std::vector<double> platformSurge, platformSway, platformHeave;
        std::vector<double> platformRoll, platformPitch, platformYaw;
        // 控制器
        std::vector<double> genTorqueCmd, pitchCmd;
    };
    std::vector<TurbineResult> turbineResults;
    
    // 波浪 (if offshore)
    std::vector<double> waveElevation;
    
    // 模态
    struct ModalData {
        std::vector<double> freqHz, dampingRatios;
    };
    std::vector<ModalData> modalData;  // per turbine
    
    // 文件名
    std::string csvPath, sumPath, selPath;
};

class SimRunner {
public:
    SimRunner();
    ~SimRunner();
    
    bool ParseConfigFile(const std::string& qsiPath);
    bool Initialize();
    
    // ── 执行仿真 ──
    SimulationResult Run();
    
    // ── 进度回调 ──
    using ProgressCallback = std::function<void(int step, int total, const std::string& status)>;
    void SetProgressCallback(ProgressCallback cb);
    
private:
    // --- 初始化 ---
    bool loadWindField();
    bool loadWaveField();
    bool initTurbines();
    void initOutputVectors();
    
    // --- 主循环 ---
    void simulationLoop();
    void precompRampUp();
    
    // --- 单步 ---
    void stepWind(int step);
    void stepWave(int step);
    void stepAero(int turbineIdx);
    void stepHydro(int turbineIdx);
    void stepStructure(int turbineIdx);
    void stepController(int turbineIdx);
    
    // --- 多风机协调 ---
    void updateWakeInteraction();
    
    // --- 存储 ---
    void storeStep(int step);
    void writeOutput();
    void writeSummary();
    void writeTimeseriesCSV();
    void writeHAWC2Output();
    
    // --- 后处理 ---
    void runModalAnalysis();
    
    SimLConfig m_config;
    
    // 模块实例
    std::unique_ptr<WindL::WindField> m_windField;
    std::unique_ptr<WaveL::WaveField> m_waveField;
    std::vector<TurbineInstance> m_turbines;
    
    // 进度
    ProgressCallback m_progressCb;
    
    // 存储缓冲区
    struct StorageBuffer {
        std::vector<std::vector<double>> channels;
        std::vector<std::string> channelNames;
    };
    std::vector<StorageBuffer> m_buffers;  // per turbine
    
    // 运行状态
    double m_currentTime = 0;
    int m_currentStep = 0;
    bool m_isRampUp = true;
};
}
```

---

## 四、仿真主循环

```cpp
void SimRunner::simulationLoop() {
    PrecomputationRampUp: for (step = 0; step < relaxSteps; ++step) {
        mbd[i]->Relax(dt, 1);  // 静态松弛
    }
    
    for (int step = 0; step < m_config.numTimesteps; ++step) {
        // 1. 更新时间
        m_currentTime = step * m_config.timestep;
        
        // 2. 推进风场/波浪（如有外部数据流）
        stepWind(step);
        stepWave(step);
        
        // 3. 对每个风机
        for (int i = 0; i < m_turbines.size(); ++i) {
            // 3a. 从 MBDL 获取结构状态
            MBDL::MBDLState state = m_turbines[i].mbd->GetState();
            
            // 3b. 调用控制器
            stepController(i);
            
            // 3c. AeroL 计算气动力
            AeroL::AeroLInput aeroInput;
            aeroInput.windSampler = [this](const Vec3& p, double t) { return m_windField->getWindSpeed(p, t); };
            aeroInput.bladeCoords = state.bladeCoords;
            aeroInput.bladeVelocity = state.bladeVelocities;
            aeroInput.omega = state.omega;
            aeroInput.azimuth = state.azimuth;
            // ... 填充其他
            AeroL::AeroLOutput aeroOutput = m_turbines[i].aero->Step(aeroInput);
            
            // 3d. HydroL 计算水动力 (if offshore)
            MBDL::MBDLLoads loads;
            if (m_config.offshore) {
                HydroL::HydroLInput hydroInput;
                // ... 填充
                HydroL::HydroLOutput hydroOutput = m_turbines[i].hydro->Step(hydroInput);
                // 转换为 MBDLLoads 格式
                LoadsFromHydro(hydroOutput, loads);
            }
            
            // 3e. 转换气动力为 MBDLLoads
            LoadsFromAero(aeroOutput, loads);
            
            // 3f. 施加到 MBDL + 推进结构
            m_turbines[i].mbd->ClearElementForces();
            m_turbines[i].mbd->ApplyLoads(loads);
            m_turbines[i].mbd->Advance(m_config.timestep);
        }
        
        // 4. 多风机尾迹交叉干扰
        if (m_config.wakeInteraction && m_currentTime >= m_config.interactionStart) {
            updateWakeInteraction();
        }
        
        // 5. 存储结果
        if (m_currentTime >= m_config.storeFrom) {
            storeStep(step);
        }
        
        // 6. 进度
        if (m_progressCb && step % 100 == 0) {
            m_progressCb(step, m_config.numTimesteps, "Simulating...");
        }
    }
    
    // 后处理
    runModalAnalysis();
    writeOutput();
}
```

---

## 五、控制器 DLL 接口

```cpp
// 支持三种控制器类型
enum class ControllerType { NONE, BLADED(DISCON), DTU, TUB };

// BLADED/DISCON 交换数组接口
struct ControllerSWAP {
    // avrSWAP[0..ANEMCOUNT-1]: 风速测量
    // avrSWAP[ANEMCOUNT]: 发电机转速
    // avrSWAP[ANEMCOUNT+1]: 发电机功率
    // ...
    static constexpr int SIZE = 550;  // arraySizeBLADED
    
    std::array<float, SIZE> fromSim;   // Sim → Controller
    std::array<float, SIZE> toSim;     // Controller → Sim
    
    void packInput(double genSpeed, double genTorque, double genPower,
                   double pitchAngle, double yawError, 
                   const std::vector<double>& bladeRootMoment1,
                   const std::vector<double>& bladeRootMoment2);
    void unpackOutput(double& genTorqueCmd, double& pitchCmd, double& yawCmd);
};

// DLL 动态加载接口
class ControllerDLL {
public:
    bool Load(const std::string& dllPath);
    void Call(const ControllerSWAP& swap);
    void Unload();
    
private:
#ifdef _WIN32
    HMODULE m_hDll = nullptr;
#else
    void* m_hDll = nullptr;
#endif
    using DISCONFunc = void(*)(float*, float*, char*, char*, char*);
    DISCONFunc m_discon = nullptr;
};
```

---

## 六、QBlade 源码映射

| SimL 实现文件 | QBlade 源方法 |
|--------------|--------------|
| `SimL_Runner.cpp::Run` | `QSimulation::onStartAnalysis` (line 697) |
| `SimL_Runner.cpp::simulationLoop` | `QSimulation::onStartAnalysis` 主 while 循环 (line 721) |
| `SimL_Runner.cpp::stepStructure` | `QTurbineSimulationData::AdvanceSimulation` (line 1844) |
| `SimL_Runner.cpp::stepController` | `QTurbineSimulationData::CallTurbineController` + `CalcActuatorInput` |
| `SimL_Runner.cpp::stepAero` | QSimulation 调用 `gammaBoundFixedPointIteration` + `wakeCalculations` |
| `SimL_Control.cpp` | `QControl::QControl` + `TurbineInputs` |
| `SimL_Output.cpp` | QBlade 存储/导出逻辑 |
| `SimL_Config.hpp` | `ImportExport::ImportSimulationDefinition` (line 1220) |

---

## 七、结果输出格式

### 7.1 主 CSV 时域结果

`{OutputDir}/{OutputName}.csv` — 每列一个通道，第一行为通道名。

```
Time, RotorSpeed, RotorThrust, RotorTorque, GenPower, Pitch, ...
0.00, 12.100, 0.000, 0.0, 0.0, 0.00, ...
0.01, 12.101, 1.234, 0.1, 0.0, 0.00, ...
```

### 7.2 摘要文件

`{OutputDir}/{OutputName}.sum` — 关键统计信息。

```
OBJECTNAME: NREL5MW_DLC1_1
DURATION: 660.0 s
TIMESTEP: 0.010 s
WIND: NTM 11.4 m/s (Kaimal, IEC Ed3)
WAVE: None
MEAN_ROTOR_SPEED: 12.1 rpm
MEAN_GENERATOR_POWER: 5215.3 kW
MAX_BLADE_ROOT_MX: 12560.2 kNm
MAX_TOWER_BASE_MY: 85320.1 kNm
...
MODAL_FREQUENCIES: [0.32, 0.48, 0.62, 0.85, 1.12, 1.45] Hz
```

### 7.3 HAWC2 兼容输出 (`.sel`)

二进制格式，用于与 HAWC2 结果对比验证。

---

## 八、实现步骤

1. **Step 1**: `SimL_Type.hpp` — 所有数据结构
2. **Step 2**: `SimL_Config.hpp` — `.qsi` 解析 + QBlade `.sim` 兼容导入
3. **Step 3**: `SimL_Control.cpp` — 控制器 DLL 加载 (DISCON/DLL)
4. **Step 4**: `SimL_Runner.cpp` — 主仿真循环骨架（无气动/水动力）
5. **Step 5**: `SimL_Runner.cpp` — AeroL + MBDL 耦合
6. **Step 6**: `SimL_Runner.cpp` — HydroL 耦合
7. **Step 7**: `SimL_Runner.cpp` — 多风机尾迹干扰
8. **Step 8**: `SimL_Output.cpp` — CSV/SUM 输出
9. **Step 9**: `SimL_Output.cpp` — HAWC2 兼容输出
10. **Step 10**: 集成测试 (NREL 5MW 陆上 + OC3 Spar 浮式)
