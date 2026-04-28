# WindL SimWind 与 TurbSim/Bladed 差距分析报告

## 1. 执行摘要

本报告聚焦当前 `SimWind` 全场风生成链路，目标是为后续研发整改提供可执行的差距清单，而不是对整个 WindL 产品做泛化评估。

基于当前代码、现有验证产物、本地 TurbSim 源码、OpenFAST/TurbSim 官方资料、Bladed `.wnd` 格式文档和公开 Bladed turbulence-generation 材料，当前结论如下：

1. **WindL 已经具备可用的 IEC 导向全场风生成能力**。当前实现覆盖 IEC 常用风况、IEC/Bladed 常见谱模型、Mann 3D 路径、`.bts/.wnd/.sum` 输出和控制台进度提示；现有本地验证已经证明其在 `30x30, dt=0.05 s, 660 s` 的 IEC 61400-3 校核范围内结果稳定。
2. **WindL 不能宣称与 TurbSim 等价**。主要原因不是“文件打不开”，而是 TurbSim 支持的谱模型族、相干结构输出、tower/formatted/HH/BIN 等输出族、双随机种子/RNG 选项、API/NONE coherence 分支、KH/coherent-structure 工作流等，WindL 当前并未实现。
3. **WindL 也不能宣称与 Bladed turbulence-generation 工作流等价**。当前证据只足够支持 “WindL 能写出一类兼容 Bladed `.wnd` 的文件头和标准化数据区”，还不足以支持“与 Bladed 内部湍流生成算法或完整建模流程数值等价”。
4. **当前最大技术风险不是缺少功能，而是“输入面大于实现面”**。有些参数已经暴露到 `.qwd/.yaml`，但并未进入 `SimWind` 计算；有些用户定义输入文件虽然读取了多列，生成时却只用了其中一列。这会让用户误以为功能已经生效。
5. **WindL 的合理定位**应表述为：  
   **一个以 IEC 风况为核心、带部分 TurbSim/Bladed 文件兼容能力的内部风场生成器**，而不是完整 TurbSim 或完整 Bladed 对等实现。

## 2. 证据基线与分析方法

### 2.1 当前 WindL 基线

- 代码主证据：
  - `src/WindL/SimWind.cpp`
  - `src/WindL/WindL_Type.hpp`
  - `src/WindL/IO/WindL_IO_Subs.hpp`
  - `src/main.cpp`
- 现有验证与性能证据：
  - `demo/WindL/IEC61400_3/certification_result/validation_report.md`
  - `demo/WindL/SimWind_Performance_Assessment.md`

### 2.2 TurbSim 对照基线

- OpenFAST TurbSim 文档：<https://openfast.readthedocs.io/en/main/source/user/turbsim/>
- TurbSim v2.00 用户手册 PDF：<https://openfast.readthedocs.io/en/main/_downloads/cb14d3e2d3533d76e405d730fea19846/TurbSim_v2.00.pdf>
- 本地源码：
  - `F:\HawtC3\docs\windL\ref\Source\TurbSim_Types.f90`
  - `F:\HawtC3\docs\windL\ref\Source\VelocitySpectra.f90`
  - `F:\HawtC3\docs\windL\ref\Source\TSsubs.f90`
  - `F:\HawtC3\docs\windL\ref\Source\TS_FileIO.f90`
  - `F:\HawtC3\docs\windL\ref\Source\CohStructures.f90`

### 2.3 Bladed 对照基线

- 本地 `.wnd` 格式说明：`F:\HawtC3\docs\windL\ref\Bladed wind file format.md`
- DNV 公开 turbulence-generation 材料：<https://www.dnv.com/publications/bladed-advanced-multisetup-tutorial-setting-up-turbulence-generation-43017/>
- `BladedWindFiledTheor.pdf` 仅作为补充；本次没有把无法可靠文本化提取的内容作为单独结论依据。

### 2.4 判定规则

每个能力点统一归类为：

- `已实现`
- `部分实现`
- `已实现但行为不同`
- `未实现`
- `当前证据不足`

每条高优先级差距均要求同时具备：

- WindL 当前代码证据
- 至少一个外部或本地参考来源
- 明确工程影响
- 优先级与整改建议

## 3. 能力矩阵

