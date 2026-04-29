# SimWind Phase-2 Validation Report

- Cases: 4 + 1 API rejection check
- Passed checks: 31/31

## Case Results

### UserSpectraScaled
- Passed: True
- Base: `F:\Qahse.Clear\demo\WindL\result\phase2\Qahse_WindL_User_Spectra_Scaled_DEMO`
- Extra check `no_legacy_unimplemented_warning`: PASS
- Extra check `user_spectra_u_psd_shape`: PASS
- Extra check `user_spectra_v_psd_shape`: PASS
- Extra check `user_spectra_w_psd_shape`: PASS
- Plots:
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\UserSpectraScaled\Qahse_WindL_User_Spectra_Scaled_DEMO_hub_timeseries.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\UserSpectraScaled\Qahse_WindL_User_Spectra_Scaled_DEMO_hub_psd.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\UserSpectraScaled\Qahse_WindL_User_Spectra_Scaled_DEMO_u_first_plane.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\UserSpectraScaled\Qahse_WindL_User_Spectra_Scaled_DEMO_hub_covariance.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\UserSpectraScaled\Qahse_WindL_User_Spectra_Scaled_DEMO_histograms.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\UserSpectraScaled\UserSpectraScaled_target_psd.png`

### USRVKM
- Passed: True
- Base: `F:\Qahse.Clear\demo\WindL\result\phase2\Qahse_WindL_USRVKM_DEMO`
- Base check `u_sigma_matches_sum_target`: PASS
- Base check `v_sigma_matches_sum_target`: PASS
- Base check `w_sigma_matches_sum_target`: PASS
- Extra check `no_legacy_unimplemented_warning`: PASS
- Extra check `usrvkm_u_sigma_profile`: PASS
- Extra check `usrvkm_u_ti_profile`: PASS
- Extra check `usrvkm_v_sigma_profile`: PASS
- Extra check `usrvkm_v_ti_profile`: PASS
- Extra check `usrvkm_w_sigma_profile`: PASS
- Extra check `usrvkm_w_ti_profile`: PASS
- Extra check `usrvkm_direction_profile`: PASS
- Extra check `usrvkm_u_hub_psd`: PASS
- Extra check `usrvkm_v_hub_psd`: PASS
- Extra check `usrvkm_w_hub_psd`: PASS
- Plots:
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\USRVKM\Qahse_WindL_USRVKM_DEMO_hub_timeseries.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\USRVKM\Qahse_WindL_USRVKM_DEMO_hub_psd.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\USRVKM\Qahse_WindL_USRVKM_DEMO_u_first_plane.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\USRVKM\Qahse_WindL_USRVKM_DEMO_hub_covariance.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\USRVKM\Qahse_WindL_USRVKM_DEMO_histograms.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\USRVKM\USRVKM_ti_profile.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\USRVKM\USRVKM_sigma_profile.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\USRVKM\USRVKM_direction_profile.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\USRVKM\USRVKM_hub_target_psd.png`

### IecKaimalStability
- Passed: True
- Base: `F:\Qahse.Clear\demo\WindL\result\phase2\Qahse_WindL_IEC_Kaimal_Stability_DEMO`
- Base check `u_sigma_matches_sum_target`: PASS
- Base check `v_sigma_matches_sum_target`: PASS
- Base check `w_sigma_matches_sum_target`: PASS
- Extra check `no_legacy_unimplemented_warning`: PASS
- Extra check `stability_mean_profile`: PASS
- Extra check `stability_summary_zl`: PASS
- Extra check `stability_summary_zi`: PASS
- Extra check `stability_reynolds_default_enabled`: PASS
- Extra check `stability_default_uw_target`: PASS
- Plots:
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\IecKaimalStability\Qahse_WindL_IEC_Kaimal_Stability_DEMO_hub_timeseries.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\IecKaimalStability\Qahse_WindL_IEC_Kaimal_Stability_DEMO_hub_psd.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\IecKaimalStability\Qahse_WindL_IEC_Kaimal_Stability_DEMO_u_first_plane.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\IecKaimalStability\Qahse_WindL_IEC_Kaimal_Stability_DEMO_hub_covariance.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\IecKaimalStability\Qahse_WindL_IEC_Kaimal_Stability_DEMO_histograms.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\IecKaimalStability\IecKaimalStability_mean_profile.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\IecKaimalStability\IecKaimalStability_covariance_compare.png`

### MannReynoldsStress
- Passed: True
- Base: `F:\Qahse.Clear\demo\WindL\result\phase2\Qahse_WindL_Mann_ReynoldsStress_DEMO`
- Extra check `no_legacy_unimplemented_warning`: PASS
- Extra check `mann_pc_uw`: PASS
- Extra check `mann_reynolds_not_clipped`: PASS
- Plots:
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\MannReynoldsStress\Qahse_WindL_Mann_ReynoldsStress_DEMO_hub_timeseries.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\MannReynoldsStress\Qahse_WindL_Mann_ReynoldsStress_DEMO_hub_psd.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\MannReynoldsStress\Qahse_WindL_Mann_ReynoldsStress_DEMO_u_first_plane.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\MannReynoldsStress\Qahse_WindL_Mann_ReynoldsStress_DEMO_hub_covariance.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\MannReynoldsStress\Qahse_WindL_Mann_ReynoldsStress_DEMO_histograms.png`
  - `F:\Qahse.Clear\demo\WindL\phase2_validation\plots\MannReynoldsStress\MannReynoldsStress_covariance_compare.png`

## API Rejection

- `api_invalid_v_component_rejected`: PASS
- Return code: 1

