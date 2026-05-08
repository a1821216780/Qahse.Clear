# Chrono 7.0.0 → 10.0.0 API 迁移对照表

## 概述

QBlade 2.0.4 使用 Project Chrono 7.0.0 API，Qahse.Clear 的 `include/chrono/` 为 Chrono 10.0.0。本文档对照两个版本中 MBDL 用到的全部 API，标注变更类型。

**变更类型**: 🔴=破坏性, 🟡=接口变更, 🟢=兼容, 🔵=新增替代

---

## 一、核心类型

### 1.1 向量类型 🔴

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChVector<>` | `ChVector3d` | 默认 double 精度。10.0 改为模板 `ChVector3<T>`，`ChVector3d` = `ChVector3<double>` |
| `ChVector<int>` | `ChVector3i` | 整数向量。`ChVector3i` = `ChVector3<int>` |
| `ChVector<float>` | `ChVector3f` | 单精度。`ChVector3f` = `ChVector3<float>` |
| `vec.x` / `vec.y` / `vec.z` | `vec.x()` / `vec.y()` / `vec.z()` | 10.0 改为访问器方法（只读），写入用 `vec.x() = val` |
| `ChVector<>(x,y,z)` | `ChVector3d(x,y,z)` | 构造函数语法相同 |

**迁移模板**:
```cpp
// 7.0.0
ChVector<> pos(1.0, 2.0, 3.0);
pos.x = 5.0;

// 10.0.0
ChVector3d pos(1.0, 2.0, 3.0);
pos.x() = 5.0;  // 或 SetX(5.0)
```

---

### 1.2 矩阵类型 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChMatrix33<>` | `ChMatrix33d` | `ChMatrix33<double>` 别名 |
| `ChMatrixDynamic<>` | `ChMatrixDynamic<double>` | 动态矩阵，接口相同 |

---

### 1.3 四元数 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChQuaternion<>` | `ChQuaterniond` | `ChQuaternion<double>` 别名 |
| `quad.Q_from_NasaAngles(...)` | `quad.SetFromEuler(...)` | 方法名可能变化，需验证 |
| `quad.Q_to_Euler123()` | `quad.GetEuler(...)` | 同上 |
| `!quad` (共轭) | `quad.GetConjugate()` | 运算符重载可能变化 |
| `quad1 * quad2` | `quad1 * quad2` | ✅ 乘法兼容 |
| `quad.Normalize()` | `quad.Normalize()` | ✅ 兼容 |

---

### 1.4 坐标系/Frame 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChFrame<>` | `ChFramed` | `ChFrame<double>` 别名 |
| `ChCoordsys<>` | `ChCoordsysd` | `ChCoordsys<double>` 别名 |
| `frame.GetPos()` | `frame.GetPos()` | ✅ 兼容 |
| `frame.GetRot()` / `GetA()` | `frame.GetRot()` / `GetRotMat()` | `GetA()` 可能改名 |

---

### 1.5 动态向量/矩阵 🟢

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChVectorDynamic<>` | `ChVectorDynamic<double>` | ✅ 兼容，接口相同 |
| `ChMatrixDynamic<>` | `ChMatrixDynamic<double>` | ✅ 兼容 |
| `ChSparseMatrix` | `ChSparseMatrix` | ✅ 兼容 |
| `F.segment(0, 3)` | `F.segment(0, 3)` | ✅ 兼容（Eigen 桥接） |
| `.eigen()` | `.eigen()` | ✅ 兼容 |

---

### 1.6 常量 🔴

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `chrono::VNULL` | `VNULL` | 仍可用，但推荐 `ChVector3d(0)` |
| `chrono::CH_C_PI` | `CH_PI` | 改名 |
| `chrono::CH_C_2PI` | `CH_2PI` | 改名 |

---

## 二、物理系统

### 2.1 系统类 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChSystemNSC` | `ChSystemNSC` | ✅ 类名不变 |
| `new ChSystemNSC` | `new ChSystemNSC` | ✅ 兼容 |
| `system->Add(body)` | `system->Add(body)` | ✅ 兼容 |
| `system->DoStepDynamics(dt)` | `system->DoStepDynamics(dt)` | ✅ 兼容 |
| `system->DoStaticRelaxing()` | `system->DoStaticRelaxing()` | ✅ 兼容（但推荐用 `ChStaticNonLinearAnalysis`） |
| `system->DoFullAssembly()` | `system->DoFullAssembly()` | ✅ 兼容 |
| `system->Update()` | `system->Update()` | ✅ 兼容 |
| `system->Set_G_acc(v)` | `system->SetGravitationalAcceleration(v)` | 🟡 方法名变更 |
| `system->GetChTime()` | `system->GetChTime()` | ✅ 兼容 |
| `system->GetAssembly().Get_bodylist()` | `system->GetBodies()` | 🔴 接口变更 → 返回 `std::vector<ChBody*>` |
| `system->GetTimestepperType()` | `system->GetTimestepperType()` | ✅ 兼容 |
| `system->GetTimestepper()` | `system->GetTimestepper()` | ✅ 兼容 |
| `system->SetSolver(solver)` | `system->SetSolver(solver)` | ✅ 兼容 |
| `system->SetNumThreads(1,1,n)` | `system->SetNumThreads(n)` | 🟡 参数简化 |

