# WindL IEC 61400-3 Validation Report

## Scope

This report validates the IEC wind-condition cases exposed by WindL and audits the built-in spectral models.

- Grid: `30 x 30`
- Time step: `0.05 s`
- Duration: `660.0 s`
- Hub height: `90.0 m`
- Rotor diameter / field size: `90.0 m / 90.0 x 90.0 m`
- Mean wind speed: `12.0 m/s`
- ScaleIEC: `1`

## Acceptance

- Hub sigma and hub TI checks: relative error `<= 5%` for turbulent components.
- PSD checks: log-shape RMSE `<= 0.35`, 90th-percentile absolute log-ratio `<= 0.75`, band-energy error `<= 20%`.
- Deterministic event and steady-profile checks: direct time-history or field comparison against analytical reference.

## Result

- Overall pass: `True`
- Total cases: `16`
- Certification-scope cases: `11`
- Wall time: `284.18 s`

| Case | Group | Status | Runtime [s] | Max TI Err [%] | Max PSD Log-RMSE |
| --- | --- | --- | --- | --- | --- |
| IEC3_NTM_IECKAI_30x30_660s | iec_wind_condition | PASS | 10.40 | 0.08 | 0.263 |
| IEC3_ETM_IECKAI_30x30_660s | iec_wind_condition | PASS | 10.44 | 0.80 | 0.263 |
| IEC3_EWM1_Turb_30x30_660s | iec_wind_condition | PASS | 10.38 | 0.96 | 0.263 |
| IEC3_EWM50_Turb_30x30_660s | iec_wind_condition | PASS | 9.81 | 0.96 | 0.263 |
| IEC3_EWM1_Steady_30x30_660s | iec_wind_condition | PASS | 7.92 | 0.00 | 0.000 |
| IEC3_EWM50_Steady_30x30_660s | iec_wind_condition | PASS | 7.97 | 0.00 | 0.000 |
| IEC3_EOG_30x30_660s | iec_wind_condition | PASS | 8.07 | 0.00 | 0.000 |
| IEC3_EDC_30x30_660s | iec_wind_condition | PASS | 8.26 | 0.00 | 0.000 |
| IEC3_ECD_30x30_660s | iec_wind_condition | PASS | 8.48 | 0.00 | 0.000 |
| IEC3_EWS_30x30_660s | iec_wind_condition | PASS | 8.23 | 0.00 | 0.000 |
| IEC3_UNIFORM_30x30_660s | iec_wind_condition | PASS | 7.90 | 0.00 | 0.000 |
| Spectrum_IECVKM_30x30_660s | spectral_model_audit | PASS | 19.76 | 2.51 | 0.260 |
| Spectrum_BladedKaimal_30x30_660s | spectral_model_audit | PASS | 13.40 | 0.77 | 0.263 |
| Spectrum_BladedVK_30x30_660s | spectral_model_audit | PASS | 15.86 | 0.72 | 0.260 |
| Spectrum_BladedIVK_30x30_660s | spectral_model_audit | PASS | 17.27 | 0.72 | 0.260 |
| Spectrum_Mann_30x30_660s | spectral_model_audit | FAIL | 16.05 | 73.58 | 0.000 |

## Notes

- Mann is included in the spectral-model audit, but it is not a normative IEC 61400-3 1D reference-spectrum case.
- Formal certification still requires review against the licensed IEC standard, traceable requirements, and an independent certification workflow.
- The analytical references used here follow the WindL implementation, the local TurbSim reference source, and the OpenFAST/TurbSim user guide.

## References

- OpenFAST TurbSim user documentation: <https://openfast.readthedocs.io/en/main/source/user/turbsim/>
- TurbSim v2 user guide PDF: <https://openfast.readthedocs.io/en/main/_downloads/cb14d3e2d3533d76e405d730fea19846/TurbSim_v2.00.pdf>
- Local TurbSim reference source: `F:\HawtC3\docs\windL\ref\Source\VelocitySpectra.f90`, `TSsubs.f90`
- Local Bladed format reference: `F:\HawtC3\docs\windL\ref\Bladed wind file format.md`

