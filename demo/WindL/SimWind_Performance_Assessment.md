# WindL SimWind demo cases and 100x100 performance assessment

## Demo cases

| File | TurbModel | Main purpose |
| --- | ---: | --- |
| `Qahse_WindL_Bladed_Kaimal_DEMO.qwd` | 3 | Bladed General Kaimal header (`Record 2 = 7`) plus `.bts` and TurbSim-compatible `.ts.wnd`. |
| `Qahse_WindL_Bladed_ImprovedVonKarman_DEMO.qwd` | 5 | Bladed improved von Karman header (`Record 2 = 4`) with explicit length scales and TI values. |
| `Qahse_WindL_Mann_DEMO.qwd` | 2 | Bladed Mann header (`Record 2 = 8`) with Mann length/gamma/max wavelength fields. |
| `Qahse_WindL_LargeGrid_100x100_PERF_ONLY.qwd` | 3 | Requested 100 x 100, 660 s, 0.05 s case. SimWind now runs it with preflight memory/FLOP estimates and measured ETA progress. |

## Format notes

- TurbSim native full-field binary output is `.bts`. The TurbSim user guide describes it as self-contained because the header includes the scaling needed to convert the 2-byte integer data back to velocities.
- TurbSim `.wnd` output is a Bladed-style full-field binary file and needs the `.sum` scaling information for decoding.
- WindL now keeps these outputs separate:
  - `WrBlwnd=true` writes native Bladed `.wnd` using the model-specific header in `F:\HawtC3\docs\windL\ref\Bladed wind file format.md`.
  - `WrTrwnd=true` writes TurbSim-compatible `.ts.wnd` when both WND outputs are requested; if it is the only WND output, it uses `.wnd`.
  - `WrTrbts=true` writes TurbSim native `.bts`.

## Literature and reference basis

- NREL/OpenFAST TurbSim User's Guide v2.00.00, output section: `.bts` is the TurbSim full-field binary format, while `.wnd` is the Bladed-style full-field binary format. See <https://www.nrel.gov/wind/nwtc/assets/downloads/TurbSim/TurbSim_v2.00.pdf>.
- OpenFAST InflowWind/FAST.Farm documentation lists full-field turbulent wind support in TurbSim, Bladed, and HAWC formats. See <https://openfast.readthedocs.io/en/main/source/user/turbsim/output.html>.
- The Bladed wind-file record layout used here follows `F:\HawtC3\docs\windL\ref\Bladed wind file format.md`.
- Mann-model notes follow the IEC/Mann spectral-tensor literature: the Mann uniform-shear model starts from a von Karman isotropic spectrum and evolves an anisotropic spectral tensor under mean shear.
- Kaimal spectral context: Kaimal et al. (1972), "Spectral characteristics of surface-layer turbulence", DOI `10.1002/qj.49709841707`.
- Mann spectral-tensor context: Mann (1994), "The spatial structure of neutral atmospheric surface-layer turbulence", DOI `10.1017/S0022112094001886` (<https://orbit.dtu.dk/en/publications/the-spatial-structure-of-neutral-atmospheric-surface-layer-turbul/>), and Mann (1998), "Wind field simulation", DOI `10.1016/S0266-8920(97)00036-2` (<https://www.sciencedirect.com/science/article/abs/pii/S0266892097000362>).

## Requested large case

Target:

- `NumPointY = 100`
- `NumPointZ = 100`
- `WindDuration = 660 s`
- `TimeStep = 0.05 s`

Derived SimWind dimensions:

- Spatial points: `Np = 100 * 100 = 10,000`.
- Requested time steps: `660 / 0.05 = 13,200`.
- SimWind time steps: `Nt = 13,200`.
- Positive frequency bins: `6,599`.

Current generated-case cost estimate:

- The Bladed Kaimal large-grid demo has IEC coherence only on `u`; `v/w` are synthesized with uncorrelated spatial phases, matching the current TurbSim-style default coherence settings.
- For the `u` component, WindL uses the legacy Kronecker coherence acceleration for the first `2,863` positive frequencies and then diagonal high-frequency synthesis.
- SimWind preflight estimate: peak memory `4.92 GiB`; Cholesky work `1.909e9` FLOPs.
- One component spectrum array: `13,200 * 10,000 * sizeof(complex<double>)`, about `1.97 GiB`.
- Three-component wind field: `3 * 13,200 * 10,000 * sizeof(double)`, about `2.95 GiB`.

Measured Release run:

- Command: `build\release\Qahse.exe --qwd demo\WindL\Qahse_WindL_LargeGrid_100x100_PERF_ONLY.qwd`.
- Machine run completed successfully in `186.27 s` wall time; SimWind reported `184.12 CPU seconds used`.
- Console output included preflight memory/FLOP estimates, component-stage progress, frequency progress, FFT stage progress, copy-out progress, and ETA.
- Python validation command passed:
  - `python demo\WindL\validate_simwind_output.py --base demo\WindL\result\SimWind_LargeGrid_100x100_PERF_ONLY --tolerance 0.50 --plot-dir demo\WindL\result\SimWind_LargeGrid_100x100_validation --json demo\WindL\result\SimWind_LargeGrid_100x100_validation.json`

Output size if generated with all three binary outputs:

- One 3-component full-field int16 file: `3 * 2 * 10,000 * 13,200 = 792,000,000 bytes`, about `755 MiB`, plus header.
- Generated files:
  - `.bts`: `792,000,089 bytes`
  - Bladed `.wnd`: `792,000,092 bytes`
  - TurbSim-compatible `.ts.wnd`: `792,000,104 bytes`
  - `.sum`: `1,407 bytes`

Validation summary:

- BTS physical statistics passed the validation tolerance: `u` sigma `2.3754` vs target `1.981` (`19.9%` high with `ScaleIEC=0`), `v` sigma `1.5490` vs `1.5848`, `w` sigma `0.9691` vs `0.9905`.
- Bladed `.wnd` normalized component standard deviations were all within `6e-8` of `1.0`.
- Plots were written to `demo\WindL\result\SimWind_LargeGrid_100x100_validation`.