---

### 2.2 ChBody 🔴

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChBody` | `ChBody` | ✅ 类名不变 |
| `body->GetPos()` | `body->GetPos()` | ✅ 兼容 |
| `body->SetPos(pos)` | `body->SetPos(pos)` | ✅ 兼容 |
| `body->GetRot()` | `body->GetRot()` | ✅ 兼容 |
| `body->SetRot(quat)` | `body->SetRot(quat)` | ✅ 兼容 |
| `body->GetA()` | `body->GetRotMat()` | 🟡 返回旋转矩阵 |
| `body->GetPos_dt()` | `body->GetLinVel()` | 🔴 方法名变更 |
| `body->GetPos_dtdt()` | `body->GetLinAcc()` | 🔴 方法名变更 |
| `body->GetWvel_loc()` | `body->GetAngVelLocal()` | 🔴 方法名变更 |
| `body->GetWacc_loc()` | `body->GetAngAccLocal()` | 🔴 方法名变更 |
| `body->SetPos_dt(v)` | `body->SetLinVel(v)` | 🔴 方法名变更 |
| `body->SetPos_dtdt(a)` | `body->SetLinAcc(a)` | 🔴 方法名变更 |
| `body->SetWvel_loc(w)` | `body->SetAngVelLocal(w)` | 🔴 方法名变更 |
| `body->SetWacc_loc(wa)` | `body->SetAngAccLocal(wa)` | 🔴 方法名变更 |
| `body->SetBodyFixed(bool)` | `body->SetFixed(bool)` | 🔴 方法名变更 |
| `body->GetBodyFixed()` | `body->IsFixed()` | 🔴 方法名变更 |
| `body->SetMass(m)` | `body->SetMass(m)` | ✅ 兼容 |
| `body->SetInertiaXX(iner)` | `body->SetInertia(ChVector3d(ix,iy,iz))` | 🔴 方法名变更 |
| `body->SetInertiaXY(iner)` | `body->SetInertia(ChMatrix33d(...))` | 🔴 需用完整惯性矩阵 |
| `body->GetMass()` | `body->GetMass()` | ✅ 兼容 |
| `body->GetInertia()` | `body->GetInertia()` | ✅ 兼容 |
| `body->SetInitialPosition(pos)` | `body->SetInitialPosition(pos)` | ✅ 兼容 |
| `body->SetInitialRotation(quat)` | `body->SetInitialRotation(quat)` | ✅ 兼容 |
| `body->GetCoord_dt().pos.eigen()` | `body->GetLinVel().eigen()` | 🔴 简化 |
| `body->Amatrix` | 改为通过 `GetRotMat()` 访问 | 🔴 不再直接 public |
| `body->ComputeGyro()` | 转向内部处理 | 🔴 |

---

### 2.3 ChLoadContainer 🟢

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChLoadContainer` | `ChLoadContainer` | ✅ 兼容 |
| `container->Add(load)` | `container->Add(load)` | ✅ 兼容 |

---

## 三、求解器

