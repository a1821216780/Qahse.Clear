# WaveL `.wfc` Binary Format

本文档对应当前仓库实现，作用是解释 WaveL 主缓存文件 `.wfc` 的二进制布局，供 HydroL、调试工具和外部读取器对接使用。

适用实现:
- `src/WaveL/IO/WaveL_IO_Subs.hpp`
- 当前魔数: `QWFC0001`
- 当前版本号: `1`

## 设计目的

`.wfc` 是 WaveL 的**正式主接口缓存文件**。  
它保存规则网格和离散时间轴上的一阶入射波环境场，外部求解器可据此在运行时进行插值查询，而不必在整机仿真时重新生成波场。

当前 `.wfc` 包含以下 8 个场量:

- `eta`
- `u`
- `v`
- `w`
- `ax`
- `ay`
- `az`
- `dynP`

## 字节序与基本约定

- 当前实现按 **little-endian** 写入
- 标量类型均为原生 POD 直接写入
- 当前支持平台默认是 Windows x64 / x86-64
- 文件中不含字符串表、不含压缩、不含校验和

## 文件整体布局

文件由两部分组成:

1. 固定长度头部
2. 8 个 `double` 数组块

顺序固定为:

1. `eta`
2. `u`
3. `v`
4. `w`
5. `ax`
6. `ay`
7. `az`
8. `dynP`

## 固定头部布局

| Offset | Type | Name | Meaning |
| --- | --- | --- | --- |
| `0` | `char[8]` | `magic` | 固定为 `QWFC0001` |
| `8` | `uint32` | `version` | 当前为 `1` |
| `12` | `uint32` | `flags` | 预留，当前固定为 `0` |
| `16` | `int32` | `nx` | x 方向网格点数 |
| `20` | `int32` | `ny` | y 方向网格点数 |
| `24` | `int32` | `nz` | z 方向网格点数 |
| `28` | `int32` | `nt` | 时间步数 |
| `32` | `double` | `dx` | x 方向网格间距 (m) |
| `40` | `double` | `dy` | y 方向网格间距 (m) |
| `48` | `double` | `dz` | z 方向网格间距 (m) |
| `56` | `double` | `dt` | 时间步长 (s) |
| `64` | `double` | `zBottom` | 最底部 z 坐标，当前实现固定为 `-waterDepth` |
| `72` | `double` | `waterDepth` | 水深 (m) |
| `80` | `double` | `Hs` | 结果有义波高 (m) |
| `88` | `double` | `Tp` | 结果峰值周期 (s) |
| `96` | `double` | `fp` | 结果峰值频率 (Hz) |
| `104` | `double` | `m0` | 零阶谱矩 |
| `112` | `double` | `fMin` | 成分最小频率 (Hz) |
| `120` | `double` | `fMax` | 成分最大频率 (Hz) |
| `128` | `double` | `spectralArea` | 谱面积 |
| `136` | `int32` | `reservedWaveOrder` | 当前固定写 `1`，表示一阶缓存 |
| `140` | `int32` | `numComponents` | 波浪离散成分数 |

固定头长度:

- `144 bytes`

## 数组块布局

头部之后紧接着写入 8 个 `double` 数组，每个数组长度相同:

```text
count = nx * ny * nz * nt
```

每个数组占用字节数:

```text
8 * count
```

数组顺序固定:

```text
eta
u
v
w
ax
ay
az
dynP
```

## 线性索引公式

当前实现采用 `x` 最快、`t` 最慢的存储顺序。

线性索引:

```text
idx = (((it * nz + iz) * ny + iy) * nx + ix)
```

其中:

- `0 <= ix < nx`
- `0 <= iy < ny`
- `0 <= iz < nz`
- `0 <= it < nt`

## 坐标定义

### x 坐标

```text
if nx == 1:
    x(ix) = 0
else:
    x(ix) = (ix - 0.5 * (nx - 1)) * dx
```

### y 坐标

```text
if ny == 1:
    y(iy) = 0
else:
    y(iy) = (iy - 0.5 * (ny - 1)) * dy
```

### z 坐标

```text
z(iz) = zBottom + iz * dz
```

当前生成逻辑下:

```text
zBottom = -waterDepth
```

### t 坐标

```text
t(it) = it * dt
```

## 运行时场量含义

`.wfc` 中的 8 个数组分别表示:

- `eta`: 自由液面高程 `eta(x,y,z,t)`  
  注: 当前缓存实现为方便查询，把同一时刻的 `eta` 复制到了全部 `z` 层。
- `u, v, w`: 波粒子速度三分量
- `ax, ay, az`: 波粒子加速度三分量
- `dynP`: 动水压力

## 当前范围限制

当前 `.wfc` 只表达 **WaveL 一阶入射波环境服务层**:

- 包含一阶 `eta/u/v/w/ax/ay/az/dynP`
- 不包含 WAMIT 二阶势流水动力
- 不包含 Morison 力、辐射/绕射载荷、附加质量等 HydroL 责任内容

`reservedWaveOrder` 当前固定为 `1`，因此不能把它解释成“已包含二阶水动力结果”。

## 最小读取流程

1. 读 `magic`，检查是否为 `QWFC0001`
2. 读头部标量并检查 `nx/ny/nz/nt > 0`
3. 计算:

```text
count = nx * ny * nz * nt
```

4. 依次读入 8 个长度为 `count` 的 `double` 数组
5. 按索引公式访问数据

## 读取示例伪代码

```text
read magic[8]
assert magic == "QWFC0001"

read version, flags
read nx, ny, nz, nt
read dx, dy, dz, dt
read zBottom, waterDepth
read Hs, Tp, fp, m0, fMin, fMax, spectralArea
read reservedWaveOrder, numComponents

count = nx * ny * nz * nt

eta  = read_double_array(count)
u    = read_double_array(count)
v    = read_double_array(count)
w    = read_double_array(count)
ax   = read_double_array(count)
ay   = read_double_array(count)
az   = read_double_array(count)
dynP = read_double_array(count)
```

## 与 `.wfm` 的关系

- `.wfc` 是主二进制缓存
- `.wfm` 是对应文本元数据摘要

建议外部读取器:

- 以 `.wfc` 为数值权威源
- 以 `.wfm` 作为人工可读的调试辅助

## 兼容性说明

- 当前格式版本为 `1`
- 若未来魔数或版本号变化，应按 `magic + version` 分支解析
- 当前实现未提供跨版本自动迁移
