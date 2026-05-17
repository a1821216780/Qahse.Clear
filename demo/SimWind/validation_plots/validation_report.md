# WindL Simulation Validation Report

**Date:** 2026-05-04 23:18

## Summary

- **Total cases:** 52
- **Passed:** 52
- **Failed (no stats):** 0
- **Cases with warnings:** 28

### Turbulence Models Used

| Model | Count |
|-------|-------|
| IEC Kaimal | 23 |
| Mann | 9 |
| Bladed IVK | 6 |
| Bladed VK | 5 |
| Bladed Kaimal | 5 |
| IEC VK | 2 |
| User VK | 1 |
| User Spectra | 1 |

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
| `ETM_IK(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 27.9 | 20.8 | 13.1 | 0.4426 | 2.65e+07 | 1 |
| `EWM1_Steady` | IEC Kaimal | 5x5 | 64 | 56.4 | 3.0 | 0.0 | 0.0 | 7.357e-05 | 1.61e+05 | none |
| `EWM1_Steady(cert)` | IEC Kaimal | 30x30 | 13200 | 55.7 | 3.5 | 0.0 | 0.0 | 0.4426 | 1.19e+08 | 2 |
| `EWM1_Turb` | IEC Kaimal | 5x5 | 64 | 12.1 | 51.5 | 41.1 | 25.7 | 7.357e-05 | 1.61e+05 | none |
| `EWM1_Turb(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 68.1 | 51.3 | 32.4 | 0.4426 | 2.65e+07 | 1 |
| `EWM50_Steady` | IEC Kaimal | 5x5 | 64 | 70.5 | 3.0 | 0.0 | 0.0 | 7.357e-05 | 1.61e+05 | none |
| `EWM50_Steady(cert)` | IEC Kaimal | 30x30 | 13200 | 69.7 | 3.5 | 0.0 | 0.0 | 0.4426 | 1.19e+08 | 2 |
| `EWM50_Turb` | IEC Kaimal | 5x5 | 64 | 12.1 | 64.3 | 51.3 | 32.1 | 7.357e-05 | 1.61e+05 | none |
| `EWM50_Turb(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 68.1 | 51.3 | 32.4 | 0.4426 | 2.65e+07 | 1 |
| `EWS` | IEC Kaimal | 5x5 | 64 | 12.2 | 8.9 | 0.0 | 0.0 | 5.96e-05 | 0 | none |
| `EWS(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 4.6 | 0.0 | 0.0 | 0.4426 | 0 | none |
| `NTM_IK` | IEC Kaimal | 5x5 | 64 | 12.1 | 17.4 | 13.6 | 8.5 | 7.357e-05 | 1.61e+05 | none |
| `NTM_IK(cert)` | IEC Kaimal | 30x30 | 13200 | 11.9 | 18.6 | 13.6 | 8.5 | 0.4426 | 2.65e+07 | 1 |
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
| `SimWind` | Mann | 50x50 | 13212 | 12.0 | 19.2 | 14.0 | 3.5 | 0.7617 | 0 | 1 |
| `BIVK` | Bladed IVK | 5x5 | 128 | 12.0 | 3.1 | 0.1 | 0.1 | 0.0001611 | 9.84e+05 | 1 |
| `BK` | Bladed Kaimal | 5x5 | 128 | 12.0 | 11.9 | 9.5 | 7.5 | 0.0001611 | 9.84e+05 | none |
| `PerfOnly` | Bladed Kaimal | 100x100 | 13200 | 11.1 | 20.8 | 13.6 | 8.5 | 4.918 | 1.91e+09 | 1 |
| `Mann` | Mann | 50x50 | 13212 | 12.0 | 19.2 | 14.0 | 3.5 | 0.7617 | 0 | 1 |
| `Mann30` | Mann | 30x30 | 13200 | 12.0 | 17.3 | 13.6 | 8.5 | 0.289 | 0 | 1 |
| `Spec/BIVK` | Bladed IVK | 5x5 | 64 | 12.1 | 17.4 | 13.6 | 8.5 | 0.0001015 | 4.84e+05 | 2 |
| `Spec/BIVK(cert)` | Bladed IVK | 30x30 | 13200 | 11.9 | 18.5 | 12.8 | 10.1 | 0.4427 | 9.72e+07 | 3 |
| `Spec/BK` | Bladed Kaimal | 5x5 | 64 | 12.1 | 17.4 | 13.6 | 8.5 | 0.0001015 | 4.84e+05 | none |
| `Spec/BK(cert)` | Bladed Kaimal | 30x30 | 13200 | 11.9 | 18.6 | 13.0 | 9.5 | 0.4427 | 9.72e+07 | 3 |
| `Spec/BVK` | Bladed VK | 5x5 | 64 | 12.1 | 17.4 | 13.6 | 8.5 | 0.0001015 | 4.84e+05 | 1 |
| `Spec/BVK(cert)` | Bladed VK | 30x30 | 13200 | 11.9 | 18.5 | 12.8 | 10.1 | 0.4427 | 9.72e+07 | 3 |
| `Spec/IVK` | IEC VK | 5x5 | 64 | 12.1 | 17.4 | 17.0 | 17.0 | 7.357e-05 | 1.61e+05 | none |
| `Spec/IVK(cert)` | IEC VK | 30x30 | 13200 | 11.9 | 18.5 | 17.0 | 17.3 | 0.4426 | 2.65e+07 | 1 |
| `Spec/Mann` | Mann | 5x5 | 64 | 12.1 | 17.4 | 13.6 | 8.5 | 0.0002189 | 0 | 1 |
| `Spec/Mann(cert)` | Mann | 30x30 | 13200 | 11.9 | 17.6 | 13.5 | 2.5 | 1.016 | 0 | none |
| `Test_Demo_wind` | Bladed Kaimal | 36x36 | 13200 | 11.0 | 29.8 | 21.4 | 13.4 | 0.6375 | 1.22e+08 | 3 |

## Warnings

- **`ETM_IK(cert)`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 1472 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`EWM1_Steady(cert)`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 6599 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`EWM1_Steady(cert)`:** Reynolds-stress scaling skipped because the hub u-variance is too small.
- **`EWM1_Turb(cert)`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 1472 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`EWM50_Steady(cert)`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 6599 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`EWM50_Steady(cert)`:** Reynolds-stress scaling skipped because the hub u-variance is too small.
- **`EWM50_Turb(cert)`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 1472 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`NTM_IK(cert)`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 1472 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BIVK`:** Grid bottom is at or below ground; low grid heights are clamped in profile calculations.
- **`LG100/BIVK`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 2847 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BIVK`:** Component v uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BIVK`:** Component w uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BIVK__normalized`:** Grid bottom is at or below ground; low grid heights are clamped in profile calculations.
- **`LG100/BIVK__normalized`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 2847 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BIVK__normalized`:** Component v uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BIVK__normalized`:** Component w uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BIVK__raw`:** Grid bottom is at or below ground; low grid heights are clamped in profile calculations.
- **`LG100/BIVK__raw`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 2847 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BIVK__raw`:** Component v uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BIVK__raw`:** Component w uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BVK`:** Grid bottom is at or below ground; low grid heights are clamped in profile calculations.
- **`LG100/BVK`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 2847 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BVK`:** Component v uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BVK`:** Component w uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BVK__normalized`:** Grid bottom is at or below ground; low grid heights are clamped in profile calculations.
- **`LG100/BVK__normalized`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 2847 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BVK__normalized`:** Component v uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BVK__normalized`:** Component w uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BVK__raw`:** Grid bottom is at or below ground; low grid heights are clamped in profile calculations.
- **`LG100/BVK__raw`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 2847 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BVK__raw`:** Component v uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/BVK__raw`:** Component w uses the legacy WindL Kronecker coherence acceleration for the first 3797 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`LG100/Mann`:** Grid bottom is at or below ground; low grid heights are clamped in profile calculations.
- **`LG100/Mann__normalized`:** Grid bottom is at or below ground; low grid heights are clamped in profile calculations.
- **`LG100/Mann__raw`:** Grid bottom is at or below ground; low grid heights are clamped in profile calculations.
- **`LG100/Mann__raw`:** B_MANN uses MannAlphaEps for absolute energy because ScaleIEC=0; set ScaleIEC=1 or 2 to match IEC target sigma exactly.
- **`SimWind`:** MannNx is smaller than the output time-step count; the synthesized Mann box is sampled periodically in time.
- **`BIVK`:** Reynolds-stress target covariance was softened at some points to remain positive-definite while preserving the final component sigmas.
- **`PerfOnly`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 2863 positive frequencies, then switches to diagonal high-frequency synthesis.
- **`Mann`:** MannNx is smaller than the output time-step count; the synthesized Mann box is sampled periodically in time.
- **`Mann30`:** MannNx=128 is smaller than the resolved output step count 13200 for duration 660 s at dt=0.050000000000000003 s; the synthesized Mann box will repeat every 6.4000000000000004 s because the time sampling wraps with t % MannNx. Increase MannNx to at least 13200 (preferably a power of two above that value) to avoid periodic repetition.
- **`Spec/BIVK`:** B_IVKAL requires valid Latitude and Roughness to derive the Bladed 4.11 improved von Karman profile; missing atmospheric inputs fall back to the Bladed von Karman sigma/length defaults.
- **`Spec/BIVK`:** Reynolds-stress target covariance was softened at some points to remain positive-definite while preserving the final component sigmas.
- **`Spec/BIVK(cert)`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 1472 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Spec/BIVK(cert)`:** Component v uses the legacy WindL Kronecker coherence acceleration for the first 1963 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Spec/BIVK(cert)`:** Component w uses the legacy WindL Kronecker coherence acceleration for the first 1963 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Spec/BK(cert)`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 1472 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Spec/BK(cert)`:** Component v uses the legacy WindL Kronecker coherence acceleration for the first 1963 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Spec/BK(cert)`:** Component w uses the legacy WindL Kronecker coherence acceleration for the first 1963 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Spec/BVK`:** Reynolds-stress target covariance was softened at some points to remain positive-definite while preserving the final component sigmas.
- **`Spec/BVK(cert)`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 1472 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Spec/BVK(cert)`:** Component v uses the legacy WindL Kronecker coherence acceleration for the first 1963 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Spec/BVK(cert)`:** Component w uses the legacy WindL Kronecker coherence acceleration for the first 1963 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Spec/IVK(cert)`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 1472 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Spec/Mann`:** Reynolds-stress target covariance was softened at some points to remain positive-definite while preserving the final component sigmas.
- **`Test_Demo_wind`:** Component u uses the legacy WindL Kronecker coherence acceleration for the first 1069 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Test_Demo_wind`:** Component v uses the legacy WindL Kronecker coherence acceleration for the first 1425 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.
- **`Test_Demo_wind`:** Component w uses the legacy WindL Kronecker coherence acceleration for the first 1425 positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.

## Conclusion

All 52 cases completed successfully. 28 case(s) contain warnings (see Warnings section above). The warnings are advisory and do not indicate invalid output.