### 3.1 直接求解器 🟢

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChSolverSparseLU` | `ChSolverSparseLU` | ✅ 类名不变 |
| `solver->SetVerbose(false)` | `solver->SetVerbose(false)` | ✅ 兼容 |
| `solver->UsePermutationVector(true)` | `solver->UsePermutationVector(true)` | ✅ 兼容 |
| `solver->LeverageRhsSparsity(true)` | `solver->LeverageRhsSparsity(true)` | ✅ 兼容 |
| `solver->ForceSparsityPatternUpdate()` | `solver->ForceSparsityPatternUpdate()` | ✅ 兼容 |

---

### 3.2 特征值求解 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChSparseMatrix` (参数) | `ChSparseMatrix` | ✅ 兼容 |
| `ChMatrixDynamic<complex<double>>` | `ChMatrixDynamic<complex<double>>` | ✅ 兼容 |
| `ChVectorDynamic<double>` | `ChVectorDynamic<double>` | ✅ 兼容 |
| `ChDirectSolverLS` (基类) | `ChDirectSolverLS` | ✅ 兼容 |
| `ChSolverSparseComplexLU` | 新增 | 🔵 10.0 有专门的复数求解器 |
| `ChSolverSparseComplexQR` | 新增 | 🔵 |

---

## 四、约束/Link

### 4.1 ChLinkMate 系列 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChLinkMateFix` | `ChLinkMateFix` | ✅ 类名不变 |
| `ChLinkMateGeneric` | `ChLinkMateGeneric` | ✅ 类名不变 |
| `link->Initialize(n1, n2)` | `link->Initialize(n1, n2)` | ✅ 大部分兼容 |
| `link->Initialize(body, node)` | `link->Initialize(body, node)` | ✅ 兼容 |
| `link->Get_react_force()` | `link->GetReaction1().force` | 🔴 接口变更 |
| `link->Get_react_torque()` | `link->GetReaction1().torque` | 🔴 接口变更 |
| `link->GetLinkAbsoluteCoords()` | `link->GetFrameRefToAbs()` | 🔴 接口变更 |
| `link->GetLinkAbsoluteCoords().rot.GetXaxis()` | 通过 `GetFrameRefToAbs().GetRotMat()` 获取 | 🔴 接口变更 |
| `link->SetDisabled(bool)` | `link->SetDisabled(bool)` | ✅ 兼容 |
| `link->SetConstrainedCoords(...)` | `link->SetConstrainedCoords(...)` | ✅ 兼容 |

**关键变更——反力获取**:
```cpp
// 7.0.0
ChVector<> force = link->Get_react_force();
ChVector<> torque = link->Get_react_torque();
ChVector<> x_axis = link->GetLinkAbsoluteCoords().rot.GetXaxis();

// 10.0.0
struct ChLinkMateFix::ReactionForce force = link->GetReaction1();
ChVector3d force_vec = force.force;
ChVector3d torque_vec = force.torque;
ChMatrix33d rot_matrix = link->GetFrameRefToAbs().GetRotMat();
ChVector3d x_axis = rot_matrix.GetAxisX();
```

---

## 五、传动系统 (Shaft)

### 5.1 ChShaft 🟢

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChShaft` | `ChShaft` | ✅ 兼容 |
| `shaft->SetPos(d)` | `shaft->SetPos(d)` | ✅ 兼容 |
| `shaft->SetPos_dt(d)` | `shaft->SetPos_dt(d)` | ✅ 兼容 |
| `shaft->SetPos_dtdt(d)` | `shaft->SetPos_dtdt(d)` | ✅ 兼容 |
| `shaft->GetPos()` | `shaft->GetPos()` | ✅ 兼容 |
| `shaft->GetPos_dt()` | `shaft->GetPos_dt()` | ✅ 兼容 |
| `shaft->GetPos_dtdt()` | `shaft->GetPos_dtdt()` | ✅ 兼容 |
| `shaft->SetInertia(I)` | `shaft->SetInertia(I)` | ✅ 兼容 |

---

### 5.2 ChShaftsMotor 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChShaftsMotor` | `ChShaftsMotor` | ✅ 兼容 |
| *子类拆分*: | | |
| `ChShaftsMotor` (7.x 全功能) | `ChShaftsMotor` (基类) | 🔴 某些子功能移到子类 |
| | `ChShaftsMotorPosition` | 🔵 位置控制 |
| | `ChShaftsMotorSpeed` | 🔵 速度控制 |
| | `ChShaftsMotorTorque` | 🔵 转矩控制 |
| | `ChShaftsMotorLoad` | 🔵 负载模式 |