| 功能域 | WindL 当前状态 | TurbSim 对照 | Bladed 对照 | 影响 | 优先级 |
| --- | --- | --- | --- | --- | --- |
| IEC 常用风况（NTM/ETM/EWM/EOG/EDC/ECD/EWS/UNIFORM） | 已实现并有本地验证 | 已支持 | 公开资料对完整事件族证据不足 | IEC 主线能力已可用 | - |
| IEC Kaimal / IEC von Karman | 已实现 | 已支持 | Bladed 不以 TurbSim 命名组织 | 主线谱模型齐备 | - |
| Bladed Kaimal / vK / Improved vK / Mann | 已实现 `.wnd` 写出所需路径 | TurbSim 不以 Mann 为内建谱模型族 | 与公开 `.wnd` 头部定义一致 | 文件兼容能力较强，但不等于算法等价 | - |
| TurbSim 扩展谱族（`NONE/GP_LLJ/NWTCUP/SMOOTH/WF_UPW/WF_07D/WF_14D/TIDAL/RIVER/API`） | 未实现 | 已支持 | 不适用/公开证据不足 | 与 TurbSim 覆盖面差距明显 | P1 |
| coherence 家族 | 仅 `IEC/GENERAL/default` | 至少有 `NONE/API`，并含 TurbSim 自身分支 | 公开资料不足 | 与 TurbSim 空间相干覆盖面不等价 | P1 |
| 全频严格相干矩阵 | 部分实现；大网格自动转 Kronecker + 高频对角合成 | TurbSim 有完整 coherence 求解工作流 | 公开资料不足 | 大网格下空间相关性保真度存在模型差 | P0 |
| 用户定义 shear/profile | 部分实现；只实际使用风速列 | TurbSim 用户 profile 会读取高度、风速、风向、sigma、长度尺度 | Bladed 公开资料不足 | 输入被部分忽略，易造成误用 | P0 |
| 用户定义 spectra | 部分实现；单组频率-PSD 插值 | TurbSim 用户输入与 profile/workflow 更完整 | 公开资料不足 | 可用但能力面更窄 | P1 |
| 用户定义 wind speed time series | 部分实现；最近点映射、无空间插值族 | TurbSim 有独立输入工作流 | 公开资料不足 | 可导入但表达能力有限 | P2 |
| 输出 `.bts/.wnd/.sum` | 已实现 | `.bts/.wnd` 已支持 | `.wnd` 已支持 | 基础 full-field 输出可用 | - |
| 输出 `tower/formatted/HH/BIN/CTS` | 未实现 | 已支持 | 公开资料不足 | 影响与 TurbSim 工作流兼容 | P1 |
| CLI/模式 | 仅 `Mode=GENERATE` | TurbSim 是专用生成器 | Bladed 是完整产品工作流 | SimWind 只是生成器，不是完整替代工具 | P1 |
| 验证证据 | IEC 范围内较强；等价性证据不足 | 官方工具 | 专有工具 | 影响对外等价表述 | P1 |

## 4. 详细差距分析

### 4.1 谱模型与湍流家族

#### 4.1.1 已实现部分

- WindL 当前公开的 `TurbModel` 只有：
  - `IEC_KAIMAL`
  - `IEC_VKAIMAL`
  - `B_MANN`
  - `B_KAL`
  - `B_VKAL`
  - `B_IVKAL`
  - `USER_SPECTRA`
  - `USER_WIND_SPEED`  
  证据：`src/WindL/WindL_Type.hpp:46`

- TurbSim 内建谱模型更宽，`TurbSim_Types.f90` 明确包含：
  - `SpecModel_NONE`
  - `SpecModel_IECKAI`
  - `SpecModel_IECVKM`
  - `SpecModel_GP_LLJ`
  - `SpecModel_NWTCUP`
  - `SpecModel_SMOOTH`
  - `SpecModel_WF_UPW`
  - `SpecModel_WF_07D`
  - `SpecModel_WF_14D`
  - `SpecModel_TIDAL`
  - `SpecModel_RIVER`
  - `SpecModel_API`  
  证据：`F:\HawtC3\docs\windL\ref\Source\TurbSim_Types.f90:15-26`

#### 4.1.2 已证实差距

