# Large-grid 100x100 model comparison (default)

Cases:
- Bladed von Karman
- Bladed improved von Karman
- Mann

Grid/time setup:
- 100 x 100 grid
- dt = 0.05000 s
- Tmax = 660.00 s
- case set = default

| Case | ScaleIEC | target sigma u/v/w [m/s] | hub sigma u/v/w [m/s] | hub TI u/v/w [%] | u PSD peak [Hz] | v PSD peak [Hz] | w PSD peak [Hz] |
| --- | ---: | --- | --- | --- | ---: | ---: | ---: |
| BladedVK | 1.0 | 2.0440/1.6352/1.0220 | 2.0440/1.6352/1.0220 | 17.033/13.627/8.517 | 0.0146 | 0.0098 | 0.0171 |
| BladedIVK | 1.0 | 1.1828/0.9329/0.6717 | 1.1840/0.9338/0.6722 | 9.867/7.782/5.602 | 0.0244 | 0.1392 | 0.0244 |
| Mann | 1.0 | 2.0440/1.6352/1.0220 | 2.0440/1.6352/1.0220 | 17.033/13.627/8.517 | 0.0244 | 0.0317 | 0.0464 |

Generated plots:
- `F:\Qahse.Clear\demo\SimWind\result\large_grid_compare\LG100_hub_timeseries_overlay.png`
- `F:\Qahse.Clear\demo\SimWind\result\large_grid_compare\LG100_hub_psd_overlay.png`
- `F:\Qahse.Clear\demo\SimWind\result\large_grid_compare\LG100_hub_sigma_ti_comparison.png`