**迁移 7.0.0 的 `SetMotorMode`**:
```cpp
// 7.0.0
motor->SetMotorMode(ChShaftsMotor::eCh_shaftsmotor_mode::MOT_MODE_ROTATION);
motor->SetMotorRot(target_rot);

// 10.0.0 — 使用 ChShaftsMotorPosition
auto motor_pos = chrono_types::make_shared<ChShaftsMotorPosition>();
motor_pos->Initialize(shaft1, shaft2);
motor_pos->SetMotorFunction(chrono_types::make_shared<ChFunctionConst>(target_rot));
// 或直接设置
motor_pos->SetAngle(target_rot);

// 转矩模式
auto motor_torque = chrono_types::make_shared<ChShaftsMotorTorque>();
motor_torque->Initialize(shaft1, shaft2);
motor_torque->SetMotorFunction(chrono_types::make_shared<ChFunctionConst>(torque_val));
```

**枚举迁移**:
```cpp
// 7.0.0
ChShaftsMotor::eCh_shaftsmotor_mode::MOT_MODE_ROTATION
ChShaftsMotor::eCh_shaftsmotor_mode::MOT_MODE_TORQUE

// 10.0.0 — 不再使用枚举，改用子类:
// ChShaftsMotorPosition → 位置控制 (替代 MOT_MODE_ROTATION)
// ChShaftsMotorTorque   → 转矩控制 (替代 MOT_MODE_TORQUE)
// ChShaftsMotorSpeed    → 速度控制
```

**转矩/位置读取**:
```cpp
// 7.0.0
motor->GetTorqueReactionOn1()
motor->GetMotorRot()
motor->GetMotorRot_dt()
motor->GetMotorRot_dtdt()

// 10.0.0 — 取决于子类，通常:
motor->GetMotorTorque()
motor->GetMotorAngle()
motor->GetMotorSpeed()
motor->GetMotorAcc()
```

---

### 5.3 其他 Shaft 类 🟢

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChShaftsTorsionSpring` | `ChShaftsTorsionSpring` | ✅ 兼容 |
| `ChShaftsBody` | `ChShaftBodyRotation` | 🔴 类名变更，功能拆分 |
| `ChShaftsTorque` | `ChShaftsTorque` | ✅ 兼容 |
| `ChShaftsClutch` | `ChShaftsClutch` | ✅ 兼容 |
| `ChShaftsGear` | `ChShaftsGear` | ✅ 兼容 |

**ChShaftsBody → ChShaftBodyRotation 迁移**:
```cpp
// 7.0.0
auto sb = chrono_types::make_shared<ChShaftsBody>();
sb->Initialize(shaft, body, direction);

// 10.0.0
auto sb = chrono_types::make_shared<ChShaftBodyRotation>();
sb->Initialize(shaft, body, direction);
```

---

## 六、FEA 有限元

### 6.1 FEA 节点 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChNodeFEAxyzrot` | `ChNodeFEAxyzrot` | ✅ 类名不变 |
| `ChNodeFEAxyzD` | `ChNodeFEAxyzD` | ✅ 类名不变 |
| `ChNodeFEAbase` | `ChNodeFEAbase` | ✅ |

**节点公共接口** (适用于所有 ChNode 子类):

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `node->GetPos()` | `node->GetPos()` | ✅ 兼容 |
| `node->SetPos(v)` | `node->SetPos(v)` | ✅ 兼容 |
| `node->GetPos_dt()` | `node->GetPosDt()` | 🔴 方法名变更（下划线→驼峰） |
| `node->GetPos_dtdt()` | `node->GetPosDt2()` | 🔴 |
| `node->GetRot()` | `node->GetRot()` | ✅ 兼容 |
| `node->SetRot(q)` | `node->SetRot(q)` | ✅ 兼容 |
| `node->GetWvel_loc()` | `node->GetAngVelLocal()` | 🔴 |
| `node->GetWacc_loc()` | `node->GetAngAccLocal()` | 🔴 |
| `node->SetForce(v)` | `node->SetForce(v)` | ✅ 兼容 |
| `node->GetForce()` | `node->GetForce()` | ✅ 兼容 |
| `node->SetTorque(v)` | `node->SetTorque(v)` | ✅ 兼容 |
| `node->GetTorque()` | `node->GetTorque()` | ✅ 兼容 |
| `node->SetMass(m)` | `node->SetMass(m)` | ✅ 兼容 |
| `node->GetMass()` | `node->GetMass()` | ✅ 兼容 |
| `node->Frame()` | `node->Frame()` | ✅ 兼容 |
| `node->Relax()` | `node->Relax()` | ✅ 兼容 |
| `node->SetD(v)` | `node->SetD(v)` (ChNodeFEAxyzD) | ✅ 兼容 |
| `node->GetD()` | `node->GetD()` | ✅ 兼容 |
| `node->SetX0(v)` | `node->SetX0(v)` | ✅ 兼容 |