- **WindL 缺少 TurbSim 扩展谱模型族。**
  - WindL 代码证据：`src/WindL/WindL_Type.hpp:46`
  - TurbSim 证据：`TurbSim_Types.f90:15-26`，以及 `TSsubs.f90:1042-1141` 对这些谱模型的调用分支
  - 工程影响：无法把一批现有 TurbSim 输入直接迁移到 WindL，尤其是低空急流、NWTCUP、风场下游模型、水动力模型和 API 模型。
  - 建议：先补 `NONE/NWTCUP/API`，再评估 `GP_LLJ/SMOOTH/WF_*`，最后决定是否覆盖 `TIDAL/RIVER`。
  - 优先级：`P1`

- **`WindModel::UNIFORM` 只能作为 WindModel 分支，不等于 TurbSim 的 `SpecModel_NONE`。**
  - WindL 代码证据：`src/WindL/WindL_Type.hpp:59`，`src/WindL/SimWind.cpp:1808-1817`
  - TurbSim 证据：`TurbSim_Types.f90:15`，`TSsubs.f90:1141`
  - 工程影响：WindL 的“无湍流”是按特定事件/风况逻辑实现的，不是一个可与其他 profile/coherence 组合的独立谱模型。
  - 建议：若要对齐 TurbSim 输入语义，应新增真正的 `SpecModel_NONE` 语义层。
  - 优先级：`P2`

### 4.2 平均风剖面与气象参数

#### 4.2.1 已实现部分

- `WindProfileType` 已定义并可序列化：`src/WindL/WindL_Type.hpp:106`，`src/WindL/IO/WindL_IO_Subs.hpp:171`
- `BuildGridAndProfiles()` 当前实际分支只有：
  - 用户 profile
  - `UNIFORM`
  - steady EWM `z^0.11`
  - `LOG`
  - 否则走幂律  
  证据：`src/WindL/SimWind.cpp:664-694`

#### 4.2.2 已证实差距

- **`WindProfileType::IEC` 当前不是独立实现路径。**
  - WindL 代码证据：`WindL_Type.hpp:106`；实际 profile 分支见 `SimWind.cpp:664-694`，其中没有 `WindProfileType::IEC` 专用逻辑。
  - TurbSim 证据：TurbSim 文档和源码中 profile、谱、稳定度参数是联动工作流，而不是简单的 “枚举存在但不生效”。
  - 工程影响：输入层暴露了 `IEC` profile 选项，但实现层并未形成独立语义，容易让用户误判 profile 来源。
  - 建议：要么补齐 `IEC` profile 独立逻辑，要么在输入校验/文档中明确降级为当前默认幂律行为。
  - 优先级：`P1`

- **`Rich_No/UStar/ZL/ZI/PC_UW/PC_UV/PC_VW` 已暴露到输入，但当前 `SimWind` 不参与计算。**
  - WindL 输入证据：`src/WindL/WindL_Type.hpp:228-234`
  - 序列化证据：`src/WindL/IO/WindL_IO_Subs.hpp:196-202`
  - 静态分析证据：2026-04-29 对 `src/WindL/SimWind.cpp` 做符号检索，没有发现这些字段进入 profile、谱、coherence 或事件计算。
  - TurbSim 证据：`TS_FileIO.f90`、`VelocitySpectra.f90`、`TSsubs.f90` 中这些气象/稳定度相关参数属于实际建模流程的一部分。
  - 工程影响：这是**静默失效**风险。用户可以合法写入这些参数，但当前结果不会响应这些输入。
  - 建议：短期内应在校验或 `.sum` 警告中显式报 unsupported；中期再决定是否补真实模型。
  - 优先级：`P0`

### 4.3 空间相干与合成算法

#### 4.3.1 已实现部分

- WindL 当前 `CohModel` 只有 `IEC / GENERAL / DEFAULT_COH`：`src/WindL/WindL_Type.hpp` 与 `src/WindL/IO/WindL_IO_Subs.hpp:58-78`
- `ResolveCoherenceModels()` 根据 IEC 谱或非 IEC 谱选择 `IEC` 或 `GENERAL` 作为默认值：`src/WindL/SimWind.cpp:630-637`
- 非 Mann 路径下，WindL 支持 dense strict coherence；但在大网格时会自动触发 Kronecker 近似：`src/WindL/SimWind.cpp:447-493, 809-830, 1290-1407`

