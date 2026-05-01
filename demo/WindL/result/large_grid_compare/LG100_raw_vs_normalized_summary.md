# LG100 raw vs normalized comparison

Grid/time setup:
- `100 x 100`
- `dt = 0.05 s`
- `Tmax = 660 s`
- hub point extracted from BTS header geometry, not array midpoint

## Case sets

- `raw`
  - Input files:
    - `demo/WindL/Qahse_WindL_LargeGrid_100x100_BladedVK_RAW.qwd`
    - `demo/WindL/Qahse_WindL_LargeGrid_100x100_BladedIVK_RAW.qwd`
    - `demo/WindL/Qahse_WindL_LargeGrid_100x100_Mann_RAW.qwd`
  - Meaning:
    - `ScaleIEC = 0`
    - compare natural model energy and native spectral shape

- `normalized`
  - Input files:
    - `demo/WindL/Qahse_WindL_LargeGrid_100x100_BladedVK_NORMALIZED.qwd`
    - `demo/WindL/Qahse_WindL_LargeGrid_100x100_BladedIVK_NORMALIZED.qwd`
    - `demo/WindL/Qahse_WindL_LargeGrid_100x100_Mann_NORMALIZED.qwd`
  - Meaning:
    - `ScaleIEC = 1`
    - explicit common TI targets:
      - `TI_u = 17.0333333333 %`
      - `TI_v = 13.6266666667 %`
      - `TI_w = 8.5166666667 %`
    - compare spectral shape under the same final turbulence-intensity target

## Hub sigma / TI results

### raw

| Model | u sigma | v sigma | w sigma | u TI [%] | v TI [%] | w TI [%] |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| BladedVK | 2.2736 | 1.7920 | 0.9227 | 18.947 | 14.933 | 7.689 |
| BladedIVK | 1.2049 | 0.9714 | 0.6647 | 10.041 | 8.095 | 5.539 |
| Mann | 9.5573 | 2.4365 | 2.3866 | 79.644 | 20.304 | 19.888 |

Interpretation:
- `raw` shows native model energy.
- `BladedIVK` is naturally weaker than `BladedVK` for this atmospheric setup.
- `Mann` is much stronger because `ScaleIEC=0` keeps its absolute energy path based on `MannAlphaEps`.

### normalized

| Model | u sigma | v sigma | w sigma | u TI [%] | v TI [%] | w TI [%] |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| BladedVK | 2.0440 | 1.6352 | 1.0220 | 17.033 | 13.627 | 8.517 |
| BladedIVK | 2.0426 | 1.6340 | 1.0213 | 17.021 | 13.617 | 8.511 |
| Mann | 2.0440 | 1.6352 | 1.0220 | 17.033 | 13.627 | 8.517 |

Interpretation:
- `normalized` suppresses native energy differences and makes the three models comparable on spectral shape.
- `BladedIVK` is now very close to the explicit common target; the remaining mismatch is about `0.07 %`.
- `BladedVK` and `Mann` match the common TI target essentially exactly.

## PSD peak frequencies at hub

### raw

| Model | u peak [Hz] | v peak [Hz] | w peak [Hz] |
| --- | ---: | ---: | ---: |
| BladedVK | 0.0146 | 0.0098 | 0.0171 |
| BladedIVK | 0.0244 | 0.1392 | 0.0244 |
| Mann | 0.0244 | 0.0317 | 0.0464 |

### normalized

| Model | u peak [Hz] | v peak [Hz] | w peak [Hz] |
| --- | ---: | ---: | ---: |
| BladedVK | 0.0146 | 0.0098 | 0.0171 |
| BladedIVK | 0.0244 | 0.1392 | 0.1733 |
| Mann | 0.0244 | 0.0317 | 0.0464 |

Interpretation:
- After common-TI normalization, PSD peak locations still differ.
- That is the correct signal for model discrimination; the TI bar chart alone is not.

## Artifacts

- raw report:
  - `demo/WindL/result/large_grid_compare/raw/LG100_raw_model_compare_report.md`
- raw plots:
  - `demo/WindL/result/large_grid_compare/raw/LG100_raw_hub_timeseries_overlay.png`
  - `demo/WindL/result/large_grid_compare/raw/LG100_raw_hub_psd_overlay.png`
  - `demo/WindL/result/large_grid_compare/raw/LG100_raw_hub_sigma_ti_comparison.png`

- normalized report:
  - `demo/WindL/result/large_grid_compare/normalized/LG100_normalized_model_compare_report.md`
- normalized plots:
  - `demo/WindL/result/large_grid_compare/normalized/LG100_normalized_hub_timeseries_overlay.png`
  - `demo/WindL/result/large_grid_compare/normalized/LG100_normalized_hub_psd_overlay.png`
  - `demo/WindL/result/large_grid_compare/normalized/LG100_normalized_hub_sigma_ti_comparison.png`