**坐标/Frame 访问**:

| Chrono 7.0.0 | Chrono 10.0.0 |
|-------------|-------------|
| `coord.pos.eigen()` | `GetPos().eigen()` |
| `coord.rot.eigen()` | `GetRot().eigen()` |
| `coord_dt.pos.eigen()` | `GetPosDt().eigen()` |
| `coord_dtdt.pos.eigen()` | `GetPosDt2().eigen()` |

---

### 6.2 FEA 梁单元 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChElementBeamEuler` | `ChElementBeamEuler` | ✅ 类名不变 |
| `ChElementCableANCF` | `ChElementCableANCF` | ✅ 类名不变 |
| `elem->EvaluateSectionFrame(U, displ, rot)` | `elem->EvaluateSectionFrame(U, displ, rot)` | ✅ 兼容 |
| `elem->EvaluateSectionForceTorque(U, force, torque)` | `elem->EvaluateSectionForceTorque(U, force, torque)` | ✅ 兼容 |
| `elem->EvaluateSectionStrain(U, strain)` | `elem->EvaluateSectionStrain(U, strain)` | ✅ 兼容 |
| `elem->GetNodeA()` | `elem->GetNodeA()` | ✅ 兼容 |
| `elem->GetNodeB()` | `elem->GetNodeB()` | ✅ 兼容 |
| `elem->GetSection()` | `elem->GetSection()` | ✅ 兼容 |
| `elem->GetCurrLength()` | `elem->GetCurrLength()` | ✅ 兼容 |

---

### 6.3 梁截面 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChBeamSectionEulerAdvancedGeneric` | `ChBeamSectionEulerAdvancedGeneric` | ✅ |
| `ChBeamSectionCable` | `ChBeamSectionCable` | ✅ |
| `sec->SetBeamRaleyghDamping(a,b)` | `sec->SetRayleighDamping(ChBeamSection::Rayleigh(a,b))` | 🔴 接口变更 |
| `sec->GetBeamRaleyghDampingBeta()` | `sec->GetRayleighDamping().beta()` | 🔴 |
| `sec->GetBeamRaleyghDampingAlpha()` | `sec->GetRayleighDamping().alpha()` | 🔴 |
| `sec->E` （Cable 截面） | `sec->GetE()` | 🔴 私有化 |
| `sec->Area` | `sec->GetArea()` | 🔴 私有化 |

---

### 6.4 FEA Mesh 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChMesh` | `ChMesh` | ✅ 类名不变 |
| `mesh->AddElement(elem)` | `mesh->AddElement(elem)` | ✅ 兼容 |
| `mesh->AddNode(node)` | `mesh->AddNode(node)` | ✅ 兼容 |
| `mesh->GetNodes()` | `mesh->GetNodes()` | ✅ 兼容 |
| `mesh->GetElements()` | `mesh->GetElements()` | ✅ 兼容 |
| `mesh->ClearElements()` | `mesh->ClearElements()` | ✅ 兼容 |
| `mesh->ClearNodes()` | `mesh->ClearNodes()` | ✅ 兼容 |
| `mesh->ResetTimers()` | `mesh->ResetTimers()` | ✅ 兼容 |
| `mesh->ResetCounters()` | `mesh->ResetCounters()` | ✅ 兼容 |
| `mesh->SetAutomaticGravity(bool)` | `mesh->SetAutomaticGravity(bool)` | ✅ 兼容 |

---

### 6.5 FEA 约束 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChLinkPointFrame` | `ChLinkNodeFrame` | 🔴 类名变更 |
| `ChLinkDirFrame` | `ChLinkNodeFrame` (generic) | 🔴 合并 |
| `ChLinkPointPoint` | `ChLinkNodeNode` | 🔴 类名变更 |

**迁移**:
```cpp
// 7.0.0 - FEA 到刚体的点约束
auto link = chrono_types::make_shared<ChLinkPointFrame>();
link->Initialize(fea_node, rigid_body);

// 10.0.0
auto link = chrono_types::make_shared<ChLinkNodeFrame>();
link->Initialize(fea_node, rigid_body);

// 7.0.0 - FEA 到 FEA 的点约束
auto link = chrono_types::make_shared<ChLinkPointPoint>();
link->Initialize(fea_node1, fea_node2);

// 10.0.0
auto link = chrono_types::make_shared<ChLinkNodeNode>();
link->Initialize(fea_node1, fea_node2);
```

