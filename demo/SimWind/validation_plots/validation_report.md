# SimWind Simulation Validation Report

**Date:** 2026-05-17 16:35

## Summary

- **Total cases:** 57
- **Passed:** 57
- **Failed (no stats):** 0
- **Cases with warnings:** 28

### Turbulence Models Used

| Model | Count |
|-------|-------|
| IEC Kaimal | 24 |
| Mann | 8 |
| Bladed IVK | 6 |
| Bladed VK | 5 |
| Bladed Kaimal | 5 |
| Unknown | 3 |
| User VK | 2 |
| User Spectra | 2 |
| IEC VK | 2 |

## Case Details

| Case | Turb Model | Grid (y x z) | Steps | Mean U [m/s] | TI u [%] | TI v [%] | TI w [%] | Peak Mem [GiB] | Chol FLOPs | Warnings |
|---|---|---|---|---|---|---|---|---|---|---|
| `ECD` | IEC Kaimal | 5x5 | 64 | 14.5 | 14.3 | 76.5 | 0.0 | 5.96e-05 | 0 | none |
| `ECD(cert)` | IEC Kaimal | 30x30 | 13200 | 12.8 | 7.7 | 96.6 | 0.0 | 0.4426 | 0 | none |
| `EDC` | IEC Kaimal | 5x5 | 64 | 11.4 | 7.0 | 20.6 | 0.0 | 5.96e-05 | 0 | none |
| `EDC(cert)` | IEC Kaimal | 30x30 | 13200 | 11.0 | 8.5 | 26.2 | 0.0 | 0.4426 | 0 | none |
| `EOG` | IEC Kaimal | 5x5 | 64 | 12.4 | 13.8 | 0.0 | 0.0 | 5.96e-05 | 0 | none |
| `EOG(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 4.8 | 0.0 | 0.0 | 0.4426 | 0 | none |
| `ETM_IK` | IEC Kaimal | 5x5 | 64 | 12.1 | 26.3 | 20.8 | 13.0 | 7.357e-05 | 1.61e+05 | none |
| `ETM_IK(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 26.4 | 20.8 | 13.0 | 0.4426 | 2.65e+07 | 1 |
| `EWM1_Steady` | IEC Kaimal | 5x5 | 64 | 56.4 | 3.0 | 0.0 | 0.0 | 7.357e-05 | 1.61e+05 | none |
| `EWM1_Steady(cert)` | IEC Kaimal | 30x30 | 13200 | 55.7 | 3.5 | 0.0 | 0.0 | 0.4426 | 1.19e+08 | 1 |
| `EWM1_Turb` | IEC Kaimal | 5x5 | 64 | 12.1 | 51.5 | 41.1 | 25.7 | 7.357e-05 | 1.61e+05 | none |
| `EWM1_Turb(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 51.5 | 41.1 | 25.7 | 0.4426 | 2.65e+07 | 1 |
| `EWM50_Steady` | IEC Kaimal | 5x5 | 64 | 70.5 | 3.0 | 0.0 | 0.0 | 7.357e-05 | 1.61e+05 | none |
| `EWM50_Steady(cert)` | IEC Kaimal | 30x30 | 13200 | 69.7 | 3.5 | 0.0 | 0.0 | 0.4426 | 1.19e+08 | 1 |
| `EWM50_Turb` | IEC Kaimal | 5x5 | 64 | 12.1 | 64.3 | 51.3 | 32.1 | 7.357e-05 | 1.61e+05 | none |
| `EWM50_Turb(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 64.3 | 51.3 | 32.1 | 0.4426 | 2.65e+07 | 1 |
| `EWS` | IEC Kaimal | 5x5 | 64 | 12.2 | 8.9 | 0.0 | 0.0 | 5.96e-05 | 0 | none |
| `EWS(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 4.6 | 0.0 | 0.0 | 0.4426 | 0 | none |
| `NTM_IK` | IEC Kaimal | 5x5 | 64 | 12.1 | 17.4 | 13.6 | 8.5 | 7.357e-05 | 1.61e+05 | none |
| `NTM_IK(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 17.6 | 13.6 | 8.5 | 0.4426 | 2.65e+07 | 1 |
| `UNIFORM` | IEC Kaimal | 5x5 | 64 | 12.0 | 0.0 | 0.0 | 0.0 | 5.96e-05 | 0 | none |
| `UNIFORM(cert)` | IEC Kaimal | 30x30 | 13200 | 12.0 | 0.0 | 0.0 | 0.0 | 0.4426 | 0 | none |
| `LG100/BIVK` | Bladed IVK | 100x100 | 13200 | 10.4 | 23.4 | 8.5 | 6.1 | 4.919 | 6.96e+09 | 4 |
| `LG100/BIVK__normalized` | Bladed IVK | 100x100 | 13200 | 10.4 | 25.7 | 12.2 | 7.6 | 4.919 | 6.96e+09 | 4 |
| `LG100/BIVK__raw` | Bladed IVK | 100x100 | 13200 | 10.4 | 23.1 | 8.1 | 5.6 | 4.919 | 6.96e+09 | 4 |
| `LG100/BVK` | Bladed VK | 100x100 | 13200 | 10.4 | 26.8 | 13.6 | 8.5 | 4.919 | 6.96e+09 | 4 |
| `LG100/BVK__normalized` | Bladed VK | 100x100 | 13200 | 10.4 | 26.8 | 13.6 | 8.5 | 4.919 | 6.96e+09 | 4 |
| `LG100/BVK__raw` | Bladed VK | 100x100 | 13200 | 10.4 | 27.3 | 13.7 | 8.1 | 4.919 | 6.96e+09 | 4 |
| `LG100/Mann` | Mann | 100x100 | 13200 | 10.4 | 26.8 | 13.6 | 8.5 | 10.27 | 0 | 1 |
| `LG100/Mann__normalized` | Mann | 100x100 | 13200 | 10.4 | 26.8 | 13.6 | 8.5 | 10.27 | 0 | 1 |
| `LG100/Mann__raw` | Mann | 100x100 | 13200 | 10.4 | 84.7 | 20.8 | 20.4 | 10.27 | 0 | 2 |
| `IKS` | IEC Kaimal | 12x12 | 4800 | 11.9 | 17.5 | 13.6 | 8.5 | 0.02714 | 7.16e+09 | none |
| `MannRS` | Mann | 10x10 | 4800 | 11.9 | 17.7 | 13.6 | 8.5 | 0.1045 | 0 | none |
| `USRVK` | User VK | 12x12 | 4800 | 12.1 | 21.9 | 16.3 | 8.7 | 0.02621 | 2.39e+09 | none |
| `USRVK` | User Spectra | 12x12 | 4800 | 11.9 | 21.3 | 12.4 | 10.1 | 0.02621 | 2.39e+09 | none |
| `Qahse_WindL_IKS` | IEC Kaimal | 12x12 | 4800 | 11.9 | 17.5 | 13.6 | 8.5 | 0.02714 | 7.16e+09 | none |
| `Qahse_WindL_MannRS` | Mann | 10x10 | 4800 | 11.9 | 17.7 | 13.6 | 8.5 | 0.1045 | 0 | none |
| `Qahse_WindL_USRVK` | User VK | 12x12 | 4800 | 12.1 | 21.9 | 16.3 | 8.7 | 0.02621 | 2.39e+09 | none |
| `Qahse_WindL_USRVK` | User Spectra | 12x12 | 4800 | 11.9 | 21.3 | 12.4 | 10.1 | 0.02621 | 2.39e+09 | none |
| `BIVK` | Bladed IVK | 5x5 | 128 | 12.0 | 3.1 | 0.1 | 0.1 | 0.0001611 | 9.84e+05 | 1 |
| `BK` | Bladed Kaimal | 5x5 | 128 | 12.0 | 11.9 | 9.5 | 7.5 | 0.0001611 | 9.84e+05 | none |
| `PerfOnly` | Bladed Kaimal | 100x100 | 13200 | 11.1 | 20.8 | 13.3 | 8.3 | 4.919 | 7.37e+09 | 3 |
| `Mann30` | Mann | 30x30 | 13200 | 12.0 | 17.3 | 13.6 | 8.5 | 0.289 | 0 | 1 |
| `Spec/BIVK` | Bladed IVK | 5x5 | 64 | 12.1 | 17.4 | 13.6 | 8.5 | 0.0001015 | 4.84e+05 | 2 |
| `Spec/BIVK(cert)` | Bladed IVK | 30x30 | 13200 | 11.9 | 17.6 | 13.6 | 8.5 | 0.4427 | 9.72e+07 | 4 |
| `Spec/BK` | Bladed Kaimal | 5x5 | 64 | 12.1 | 17.4 | 13.6 | 8.5 | 0.0001015 | 4.84e+05 | none |
| `Spec/BK(cert)` | Bladed Kaimal | 30x30 | 13200 | 11.9 | 17.6 | 13.6 | 8.5 | 0.4427 | 9.72e+07 | 3 |
| `Spec/BVK` | Bladed VK | 5x5 | 64 | 12.1 | 17.4 | 13.6 | 8.5 | 0.0001015 | 4.84e+05 | 1 |
| `Spec/BVK(cert)` | Bladed VK | 30x30 | 13200 | 11.9 | 17.6 | 13.6 | 8.5 | 0.4427 | 9.72e+07 | 3 |
| `Spec/IVK` | IEC VK | 5x5 | 64 | 12.1 | 17.4 | 17.0 | 17.0 | 7.357e-05 | 1.61e+05 | none |
| `Spec/IVK(cert)` | IEC VK | 30x30 | 13200 | 11.9 | 17.6 | 17.0 | 17.0 | 0.4426 | 2.65e+07 | 1 |
| `Spec/Mann` | Mann | 5x5 | 64 | 12.1 | 17.4 | 13.6 | 8.5 | 0.0002189 | 0 | 1 |
| `Spec/Mann(cert)` | Mann | 30x30 | 13200 | 11.9 | 17.6 | 13.6 | 8.5 | 1.016 | 0 | none |
| `Test_Demo_wind` | Bladed Kaimal | 36x36 | 13200 | 11.0 | 29.8 | 21.4 | 13.4 | 0.6375 | 1.22e+08 | 3 |
| `Test_Demo_wind_import_bladed` | T? | 36x36 | 13200 | 11.0 | 29.8 | 21.4 | 13.4 | 0 | 0 | 1 |
| `Test_Demo_wind_import_bts` | T? | 36x36 | 13200 | 11.0 | 29.8 | 21.4 | 13.4 | 0 | 0 | none |
| `Test_Demo_wind_import_tswnd` | T? | 36x36 | 13200 | 11.0 | 29.8 | 21.4 | 13.4 | 0 | 0 | 1 |

## Warnings

- **`ETM_IK(cert)`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 1472 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`EWM1_Steady(cert)`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 6599 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`EWM1_Turb(cert)`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 1472 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`EWM50_Steady(cert)`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 6599 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`EWM50_Turb(cert)`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 1472 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`NTM_IK(cert)`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 1472 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BIVK`:** 网格底部在地面处或低于地面；低处网格高度在剖面计算中已被钳制
- **`LG100/BIVK`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 2847 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BIVK`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BIVK`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BIVK__normalized`:** 网格底部在地面处或低于地面；低处网格高度在剖面计算中已被钳制
- **`LG100/BIVK__normalized`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 2847 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BIVK__normalized`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BIVK__normalized`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BIVK__raw`:** 网格底部在地面处或低于地面；低处网格高度在剖面计算中已被钳制
- **`LG100/BIVK__raw`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 2847 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BIVK__raw`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BIVK__raw`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BVK`:** 网格底部在地面处或低于地面；低处网格高度在剖面计算中已被钳制
- **`LG100/BVK`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 2847 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BVK`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BVK`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BVK__normalized`:** 网格底部在地面处或低于地面；低处网格高度在剖面计算中已被钳制
- **`LG100/BVK__normalized`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 2847 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BVK__normalized`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BVK__normalized`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BVK__raw`:** 网格底部在地面处或低于地面；低处网格高度在剖面计算中已被钳制
- **`LG100/BVK__raw`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 2847 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BVK__raw`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/BVK__raw`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 3797 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`LG100/Mann`:** 网格底部在地面处或低于地面；低处网格高度在剖面计算中已被钳制
- **`LG100/Mann__normalized`:** 网格底部在地面处或低于地面；低处网格高度在剖面计算中已被钳制
- **`LG100/Mann__raw`:** 网格底部在地面处或低于地面；低处网格高度在剖面计算中已被钳制
- **`LG100/Mann__raw`:** B_MANN 由于 ScaleIEC=0 使用 MannAlphaEps 作为绝对能量；设置 ScaleIEC=1 或 2 可精确匹配 IEC 目标标准差
- **`BIVK`:** 部分点处雷诺应力目标协方差被软化以保持正定性，同时保留最终分量标准差
- **`PerfOnly`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 3014 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`PerfOnly`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 4019 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`PerfOnly`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 4019 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Mann30`:** MannNx=128 is smaller than the resolved output step count 13200 for duration 660 s at dt=0.050000000000000003 s; the synthesized Mann box will repeat every 6.4000000000000004 s because the time sampling wraps with t % MannNx. Increase MannNx to at least 13200 (preferably a power of two above that value) to avoid periodic repetition.
- **`Spec/BIVK`:** B_IVKAL 需要有效的 Latitude 和 Roughness 来推导改进 von Karman 剖面；缺少大气输入时回退到 Bladed von Karman 默认值
- **`Spec/BIVK`:** 部分点处雷诺应力目标协方差被软化以保持正定性，同时保留最终分量标准差
- **`Spec/BIVK(cert)`:** B_IVKAL 需要有效的 Latitude 和 Roughness 来推导改进 von Karman 剖面；缺少大气输入时回退到 Bladed von Karman 默认值
- **`Spec/BIVK(cert)`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 1472 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Spec/BIVK(cert)`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 1963 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Spec/BIVK(cert)`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 1963 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Spec/BK(cert)`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 1472 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Spec/BK(cert)`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 1963 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Spec/BK(cert)`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 1963 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Spec/BVK`:** 部分点处雷诺应力目标协方差被软化以保持正定性，同时保留最终分量标准差
- **`Spec/BVK(cert)`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 1472 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Spec/BVK(cert)`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 1963 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Spec/BVK(cert)`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 1963 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Spec/IVK(cert)`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 1472 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Spec/Mann`:** 部分点处雷诺应力目标协方差被软化以保持正定性，同时保留最终分量标准差
- **`Test_Demo_wind`:** 分量 u 使用了经典 WindL Kronecker 近似加速，覆盖前 1069 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Test_Demo_wind`:** 分量 v 使用了经典 WindL Kronecker 近似加速，覆盖前 1425 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Test_Demo_wind`:** 分量 w 使用了经典 WindL Kronecker 近似加速，覆盖前 1425 个正频率，随后切换到对角高频合成。设置 AllowCohApprox=false 可强制使用精确严格相干路径。
- **`Test_Demo_wind_import_bladed`:** Imported file dt does not match TimeStep; using file header value.
- **`Test_Demo_wind_import_tswnd`:** Imported file dt does not match TimeStep; using file header value.

## Conclusion

All 57 cases completed successfully. 28 case(s) contain warnings (see Warnings section above). The warnings are advisory and do not indicate invalid output.