#### 4.3.2 已证实差距

- **WindL 当前没有 TurbSim 的 `CohMod_NONE` 与 `CohMod_API` 家族。**
  - WindL 代码证据：`src/WindL/WindL_Type.hpp` 仅有 `IEC/GENERAL/DEFAULT_COH`
  - TurbSim 证据：`TurbSim_Types.f90:33-36`，`TSsubs.f90:370-419`（NONE），`TSsubs.f90:481-677`（API）
  - 工程影响：无法直接复现一批基于 TurbSim `NONE/API` coherence 语义的输入。
  - 建议：先补 `NONE`，再补 `API`；否则至少在输入文档里把当前 coherence 覆盖边界写清楚。
  - 优先级：`P1`

- **WindL 当前并不是“全频完整严格相干矩阵”实现。**
  - WindL 代码证据：
    - 自动判断近似：`src/WindL/SimWind.cpp:447-493`
    - 启用 Kronecker：`src/WindL/SimWind.cpp:809-830`
    - 高频切到 diagonal synthesis：`src/WindL/SimWind.cpp:1344-1437`
    - 警告文本明确写出：`legacy WindL Kronecker coherence acceleration ... then switches to diagonal high-frequency synthesis`，见 `src/WindL/SimWind.cpp:830-833`
  - TurbSim 对照证据：TurbSim coherence 工作流在 `TSsubs.f90` 中按具体 coherence 家族构造 Fourier coefficients，并没有 WindL 当前这套自动“先 Kronecker 后对角”降级语义。
  - 工程影响：大网格下，WindL 的空间相关性保真度与“完整 pairwise strict coherence”存在可预期偏差。这是结果层风险，不只是性能实现差异。
  - 建议：将当前近似路径标记为显式算法选项，而不是自动替代；对认证或对标场景保留全频 dense 路径。
  - 优先级：`P0`

### 4.4 用户定义输入

#### 4.4.1 已实现部分

- `UserShearData`、`UserSpectraData`、`UserWindSpeedData` 已定义：`src/WindL/WindL_Type.hpp:314, 349, 389`
- 对应读取器已实现：`src/WindL/IO/WindL_IO_Subs.hpp:598, 679, 738`

#### 4.4.2 已证实差距

- **`UserShear` 文件读取了 5 列，但生成时实际只用了风速列。**
  - WindL 输入结构证据：`UserShearData` 包含 `heights/windSpeeds/windDirections/standardDeviations/lengthScales`，见 `src/WindL/WindL_Type.hpp:314`
  - 读取证据：`src/WindL/IO/WindL_IO_Subs.hpp:598-624`
  - 实际使用证据：`src/WindL/SimWind.cpp:666, 677`，仅对 `windSpeeds` 做线性插值
  - TurbSim 证据：`TS_FileIO.f90:1220-1248` 明确读取 `USR_Z/USR_U/USR_WindDir/USR_Sigma/USR_L`
  - 工程影响：这是第二个**静默部分生效**风险。用户定义 profile 文件中除风速外的列当前不会影响 WindL 生成结果。
  - 建议：短期内在读取后直接告警“direction/sigma/length scale currently ignored”；中期补齐 profile 和 turbulence 耦合逻辑。
  - 优先级：`P0`

- **`USER_SPECTRA` 目前是单组频率-PSD 插值，不是 TurbSim 那类更完整的用户定义风场工作流。**
  - WindL 代码证据：`src/WindL/SimWind.cpp:798-800, 862-892`
  - TurbSim 证据：用户定义输入在 TurbSim 中与 profile、稳定度、其他输出族共同工作，不只是简单 PSD 查表。
  - 工程影响：能做基础谱替换，但不能认为已覆盖 TurbSim 用户定义输入生态。
  - 建议：把 `USER_SPECTRA` 文档定位为“简化 PSD 替换模式”。
  - 优先级：`P1`