---

## 七、载荷/Loader

### 7.1 载荷基类 🟢

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChLoad<ChLoaderX>` | `ChLoad<ChLoaderX>` | ✅ |
| `ChLoaderUatomic` | `ChLoaderUatomic` | ✅ |
| `ChLoaderUdistributed` | `ChLoaderUdistributed` | ✅ |
| `ChLoaderUVWatomic` | `ChLoaderUVWatomic` | ✅ |
| `ChLoadableU` | `ChLoadableU` | ✅ |
| `ChLoadableUVW` | `ChLoadableUVW` | ✅ |

---

### 7.2 自定义 Loader 迁移 🔴

**ChLoaderWrenchAero 和 ChLoadWrenchAero**（QBlade 自定义类）：

```cpp
// 7.0.0
class ChLoaderWrenchAero : public chrono::ChLoaderUatomic {
    chrono::ChVector<> torque, d_torque;
    chrono::ChVector<> force, d_force;
    chrono::ChQuaternion<> ref_rot;
    
    ChLoaderWrenchAero(std::shared_ptr<chrono::ChLoadableU> mloadable) 
        : ChLoaderUatomic(mloadable) {
        this->torque = chrono::VNULL;
        this->force = chrono::VNULL;
    }
    
    void ComputeF(double U, chrono::ChVectorDynamic<>& F, 
                  chrono::ChVectorDynamic<>* state_x, 
                  chrono::ChVectorDynamic<>* state_w) {
        chrono::ChVector<> displ;
        chrono::ChQuaternion<> rot;
        m_elem->EvaluateSectionFrame(U, displ, rot);
        chrono::ChVector<> global_force = ...rot.GetXaxis()...;
        F.segment(0, 3) = global_force.eigen();
        F.segment(3, 3) = torque.eigen();
    }
    
    void SetForce(const chrono::ChVector<>& mf) { force = mf; }
    void SetTorque(const chrono::ChVector<>& mt) { torque = mt; }
};

// 10.0.0 — 需将所有 ChVector<> → ChVector3d，VNULL → ChVector3d(0)
class ChLoaderWrenchAero : public chrono::ChLoaderUatomic {
    ChVector3d torque, d_torque;
    ChVector3d force, d_force;
    ChQuaterniond ref_rot;
    
    ChLoaderWrenchAero(std::shared_ptr<chrono::ChLoadableU> mloadable) 
        : ChLoaderUatomic(mloadable) {
        this->torque = ChVector3d(0);
        this->force = ChVector3d(0);
    }
    
    void ComputeF(double U, ChVectorDynamic<double>& F, 
                  ChVectorDynamic<double>* state_x, 
                  ChVectorDynamic<double>* state_w) override {
        ChVector3d displ;
        ChQuaterniond rot;
        m_elem->EvaluateSectionFrame(U, displ, rot);
        ChVector3d global_force = force.x() * rot.GetAxisX()
                                + force.y() * rot.GetAxisY()
                                + force.z() * rot.GetAxisZ();
        F.segment(0, 3) = global_force.eigen();
        F.segment(3, 3) = torque.eigen();
    }
    
    void SetForce(const ChVector3d& mf) { force = mf; }
    void SetTorque(const ChVector3d& mt) { torque = mt; }
};
```

**ChLoaderDistributedAero** 迁移同理。

**SpringDamperLoader**：

核心问题在于 `state_x` 和 `state_w` 中的坐标访问方式——7.0 用 `state_x->coeff(i)`，10.0 改为 `(*state_x)(i)`。

```cpp
// 7.0.0
ChVector<> pos(state_x->coeff(0), state_x->coeff(1), state_x->coeff(2));

// 10.0.0
ChVector3d pos((*state_x)(0), (*state_x)(1), (*state_x)(2));
```

---

## 八、Chrono 工厂/智能指针

### 8.1 chrono_types::make_shared 🟢

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `chrono_types::make_shared<T>()` | `chrono_types::make_shared<T>()` | ✅ 完全相同 |

---

### 8.2 工厂注册 🔴

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `CH_FACTORY_REGISTER(T)` | `CH_FACTORY_REGISTER(T)` | ✅ 宏兼容 |
| `newChBodyAddedMass()` | `chrono_types::make_shared<ChBodyAddedMass>()` | 推荐直接用 smart pointer |

---

## 九、Chrono 扩展类迁移

QBlade 自定义了三个 Chrono 扩展类，需全部迁移语法：

### 9.1 ChBodyAddedMass 🔴

```cpp
// 7.0.0
class ChBodyAddedMass : public ChBody {
    ChVariablesBodyAddedMass variables;
    ChVariables& Variables() override;
    void IntLoadResidual_F(const ChVector<>& a, ...);
    void SetMfullmass(ChMatrixDynamic<> M);
    ChMatrixDynamic<>& GetMfullmass();
};

