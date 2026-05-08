# WaveL Validation Manual Plot Review

日期: 2026-05-09

范围:
- 本次人工审图覆盖 `demo/WaveL/validation_result` 下的 9 张 WaveL 校核图。
- 结论仅针对当前 WaveL 已声明范围: 一阶入射波环境生成、`.wfc/.wfm` 缓存输出、`eta/u/v/w/ax/ay/az/dynP` 查询与浸润判断。

## 总体结论

- 9 张图均显示正常，与自动验证结果一致。
- `USER_TIMESERIES` 的频域图已经补齐，验证链路闭环。
- 新增的密集网格案例已经改成真正的二维切片图，能够说明 `.wfc` 是一个 `(x,y,z,t)` 的四维场缓存，而不是单点时程序列。
- 结合单元测试、自动验证报告和人工审图，当前 WaveL 主模块在既定一阶范围内可判断为正确。

## 分图结论

### 1. Qahse_WaveL_Main_DEMO_validation.png

- `eta(t)` 非空白，时程序列形态合理。
- JONSWAP 理论谱线与离散谱线吻合良好。
- 未见主峰漂移或高频尾部异常。
- 结论: 正常。

### 2. Qahse_WaveL_PiersonMoskowitz_DEMO_validation.png

- `eta(t)` 形态正常。
- PM 理论谱与离散谱吻合良好。
- 主峰位置和尾部衰减趋势合理。
- 结论: 正常。

### 3. Qahse_WaveL_OchiHubble_DEMO_validation.png

- `eta(t)` 时程合理。
- 双峰谱结构清晰，主峰与次峰均可辨认。
- 理论谱与离散谱整体一致。
- 结论: 正常。

### 4. Qahse_WaveL_Torsethaugen_DEMO_validation.png

- 时程振幅与海况设定一致。
- 双峰结构和高频尾部都正常。
- 未见离散化断裂或错峰。
- 结论: 正常。

### 5. Qahse_WaveL_User_Spectrum_DEMO_validation.png

- `eta(t)` 非空白。
- 用户输入谱的插值曲线与离散谱线吻合良好。
- 输入频带内未见异常插值波动。
- 结论: 正常。

### 6. Qahse_WaveL_User_TimeSeries_DEMO_validation.png

- 上半幅显示去均值后的源时程与输出时程，二者基本重合。
- 下半幅显示源时程分解谱与输出重建谱，主能量频带一致。
- 低能尾部仍有轻微差异，但不影响主频带结论。
- 结论: 正常。

### 7. Qahse_WaveL_Regular_Kinematics_DEMO_validation.png

- `eta/u/w` 的 cache 曲线与 theory 曲线基本完全重合。
- 浸润状态和压力符号切换边界清晰。
- 这是当前证据最强的单点运动学校核图。
- 结论: 正常，并对一阶运动学正确性给出强支撑。

### 8. Qahse_WaveL_DenseGrid_Regular_DEMO_validation.png

- 该图是密集网格二维切片校核，不是单点时程序列。
- 第一行 `eta(x,y)` 显示了规则斜向入射波在水平面上的相位梯度。
- 第二行 `u(x,z)` 显示了近自由液面速度较大、随水深衰减的典型分布。
- 第三行 `w(x,z)` 显示了垂向速度的合理空间变化。
- 第四行 `u(y,z)` 给出了固定 `x` 位置处的侧向-竖向截面，说明缓存不仅能做 `x-z` 剖面，也能做 `y-z` 剖面。
- 三个误差图均为零场，说明 `.wfc` 缓存与解析理论在该规则波案例上逐点一致。
- 结论: 正常，并确认 `.wfc` 表示的是 `(x,y,z,t)` 四维场。

### 9. Qahse_WaveL_DenseGrid_Regular_DEMO_sequence.png

- 该图展示了同一 `eta(x,y)` 水平切片在多个时刻的序列。
- 颜色场随时间平滑平移，符合规则斜向入射波的传播预期。
- 统一色标使不同时刻之间可以直接比较波峰和波谷位置变化。
- 结论: 正常，说明 `.wfc` 不只是静态空间场，而是可用于时变四维查询。

## 对当前模块是否正确的判断

按当前正式范围:
- 一阶入射波环境生成
- `.wfc/.wfm` 主缓存输出
- `eta/u/v/w/ax/ay/az/dynP` 时空查询
- `IsSubmerged / immersionDepth` 判断

当前判断:

- **主模块正确。**
- **自动验证、单元测试与人工审图对当前一阶主链给出了相互一致的支撑。**
- **当前未见阻断性错误。**

## 仍需保守说明的边界

1. 这里的“正确”仅指一阶波浪环境服务层，不包含二阶势流或二阶水动力。
2. `USER_TIMESERIES` 的频域判断以主能量频带为主，不是逐频点严格相等证明。
3. 当前人工审图覆盖的是现有 demo 集，并不等价于任意输入条件下的全覆盖证明。
