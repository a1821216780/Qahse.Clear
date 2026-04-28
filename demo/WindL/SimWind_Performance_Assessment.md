# WindL SimWind demo cases and 100x100 performance assessment

## Demo cases

| File | TurbModel | Main purpose |
| --- | ---: | --- |
| `Qahse_WindL_Bladed_Kaimal_DEMO.qwd` | 3 | Bladed General Kaimal header (`Record 2 = 7`) plus `.bts` and TurbSim-compatible `.ts.wnd`. |
| `Qahse_WindL_Bladed_ImprovedVonKarman_DEMO.qwd` | 5 | Bladed improved von Karman header (`Record 2 = 4`) with explicit length scales and TI values. |
| `Qahse_WindL_Mann_DEMO.qwd` | 2 | Bladed Mann header (`Record 2 = 8`) with Mann length/gamma/max wavelength fields. |
| `Qahse_WindL_LargeGrid_100x100_PERF_ONLY.qwd` | 3 | Records the requested 100 x 100, 660 s, 0.05 s case. SimWind preflight refuses this strict-coherence run by default. |

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
- SimWind FFT-padded steps: `Nt = 16,384`.
- Positive frequency bins used by the strict-coherence loop: about `8,191` per velocity component.

Current strict-coherence cost:

- One full double coherence matrix: `Np^2 * 8 = 800,000,000 bytes`, about `763 MiB`.
- Cholesky lower matrix: another about `763 MiB`.
- The current Cholesky helper takes the matrix by value, so peak matrix storage is about three such matrices during factorization, before spectrum and wind-field arrays.
- One component spectrum array: `Nt * Np * sizeof(complex<double>) = 2.44 GiB`.
- Three-component wind field: `3 * Nt * Np * sizeof(double) = 3.66 GiB`.
- Practical peak memory is therefore above `8 GiB` before allocator overhead, file buffers, and temporary vectors.

Operation count:

- Dense Cholesky cost per frequency is roughly `Np^3 / 3 = 3.33e11` floating-point operations.
- Per component: `3.33e11 * 8191 = 2.73e15` floating-point operations.
- For three components: about `8.19e15` floating-point operations.
- At an ideal sustained `1 TFLOP/s`, Cholesky alone is about `2.3 h`; at `100 GFLOP/s`, about `22.8 h`. The current portable C++ implementation is not a blocked BLAS/LAPACK implementation, so real runtime is expected to be much worse and memory/cache limited.

Output size if generated with all three binary outputs:

- One 3-component full-field int16 file at padded length: `3 * 2 * 10,000 * 16,384 = 983,040,000 bytes`, about `938 MiB`.
- `.bts`, Bladed `.wnd`, and TurbSim-compatible `.ts.wnd` together would be about `2.8 GiB`, plus headers and `.sum`.

Conclusion:

- The 100 x 100, 660 s, 0.05 s case is not practical with the current strict full-coherence Cholesky implementation.
- For production-scale 100 x 100 grids, the implementation needs a dedicated high-performance path: blocked threaded LAPACK/BLAS, reduced temporary copies, streaming output, and likely a mathematically different synthesis method for Mann/Bladed spectral-tensor grids. That would change the performance profile but is outside this strict-coherence path.