// 10.0.0 — ChVector<> → ChVector3d
class ChBodyAddedMass : public ChBody {
    ChVariablesBodyAddedMass variables;
    ChVariables& Variables() override { return variables; }
    void IntLoadResidual_F(const ChVector3d& a, ...) override;  // 确认参数类型
    void SetMfullmass(ChMatrixDynamic<double>& M);
    ChMatrixDynamic<double>& GetMfullmass();
};
```

### 9.2 ChNodeFEAxyzrotAddedMass 🔴

```cpp
// 同样需要 ChVector<> → ChVector3d 的全局替换
// coord.pos.eigen() → GetPos().eigen() (如果 10.0 改了接口)
```

### 9.3 ChVariablesBodyAddedMass 🔴

可能需要依据 Chrono 10.0 的 `ChVariablesBodyOwnMass` 接口变化进行调整，特别是:
- `Compute_invMb_v` → 确认签名
- `Compute_inc_invMb_v` → 确认签名
- `Compute_inc_Mb_v` → 确认签名
- `MultiplyAndAdd` → 确认签名
- `Build_M` → 确认签名

---

## 十、时间积分器 🟡

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `ChTimestepper` | `ChTimestepper` | ✅ |
| `ChTimestepperHHT` | `ChTimestepperHHT` | ✅ |
| `ChTimestepper::Type::HHT` | `ChTimestepper::Type::HHT` | 🟡 枚举值可能改名 |
| `stepper->SetModifiedNewton(bool)` | `stepper->SetModifiedNewton(bool)` | ✅ |
| `dynamic_pointer_cast<ChTimestepperHHT>(...)` | `dynamic_pointer_cast<ChTimestepperHHT>(...)` | ✅ |

---

## 十一、全局替换速查表

### 符号替换

| 搜索 (7.0.0) | 替换 (10.0.0) |
|-------------|-------------|
| `ChVector<>` | `ChVector3d` |
| `ChVector<int>` | `ChVector3i` |
| `ChVector<float>` | `ChVector3f` |
| `ChQuaternion<>` | `ChQuaterniond` |
| `ChMatrix33<>` | `ChMatrix33d` |
| `ChFrame<>` | `ChFramed` |
| `ChCoordsys<>` | `ChCoordsysd` |
| `chrono::VNULL` | `ChVector3d(0.0)` |
| `chrono::CH_C_PI` | `CH_PI` |

### 方法替换

| 搜索 (7.0.0) | 替换 (10.0.0) |
|-------------|-------------|
| `vec.x` (读) | `vec.x()` |
| `vec.x = v` (写) | `vec.x() = v` |
| `.GetPos_dt()` | `.GetPosDt()` |
| `.GetPos_dtdt()` | `.GetPosDt2()` |
| `.GetWvel_loc()` | `.GetAngVelLocal()` |
| `.GetWacc_loc()` | `.GetAngAccLocal()` |
| `->Set_G_acc(v)` | `->SetGravitationalAcceleration(v)` |
| `->SetBodyFixed(b)` | `->SetFixed(b)` |
| `->GetBodyFixed()` | `->IsFixed()` |
| `->SetInertiaXX(v)` | `->SetInertia(ChMatrix33d(...))` |
| `->SetInertiaXY(v)` | `->SetInertia(ChMatrix33d(...))` |
| `->Get_react_force()` | `->GetReaction1().force` |
| `->Get_react_torque()` | `->GetReaction1().torque` |
| `->GetLinkAbsoluteCoords()` | `->GetFrameRefToAbs()` |
| `->GetLinkAbsoluteCoords().rot` | `->GetFrameRefToAbs().GetRotMat()` |
| `.rot.GetXaxis()` | `.GetRotMat().GetAxisX()` |
| `.rot.GetYaxis()` | `.GetRotMat().GetAxisY()` |
| `.rot.GetZaxis()` | `.GetRotMat().GetAxisZ()` |
| `.GetA().Get_A_Xaxis()` | `.GetRotMat().GetAxisX()` |
| `.GetA().Get_A_Yaxis()` | `.GetRotMat().GetAxisY()` |
| `.GetA().Get_A_Zaxis()` | `.GetRotMat().GetAxisZ()` |
| `node->Frame()` (赋值) | `node->SetFrame(frame)` 或 `node->Frame() = frame` (应验证) |

### 类替换

| 搜索 (7.0.0) | 替换 (10.0.0) |
|-------------|-------------|
| `ChShaftsBody` (连接 shaft-body) | `ChShaftBodyRotation` |
| (如用平移连接) | `ChShaftBodyTranslation` |
| `ChShaftsMotor` (全功能模式切换) | 拆分为 `ChShaftsMotorPosition` / `ChShaftsMotorSpeed` / `ChShaftsMotorTorque` |
| `ChLinkPointFrame` | `ChLinkNodeFrame` |
| `ChLinkDirFrame` | `ChLinkNodeFrame` (generic) |
| `ChLinkPointPoint` | `ChLinkNodeNode` |

### 枚举替换

| 7.0.0 | 10.0.0 |
|-------|--------|
| `ChShaftsMotor::eCh_shaftsmotor_mode::MOT_MODE_ROTATION` | 使用 `ChShaftsMotorPosition` 类替代 |
| `ChShaftsMotor::eCh_shaftsmotor_mode::MOT_MODE_TORQUE` | 使用 `ChShaftsMotorTorque` 类替代 |
| `ChTimestepper::Type::HHT` | 确认 10.0 枚举是否改为 `ChTimestepper::HHT` |

---

## 十二、include 路径对照

| Chrono 7.0.0 | Chrono 10.0.0 | 说明 |
|-------------|-------------|------|
| `"core/ChVector.h"` (相对路径) | `"chrono/core/ChVector3.h"` | 统一到 `chrono/` 前缀 |
| `"physics/ChLinkMate.h"` | `"chrono/physics/ChLinkMate.h"` | |
| `"chrono/physics/ChSystemNSC.h"` | `"chrono/physics/ChSystemNSC.h"` | ✅ 一致 |
| `"chrono/fea/ChNodeFEAxyzrot.h"` | `"chrono/fea/ChNodeFEAxyzrot.h"` | ✅ 一致 |

> 注意：QBlade 7.0.0 某些 include 可能无 `chrono/` 前缀（如 `#include "core/ChVector.h"`），10.0.0 统一需要 `chrono/` 前缀。

