# Large-grid 100x100 model comparison (raw)

Cases:
- Bladed von Karman
- Bladed improved von Karman
- Mann

Grid/time setup:
- 100 x 100 grid
- dt = 0.05000 s
- Tmax = 660.00 s
- case set = raw

| Case | ScaleIEC | target sigma u/v/w [m/s] | hub sigma u/v/w [m/s] | hub TI u/v/w [%] | u PSD peak [Hz] | v PSD peak [Hz] | w PSD peak [Hz] |
| --- | ---: | --- | --- | --- | ---: | ---: | ---: |
| BladedVK | 0.0 | 2.0440/1.6352/1.0220 | 2.2736/1.7920/0.9227 | 18.947/14.933/7.689 | 0.0146 | 0.0098 | 0.0171 |
| BladedIVK | 0.0 | 1.1828/0.9329/0.6717 | 1.2049/0.9714/0.6647 | 10.041/8.095/5.539 | 0.0244 | 0.1392 | 0.0244 |
| Mann | 0.0 | 2.0440/1.6352/1.0220 | 9.5573/2.4365/2.3866 | 79.644/20.304/19.888 | 0.0244 | 0.0317 | 0.0464 |

Generated plots:
- `F:\Qahse.Clear\demo\SimWind\result\large_grid_compare\LG100_raw_hub_timeseries_overlay.png`
- `F:\Qahse.Clear\demo\SimWind\result\large_grid_compare\LG100_raw_hub_psd_overlay.png`
- `F:\Qahse.Clear\demo\SimWind\result\large_grid_compare\LG100_raw_hub_sigma_ti_comparison.png`