- **`USER_WIND_SPEED` 当前采用最近点映射，不是一个完整的空间插值/重采样框架。**
  - WindL 代码证据：`src/WindL/SimWind.cpp:1223-1261`
  - 工程影响：输入点云与目标网格不一致时，可能出现明显的空间折线化和点状复制。
  - 建议：后续如要对齐工程级导入，应至少提供双线性/反距离插值选项。
  - 优先级：`P2`

### 4.5 输出格式与工作流兼容

#### 4.5.1 已实现部分

- WindL 已支持写出：
  - `.bts`：`src/WindL/SimWind.cpp:1908`
  - Bladed `.wnd`：`src/WindL/SimWind.cpp:1954`
  - TurbSim-compatible `.wnd`：`src/WindL/SimWind.cpp:2076`
  - `.sum`：`src/WindL/SimWind.cpp:2238-2274`
- CLI 已支持 `--qwd` 单文件生成：`src/main.cpp:74, 105, 116`
- Bladed `.wnd` 本地格式文档明确写出：
  - `Record 2` 定义模型 ID，包括 `8 = Mann model`：`F:\HawtC3\docs\windL\ref\Bladed wind file format.md:16-25`
  - 数据区应为 **Normalised, zero mean, unit standard deviation wind speed deviations**：`...Bladed wind file format.md:114`

#### 4.5.2 已证实差距

- **WindL 缺少 TurbSim 其他重要输出族。**
  - WindL 代码证据：当前只写 `.bts/.wnd/.sum`，见 `src/WindL/SimWind.cpp:1908, 1954, 2076, 2238-2274`
  - TurbSim 证据：
    - 输入侧输出开关：`TS_FileIO.f90:222-250`
    - 运行侧输出：`TurbSim.f90:296-326`
    - 具体包括 `WrBHHTP/WrADHH/WrADFF/WrBLFF/WrADTWR/WrFMTFF/WrACT`
  - 工程影响：与 TurbSim 的后处理、塔点、coherent-structure 和老工作流兼容性不完整。
  - 建议：按需求优先补 `tower(.twr)` 与 `formatted full-field(.u/.v/.w)`；`CTS/HH/BIN` 后置。
  - 优先级：`P1`

- **WindL 当前只支持 `Mode=GENERATE`，不是 TurbSim/Bladed 那类完整工作流入口。**
  - WindL 代码证据：`src/WindL/SimWind.cpp:516-519` 直接限制 `Mode=GENERATE`
  - CLI 证据：`src/main.cpp:74, 105-126`
  - 工程影响：虽然 `WindLInput` 有 `IMPORT/BATCH` 枚举，但 `SimWind` 本身不是完整替代入口。
  - 建议：报告与文档中避免把 `SimWind` 表述成“完整 TurbSim 替代器”。
  - 优先级：`P1`

- **WindL 的 Bladed `.wnd` 路径固定按 3 分量写出，不覆盖 1/2 分量工作模式。**
  - WindL 代码证据：`src/WindL/SimWind.cpp:1963-1964` 固定 `componentCount = 3`
  - Bladed 格式证据：本地文档在 header group 1/2/4 中明确允许 `1, 2 or 3` turbulence components
  - 工程影响：与 Bladed 格式名义兼容，但能力面不是完整子集。
  - 建议：如果要宣称更强的 Bladed 兼容，应把 component-count 语义做成真实可配置项。
  - 优先级：`P2`

#### 4.5.3 需保守表述的部分

- **当前可以说 WindL 的 `.wnd` 是“Bladed-compatible export”**，但不能说“WindL 已经实现完整 Bladed turbulence-generation 工作流”。  
  原因是当前证据只覆盖 `.wnd` 头部和数据规范、少量模型 ID 和基础导出，不覆盖 Bladed 专有的整体建模流程、交互式参数体系或内部数值细节。

### 4.6 性能与可扩展性

- 当前 WindL 已有两类正面证据：
  - IEC 61400-3 `30x30, dt=0.05 s, 660 s` 校核全通过：见 `demo/WindL/IEC61400_3/certification_result/validation_report.md`
  - `100x100, 660 s, 0.05 s` 大网格案例在本机 Release 下跑通，墙钟时间约 `186.27 s`：见 `demo/WindL/SimWind_Performance_Assessment.md`