---

## 十三、迁移步骤建议

### Step 1: 全局类型替换
在 MBDL 模块中全局替换：
- `ChVector<>` → `ChVector3d`
- `ChQuaternion<>` → `ChQuaterniond`
- `ChMatrix33<>` → `ChMatrix33d`
- `ChFrame<>` → `ChFramed`
- `chrono::VNULL` → `ChVector3d(0.0)`
- `.x` `.y` `.z` 读写 → `.x()` `.x() = val`

### Step 2: 方法名迁移
- `GetPos_dt` → `GetPosDt`
- `GetPos_dtdt` → `GetPosDt2`
- `GetWvel_loc` → `GetAngVelLocal`
- `GetWacc_loc` → `GetAngAccLocal`

### Step 3: 旋转/坐标访问迁移
- `GetA().Get_A_Xaxis()` → `GetRotMat().GetAxisX()`
- `GetLinkAbsoluteCoords().rot.GetXaxis()` → `GetFrameRefToAbs().GetRotMat().GetAxisX()`

### Step 4: 反力接口迁移
- `Get_react_force()` → `GetReaction1().force`
- `Get_react_torque()` → `GetReaction1().torque`

### Step 5: ChShaftsMotor 拆分
将 `SetMotorMode` 模式切换改为使用专门的子类。

### Step 6: FEA Link 类名更新
- `ChLinkPointFrame` → `ChLinkNodeFrame`
- `ChLinkPointPoint` → `ChLinkNodeNode`

### Step 7: include 路径修正
统一所有 include 使用 `chrono/` 前缀。

### Step 8: 自定义扩展类适配
更新 `ChBodyAddedMass`、`ChNodeFEAxyzrotAddedMass`、`ChVariablesBodyAddedMass` 的接口签名以匹配 10.0 基类变化。

### Step 9: 编译验证
每个步骤完成后编译，根据实际编译错误补充遗漏的 API 变更。