- 但当前**没有**可以支持“WindL 比 TurbSim 或 Bladed 更快/更慢”的直接对标证据。
  - 工程影响：性能章节只能写 WindL 当前证据，不能下跨工具定量优劣结论。
  - 建议：如后续要做正式对标，应固定同一网格、同一风况、同一 RNG 统计指标、同一硬件和同一输出族。
  - 优先级：`P2`

## 5. 优先级路线图

### P0：先解决数值正确性和静默失效风险

1. **把当前自动 Kronecker/高频对角合成改成显式算法选项**
   - 默认是否允许近似必须对用户可见
   - 认证/对标场景应强制全频完整 strict coherence
2. **对“已读入但未参与计算”的参数做强告警或拒绝**
   - `Rich_No/UStar/ZL/ZI/PC_UW/PC_UV/PC_VW`
3. **对 `UserShear` 的部分列忽略行为做强告警**
   - 至少在 `.sum` 和控制台中明确列出 ignored columns

### P1：补齐常用工程能力缺口

1. 补 `NONE/NWTCUP/API` 等 TurbSim 常用扩展谱和 coherence 家族
2. 补 `tower(.twr)`、`formatted full-field(.u/.v/.w)` 输出
3. 让 `WindProfileType::IEC` 成为真实实现语义，或明确降级
4. 明确文档定位：
   - WindL 当前是 IEC-oriented internal generator
   - 不是完整 TurbSim / Bladed 替代

### P2：增强覆盖面和对标深度

1. 补 1/2-component Bladed `.wnd`
2. 补 `USER_WIND_SPEED` 的空间插值与重采样
3. 评估 `GP_LLJ/SMOOTH/WF_07D/WF_14D/TIDAL/RIVER`
4. 建立 WindL vs TurbSim 的统一性能对标基准

## 6. 结论

当前 WindL `SimWind` 模块已经跨过了“只能做 demo”的阶段：IEC 风况主线、Mann/Bladed/TurbSim 关键 full-field 输出、控制台进度、性能预估和 IEC 本地验证都已经具备。

但它与 TurbSim/Bladed 的主要差距，集中在三件事：

1. **覆盖面差距**：TurbSim 的扩展谱模型、coherent structures、tower/formatted/HH/BIN 输出族还没有。
2. **语义差距**：一部分输入参数和用户定义文件列已经暴露，但还没有真正进入计算。
3. **算法边界差距**：大网格下当前会自动进入 Kronecker + 对角高频近似，这与“完整 strict coherence”不是一回事。

因此，当前最准确的产品表述应该是：

> **WindL SimWind 是一个以 IEC 风况为核心、带部分 TurbSim/Bladed 文件兼容能力的内部风场生成器，而不是完整 TurbSim 或完整 Bladed 对等实现。**

## 7. 参考资料

### 官方与公开资料

- OpenFAST TurbSim 用户文档：<https://openfast.readthedocs.io/en/main/source/user/turbsim/>
- TurbSim v2.00 用户手册 PDF：<https://openfast.readthedocs.io/en/main/_downloads/cb14d3e2d3533d76e405d730fea19846/TurbSim_v2.00.pdf>
- DNV Bladed turbulence-generation 公开材料：<https://www.dnv.com/publications/bladed-advanced-multisetup-tutorial-setting-up-turbulence-generation-43017/>
- Mann, J. (1994). *The spatial structure of neutral atmospheric surface-layer turbulence*. DOI: <https://doi.org/10.1017/S0022112094001886>
- Mann, J. (1998). *Wind field simulation*. DOI: <https://doi.org/10.1016/S0266-8920(97)00036-2>

### 本地参考资料

- `F:\HawtC3\docs\windL\ref\Source\TurbSim_Types.f90`
- `F:\HawtC3\docs\windL\ref\Source\VelocitySpectra.f90`
- `F:\HawtC3\docs\windL\ref\Source\TSsubs.f90`
- `F:\HawtC3\docs\windL\ref\Source\TS_FileIO.f90`
- `F:\HawtC3\docs\windL\ref\Source\CohStructures.f90`
- `F:\HawtC3\docs\windL\ref\Bladed wind file format.md`
- `demo/WindL/IEC61400_3/certification_result/validation_report.md`
- `demo/WindL/SimWind_Performance_Assessment.md`
