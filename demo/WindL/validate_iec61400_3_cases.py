#!/usr/bin/env python3
"""Certification-style validation for WindL IEC wind conditions and spectra.

The default profile uses:
- 30 x 30 full-field grid
- dt = 0.05 s
- duration = 660 s

It validates:
- hub-point sigma/TI for u/v/w against IEC targets
- hub-point PSD shape against analytical target spectra where an IEC/Bladed
  one-dimensional reference exists
- deterministic event time histories and steady profiles
- output readability through BTS/optional WND readers

This script generates a machine-readable JSON summary and a human-readable
Markdown report that can be used as validation evidence. It is not a formal
certification by itself.
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

import numpy as np

try:
    import matplotlib.pyplot as plt
except Exception:  # pragma: no cover
    plt = None

try:
    from scipy.signal import welch
except Exception as exc:  # pragma: no cover
    raise RuntimeError("scipy is required for certification PSD validation") from exc

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from validate_simwind_output import make_plots, parse_sum, read_bts, validate  # noqa: E402


TURB_MODEL = {
    "IEC_KAIMAL": 0,
    "IEC_VKAIMAL": 1,
    "B_MANN": 2,
    "B_KAL": 3,
    "B_VKAL": 4,
    "B_IVKAL": 5,
    "USRVKM": 8,
}

WIND_MODEL = {
    "NTM": 0,
    "ETM": 1,
    "EWM1": 2,
    "EWM50": 3,
    "EOG": 4,
    "EDC": 5,
    "ECD": 6,
    "EWS": 7,
    "UNIFORM": 8,
}

EWM_TYPE = {"TURBULENT": 0, "STEADY": 1}
TURB_CLASS = {"A": 0, "B": 1, "C": 2}
TURBINE_CLASS = {"I": 0, "II": 1, "III": 2, "S": 3}


@dataclass
class CampaignConfig:
    grid_points: int = 30
    time_step: float = 0.05
    duration: float = 660.0
    hub_height: float = 90.0
    ref_height: float = 90.0
    rotor_diameter: float = 90.0
    field_dim_y: float = 90.0
    field_dim_z: float = 90.0
    mean_wind_speed: float = 12.0
    v_ref: float = 50.0
    iec_edition: int = 1
    turbine_class: str = "I"
    turbulence_class: str = "B"
    scale_iec: int = 1
    mann_nx: int = 16384
    mann_ny: int = 32
    mann_nz: int = 32
    gust_period: float = 10.5
    event_start: float = 300.0
    event_sign: int = 0
    ecd_vcog: float = 15.0
    random_seed: int = 260429


@dataclass
class Case:
    name: str
    group: str
    turb_model: str = "IEC_KAIMAL"
    wind_model: str = "NTM"
    ewm_type: str = "TURBULENT"
    wr_bladed: bool = False
    expect_turbulence: bool = True
    psd_reference: str | None = None
    event_reference: str | None = None
    profile_reference: str | None = None
    include_in_certification: bool = True
    extras: dict[str, Any] = field(default_factory=dict)
    notes: list[str] = field(default_factory=list)


def bool_text(value: bool) -> str:
    return "true" if value else "false"


def default_vref(turbine_class: str) -> float:
    return {"I": 50.0, "II": 42.5, "III": 37.5, "S": 50.0}[turbine_class]


def turbulence_class_iref(turbulence_class: str) -> float:
    return {"A": 0.16, "B": 0.14, "C": 0.12}[turbulence_class]


def extreme_wind_speed_50(cfg: CampaignConfig) -> float:
    return 1.4 * (cfg.v_ref if cfg.v_ref > 0.0 else default_vref(cfg.turbine_class))


def extreme_wind_speed_1(cfg: CampaignConfig) -> float:
    return 0.8 * extreme_wind_speed_50(cfg)


def lambda_iec(cfg: CampaignConfig) -> float:
    if cfg.iec_edition == 0:
        return 0.7 * cfg.hub_height if cfg.hub_height < 30.0 else 21.0
    return 0.7 * cfg.hub_height if cfg.hub_height < 60.0 else 42.0


def lc_iec(cfg: CampaignConfig) -> float:
    lam = lambda_iec(cfg)
    return 3.5 * lam if cfg.iec_edition == 0 else 8.1 * lam


def reference_hub_speed(case: Case, cfg: CampaignConfig) -> float:
    if case.wind_model == "EWM1" and case.ewm_type == "STEADY":
        return extreme_wind_speed_1(cfg)
    if case.wind_model == "EWM50" and case.ewm_type == "STEADY":
        return extreme_wind_speed_50(cfg)
    return cfg.mean_wind_speed


def sigma_targets(case: Case, cfg: CampaignConfig) -> list[float]:
    u_hub = reference_hub_speed(case, cfg)
    iref = turbulence_class_iref(cfg.turbulence_class)

    if case.wind_model in {"EOG", "EDC", "ECD", "EWS", "UNIFORM"}:
        return [0.0, 0.0, 0.0]

    if case.wind_model in {"EWM1", "EWM50"} and case.ewm_type == "STEADY":
        return [0.0, 0.0, 0.0]

    if case.wind_model == "ETM":
        c = float(case.extras.get("ETMc", 2.0))
        v_ave = 0.2 * (cfg.v_ref if cfg.v_ref > 0.0 else default_vref(cfg.turbine_class))
        sigma_u = c * iref * (0.072 * (v_ave / c + 3.0) * (u_hub / c - 4.0) + 10.0)
    elif case.wind_model in {"EWM1", "EWM50"}:
        sigma_u = 0.11 * extreme_wind_speed_50(cfg)
    else:
        sigma_u = iref * (0.75 * u_hub + 5.6)

    if case.turb_model == "IEC_VKAIMAL":
        return [sigma_u, sigma_u, sigma_u]

    sigma_v = float(case.extras.get("TI_v", 0.0)) * u_hub if float(case.extras.get("TI_v", 0.0)) > 0.0 else 0.8 * sigma_u
    sigma_w = float(case.extras.get("TI_w", 0.0)) * u_hub if float(case.extras.get("TI_w", 0.0)) > 0.0 else 0.5 * sigma_u
    return [sigma_u, sigma_v, sigma_w]


def integral_scales(case: Case, cfg: CampaignConfig) -> list[float]:
    lam = lambda_iec(cfg)
    if case.turb_model in {"IEC_VKAIMAL", "B_VKAL", "B_IVKAL"}:
        default = [3.5 * lam, 3.5 * lam, 3.5 * lam]
    elif case.turb_model == "B_MANN":
        mann_length = float(case.extras.get("MannScalelength", 33.6))
        default = [mann_length, mann_length, mann_length]
    else:
        default = [8.1 * lam, 2.7 * lam, 0.66 * lam]

    overrides = [
        float(case.extras.get("VxLu", 0.0)),
        float(case.extras.get("VxLv", 0.0)),
        float(case.extras.get("VxLw", 0.0)),
    ]
    return [override if override > 0.0 else base for override, base in zip(overrides, default)]


def kaimal_psd(freq: np.ndarray, sigma: float, length_scale: float, u_hub: float) -> np.ndarray:
    l_over_u = max(length_scale, 1.0e-12) / max(u_hub, 0.1)
    return 4.0 * sigma * sigma * l_over_u / np.power(1.0 + 6.0 * l_over_u * freq, 5.0 / 3.0)


def von_karman_psd(freq: np.ndarray, sigma: float, length_scale: float, u_hub: float, component: int) -> np.ndarray:
    l_over_u = max(length_scale, 1.0e-12) / max(u_hub, 0.1)
    flu2 = np.square(freq * l_over_u)
    sigma_l = 2.0 * sigma * sigma * l_over_u
    if component == 0:
        return 2.0 * sigma_l / np.power(1.0 + 71.0 * flu2, 5.0 / 6.0)
    return sigma_l * (1.0 + 189.0 * flu2) / np.power(1.0 + 71.0 * flu2, 11.0 / 6.0)


def target_psd_function(case: Case, cfg: CampaignConfig, component: int) -> Callable[[np.ndarray], np.ndarray] | None:
    if not case.expect_turbulence or case.psd_reference is None:
        return None

    sigmas = sigma_targets(case, cfg)
    scales = integral_scales(case, cfg)
    u_hub = reference_hub_speed(case, cfg)
    sigma = sigmas[component]
    scale = scales[component]

    if case.psd_reference in {"IEC_KAIMAL", "B_KAL"}:
        return lambda freq: kaimal_psd(freq, sigma, scale, u_hub)
    if case.psd_reference in {"IEC_VKAIMAL", "B_VKAL", "B_IVKAL"}:
        return lambda freq: von_karman_psd(freq, sigma, scale, u_hub, component)
    return None


def qwd_text(case: Case, cfg: CampaignConfig, output_dir: Path) -> str:
    event_start = cfg.event_start
    if event_start + cfg.gust_period > cfg.duration:
        event_start = max(0.0, 0.5 * max(cfg.duration - cfg.gust_period, 0.0))

    values: dict[str, Any] = {
        "Mode": 0,
        "TurbModel": TURB_MODEL[case.turb_model],
        "WindModel": WIND_MODEL[case.wind_model],
        "CalWu": True,
        "CalWv": True,
        "CalWw": True,
        "WrBlwnd": case.wr_bladed,
        "WrTrbts": True,
        "WrTrwnd": False,
        "IECStandard": cfg.iec_edition,
        "IECClass": TURBINE_CLASS[cfg.turbine_class],
        "TurbulenceClass": TURB_CLASS[cfg.turbulence_class],
        "VRef": cfg.v_ref,
        "RotorDiameter": cfg.rotor_diameter,
        "MeanWindSpeed": cfg.mean_wind_speed,
        "HubHt": cfg.hub_height,
        "RefHt": cfg.ref_height,
        "ShearType": 0,
        "WindProfileType": 1,
        "PLExp": 0.2,
        "Roughness": 0.01,
        "TurbIntensity": 0.0,
        "RandSeed": cfg.random_seed,
        "NumPointY": cfg.grid_points,
        "NumPointZ": cfg.grid_points,
        "LenWidthY": cfg.field_dim_y,
        "LenHeightZ": cfg.field_dim_z,
        "WindDuration": cfg.duration,
        "TimeStep": cfg.time_step,
        "CycleWind": False,
        "UseFFT": True,
        "UseIECSimmga": False,
        "ScaleIEC": cfg.scale_iec,
        "ETMc": 2.0,
        "MannAlphaEps": 1.0,
        "MannScalelength": 33.6,
        "MannGamma": 3.9,
        "MannMaxL": 268.8,
        "MannNx": cfg.mann_nx,
        "MannNy": cfg.mann_ny,
        "MannNz": cfg.mann_nz,
        "CohMod1": "default",
        "CohMod2": "default",
        "CohMod3": "default",
        "EWMType": EWM_TYPE[case.ewm_type],
        "EWMReturn": 0,
        "GustPeriod": cfg.gust_period,
        "EventStart": event_start,
        "EventSign": cfg.event_sign,
        "EcdVcog": cfg.ecd_vcog,
        "WrWndPath": str(output_dir),
        "WrWndName": case.name,
        "SumPrint": True,
    }
    values.update(case.extras)

    lines = [f"-- Auto-generated WindL IEC certification case: {case.name}"]
    for key, value in values.items():
        if isinstance(value, bool):
            rendered = bool_text(value)
        elif isinstance(value, str):
            rendered = value if value == "default" else json.dumps(value)
        else:
            rendered = value
        lines.append(f"{rendered} {key}")
    return "\n".join(str(line) for line in lines) + "\n"


def default_cases() -> list[Case]:
    return [
        Case("IEC3_NTM_IECKAI_30x30_660s", "iec_wind_condition", wr_bladed=False, psd_reference="IEC_KAIMAL"),
        Case("IEC3_ETM_IECKAI_30x30_660s", "iec_wind_condition", wind_model="ETM", wr_bladed=False, psd_reference="IEC_KAIMAL"),
        Case("IEC3_EWM1_Turb_30x30_660s", "iec_wind_condition", wind_model="EWM1", ewm_type="TURBULENT", wr_bladed=False, psd_reference="IEC_KAIMAL"),
        Case("IEC3_EWM50_Turb_30x30_660s", "iec_wind_condition", wind_model="EWM50", ewm_type="TURBULENT", wr_bladed=False, psd_reference="IEC_KAIMAL"),
        Case("IEC3_EWM1_Steady_30x30_660s", "iec_wind_condition", wind_model="EWM1", ewm_type="STEADY", expect_turbulence=False, profile_reference="EWM_STEADY"),
        Case("IEC3_EWM50_Steady_30x30_660s", "iec_wind_condition", wind_model="EWM50", ewm_type="STEADY", expect_turbulence=False, profile_reference="EWM_STEADY"),
        Case("IEC3_EOG_30x30_660s", "iec_wind_condition", wind_model="EOG", expect_turbulence=False, event_reference="EOG"),
        Case("IEC3_EDC_30x30_660s", "iec_wind_condition", wind_model="EDC", expect_turbulence=False, event_reference="EDC"),
        Case("IEC3_ECD_30x30_660s", "iec_wind_condition", wind_model="ECD", expect_turbulence=False, event_reference="ECD"),
        Case("IEC3_EWS_30x30_660s", "iec_wind_condition", wind_model="EWS", expect_turbulence=False, event_reference="EWS"),
        Case("IEC3_UNIFORM_30x30_660s", "iec_wind_condition", wind_model="UNIFORM", expect_turbulence=False, profile_reference="UNIFORM"),
        Case("Spectrum_IECVKM_30x30_660s", "spectral_model_audit", turb_model="IEC_VKAIMAL", wr_bladed=True, psd_reference="IEC_VKAIMAL", include_in_certification=False),
        Case("Spectrum_BladedKaimal_30x30_660s", "spectral_model_audit", turb_model="B_KAL", wr_bladed=True, psd_reference="B_KAL", include_in_certification=False),
        Case("Spectrum_BladedVK_30x30_660s", "spectral_model_audit", turb_model="B_VKAL", wr_bladed=True, psd_reference="B_VKAL", include_in_certification=False),
        Case("Spectrum_BladedIVK_30x30_660s", "spectral_model_audit", turb_model="B_IVKAL", wr_bladed=True, psd_reference="B_IVKAL", include_in_certification=False),
        Case(
            "Spectrum_Mann_30x30_660s",
            "spectral_model_audit",
            turb_model="B_MANN",
            wr_bladed=True,
            psd_reference=None,
            include_in_certification=False,
            notes=["Mann is not an IEC 61400-3 normative 1D spectral formula case; PSD certification is not claimed."],
        ),
    ]


def resolve_exe(path: str | None) -> Path:
    if path:
        return Path(path).resolve()
    for candidate in (REPO_ROOT / "build" / "release" / "Qahse.exe", REPO_ROOT / "build" / "debug" / "Qahse.exe"):
        if candidate.exists():
            return candidate
    return REPO_ROOT / "build" / "release" / "Qahse.exe"


def grid_coordinates(bts: dict) -> tuple[np.ndarray, np.ndarray]:
    header = bts["header"]
    nz = int(header["nz"])
    ny = int(header["ny"])
    dz = float(header["dz"])
    dy = float(header["dy"])
    z_bottom = float(header["zbottom"])
    z = z_bottom + dz * np.arange(nz, dtype=float)
    y = -0.5 * dy * (ny - 1) + dy * np.arange(ny, dtype=float)
    return y, z


def hub_indices(bts: dict) -> tuple[int, int]:
    data = bts["data"]
    y, z = grid_coordinates(bts)
    hub_height = float(bts["header"]["hub_height"])
    y_delta = np.abs(y)
    z_delta = np.abs(z - hub_height)
    y_min = float(np.min(y_delta))
    z_min = float(np.min(z_delta))
    iy_candidates = np.flatnonzero(y_delta <= y_min + 1.0e-6)
    iz_candidates = np.flatnonzero(z_delta <= z_min + 1.0e-6)
    iy = int(iy_candidates[0])
    iz = int(iz_candidates[0])
    return iy, iz


def hub_series(bts: dict, comp: int) -> np.ndarray:
    iy, iz = hub_indices(bts)
    return bts["data"][:, iz, iy, comp]


def mean_profile_u(bts: dict) -> np.ndarray:
    return bts["data"][..., 0].mean(axis=(0, 2))


def time_axis(bts: dict) -> np.ndarray:
    dt = float(bts["header"]["dt"])
    nt = int(bts["header"]["nt"])
    return dt * np.arange(nt, dtype=float)


def effective_event_start(cfg: CampaignConfig) -> float:
    if cfg.event_start + cfg.gust_period > cfg.duration:
        return max(0.0, 0.5 * max(cfg.duration - cfg.gust_period, 0.0))
    return cfg.event_start


def reference_sigma1_ntm(case: Case, cfg: CampaignConfig) -> float:
    u_hub = reference_hub_speed(case, cfg)
    return turbulence_class_iref(cfg.turbulence_class) * (0.75 * u_hub + 5.6)


def extreme_operating_gust_amplitude(case: Case, cfg: CampaignConfig) -> float:
    sigma1 = reference_sigma1_ntm(case, cfg)
    denom = 1.0 + 0.1 * cfg.rotor_diameter / max(lambda_iec(cfg), 1.0e-12)
    a = 1.35 * (extreme_wind_speed_1(cfg) - reference_hub_speed(case, cfg))
    b = 3.3 * sigma1 / denom
    return min(a, b)


def extreme_direction_change_deg(case: Case, cfg: CampaignConfig) -> float:
    sigma1 = reference_sigma1_ntm(case, cfg)
    denom = reference_hub_speed(case, cfg) * (1.0 + 0.1 * cfg.rotor_diameter / max(lambda_iec(cfg), 1.0e-12))
    theta = 4.0 * math.degrees(math.atan(sigma1 / max(denom, 1.0e-12)))
    return min(theta, 180.0)


def steady_profile(case: Case, cfg: CampaignConfig, z: np.ndarray) -> np.ndarray:
    if case.profile_reference == "UNIFORM":
        return np.full_like(z, cfg.mean_wind_speed, dtype=float)
    if case.profile_reference == "EWM_STEADY":
        u_hub = reference_hub_speed(case, cfg)
        return u_hub * np.power(np.maximum(z, 0.05) / max(cfg.hub_height, 1.0e-12), 0.11)
    raise ValueError(f"Unsupported profile reference: {case.profile_reference}")


def eog_reference(case: Case, cfg: CampaignConfig, time_s: np.ndarray) -> np.ndarray:
    u_hub = reference_hub_speed(case, cfg)
    period = cfg.gust_period
    start = effective_event_start(cfg)
    sign = -1.0 if cfg.event_sign == 1 else 1.0
    amplitude = extreme_operating_gust_amplitude(case, cfg)
    local = time_s - start
    d_u = np.zeros_like(time_s)
    mask = (local >= 0.0) & (local <= period)
    d_u[mask] = (
        -sign
        * 0.37
        * amplitude
        * np.sin(3.0 * math.pi * local[mask] / period)
        * (1.0 - np.cos(2.0 * math.pi * local[mask] / period))
    )
    return u_hub + d_u


def edc_reference(case: Case, cfg: CampaignConfig, time_s: np.ndarray) -> np.ndarray:
    period = cfg.gust_period
    start = effective_event_start(cfg)
    sign = -1.0 if cfg.event_sign == 1 else 1.0
    theta_e = math.radians(extreme_direction_change_deg(case, cfg))
    local = time_s - start
    theta = np.zeros_like(time_s)
    mask = (local >= 0.0) & (local <= period)
    theta[mask] = 0.5 * theta_e * sign * (1.0 - np.cos(math.pi * local[mask] / period))
    theta[local > period] = theta_e * sign
    return np.degrees(theta)


def ecd_reference(case: Case, cfg: CampaignConfig, time_s: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    period = cfg.gust_period
    start = effective_event_start(cfg)
    sign = -1.0 if cfg.event_sign == 1 else 1.0
    u_hub = reference_hub_speed(case, cfg)
    v_cog = cfg.ecd_vcog
    theta_cg_deg = 180.0 if u_hub < 4.0 else 720.0 / max(u_hub, 1.0e-12)
    theta_cg = math.radians(theta_cg_deg) * sign
    local = time_s - start
    d_u = np.zeros_like(time_s)
    theta = np.zeros_like(time_s)
    mask = (local >= 0.0) & (local <= period)
    d_u[mask] = 0.5 * v_cog * (1.0 - np.cos(math.pi * local[mask] / period))
    theta[mask] = 0.5 * theta_cg * (1.0 - np.cos(math.pi * local[mask] / period))
    d_u[local > period] = v_cog
    theta[local > period] = theta_cg
    total_u = u_hub + d_u
    direction_deg = np.degrees(theta)
    return total_u, direction_deg


def ews_reference_plane(case: Case, cfg: CampaignConfig, time_value: float, y: np.ndarray, z: np.ndarray) -> np.ndarray:
    period = cfg.gust_period
    start = effective_event_start(cfg)
    sign = -1.0 if cfg.event_sign == 1 else 1.0
    sigma1 = reference_sigma1_ntm(case, cfg)
    denom = 1.0 + 0.1 * cfg.rotor_diameter / max(lambda_iec(cfg), 1.0e-12)
    local = time_value - start
    envelope = 0.0
    if 0.0 <= local <= period:
        envelope = 0.5 * (1.0 - math.cos(2.0 * math.pi * local / period))
    yy, zz = np.meshgrid(y, z)
    vert_shear = sign * 2.0 * sigma1 / denom * (zz - cfg.hub_height) / max(cfg.rotor_diameter, 1.0e-12) * envelope
    lat_shear = sign * 2.0 * sigma1 / denom * yy / max(cfg.rotor_diameter, 1.0e-12) * envelope
    profile_exp = 0.14 if cfg.iec_edition in (1, 2) else 0.2
    mean_u = cfg.mean_wind_speed * np.power(np.maximum(zz, 0.05) / max(cfg.ref_height, 1.0e-12), profile_exp)
    return mean_u + vert_shear + lat_shear


def psd_metrics(series: np.ndarray, dt: float, target_func: Callable[[np.ndarray], np.ndarray]) -> dict[str, Any]:
    detrended = np.asarray(series, dtype=float) - float(np.mean(series))
    nperseg = min(len(detrended), 4096)
    noverlap = nperseg // 2
    freq, measured = welch(
        detrended,
        fs=1.0 / dt,
        window="hann",
        nperseg=nperseg,
        noverlap=noverlap,
        detrend="constant",
        scaling="density",
    )
    target = target_func(freq)

    nyquist = 0.5 / dt
    fmin = max(2.0 / max(len(detrended) * dt, 1.0e-12), 0.005)
    fmax = min(2.0, 0.8 * nyquist)
    band = (freq >= fmin) & (freq <= fmax) & np.isfinite(measured) & np.isfinite(target) & (measured > 0.0) & (target > 0.0)
    if int(np.count_nonzero(band)) < 64:
        return {
            "passed": False,
            "reason": "insufficient_frequency_points",
            "freq": freq.tolist(),
            "measured": measured.tolist(),
            "target": target.tolist(),
        }

    band_freq = freq[band]
    band_measured = measured[band]
    band_target = target[band]
    edges = np.geomspace(float(band_freq[0]), float(band_freq[-1]), 13)

    ratios: list[float] = []
    centers: list[float] = []
    for f0, f1 in zip(edges[:-1], edges[1:]):
        mask = (band_freq >= f0) & (band_freq <= f1)
        if int(np.count_nonzero(mask)) < 3:
            continue
        m = float(np.mean(band_measured[mask]))
        t = float(np.mean(band_target[mask]))
        if m > 0.0 and t > 0.0:
            ratios.append(m / t)
            centers.append(math.sqrt(f0 * f1))

    log_ratios = np.log(np.asarray(ratios))
    shape_log_rmse = float(np.sqrt(np.mean(np.square(log_ratios))))
    shape_p90 = float(np.percentile(np.abs(log_ratios), 90))
    band_energy_rel_error = float(
        abs(np.trapezoid(band_measured, band_freq) - np.trapezoid(band_target, band_freq)) /
        max(np.trapezoid(band_target, band_freq), 1.0e-12)
    )
    passed = shape_log_rmse <= 0.35 and shape_p90 <= 0.75 and band_energy_rel_error <= 0.20
    return {
        "passed": passed,
        "freq": freq.tolist(),
        "measured": measured.tolist(),
        "target": target.tolist(),
        "band_freq": band_freq.tolist(),
        "band_measured": band_measured.tolist(),
        "band_target": band_target.tolist(),
        "bin_centers": centers,
        "bin_ratios": ratios,
        "shape_log_rmse": shape_log_rmse,
        "shape_p90_abs_log_ratio": shape_p90,
        "band_energy_relative_error": band_energy_rel_error,
        "fmin": float(band_freq[0]),
        "fmax": float(band_freq[-1]),
    }


def make_psd_plot(case_name: str, psd_report: dict[str, Any], output_dir: Path) -> str | None:
    if plt is None or not psd_report:
        return None

    names = ["u", "v", "w"]
    fig, axes = plt.subplots(3, 1, figsize=(8, 10), sharex=True)
    for idx, ax in enumerate(axes):
        item = psd_report.get(names[idx])
        if not item or "freq" not in item:
            ax.text(0.5, 0.5, "N/A", ha="center", va="center", transform=ax.transAxes)
            ax.set_ylabel(f"{names[idx]} PSD")
            continue
        freq = np.asarray(item["freq"], dtype=float)
        measured = np.asarray(item["measured"], dtype=float)
        target = np.asarray(item["target"], dtype=float)
        ax.loglog(freq[1:], measured[1:], label="Measured", linewidth=1.0)
        ax.loglog(freq[1:], target[1:], label="Target", linewidth=1.0)
        ax.set_ylabel(f"{names[idx]} PSD")
        ax.grid(True, which="both", alpha=0.25)
        ax.legend()
        if "shape_log_rmse" in item:
            ax.set_title(
                f"{names[idx]} PSD, log-RMSE={item['shape_log_rmse']:.3f}, "
                f"band err={100.0 * item['band_energy_relative_error']:.1f}%"
            )
    axes[-1].set_xlabel("frequency [Hz]")
    path = output_dir / f"{case_name}_psd_overlay.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


def make_ti_plot(case_name: str, measured_ti: list[float], target_ti: list[float], output_dir: Path) -> str | None:
    if plt is None:
        return None
    x = np.arange(3)
    width = 0.35
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.bar(x - width / 2, measured_ti, width, label="Measured")
    ax.bar(x + width / 2, target_ti, width, label="Target")
    ax.set_xticks(x)
    ax.set_xticklabels(["u", "v", "w"])
    ax.set_ylabel("TI [%]")
    ax.set_title("Hub-point turbulence intensity")
    ax.grid(True, axis="y", alpha=0.25)
    ax.legend()
    path = output_dir / f"{case_name}_ti_compare.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


def make_overview_plot(case_reports: list[dict[str, Any]], output_dir: Path) -> str | None:
    if plt is None:
        return None
    names = [Path(item["base"]).name for item in case_reports]
    ti_errors = [item.get("summary_metrics", {}).get("max_ti_relative_error_percent", 0.0) for item in case_reports]
    fig, ax = plt.subplots(figsize=(max(12, len(names) * 0.65), 5))
    ax.bar(np.arange(len(names)), ti_errors)
    ax.set_xticks(np.arange(len(names)))
    ax.set_xticklabels(names, rotation=45, ha="right")
    ax.set_ylabel("max TI relative error [%]")
    ax.set_title("IEC certification campaign: max hub TI error by case")
    ax.grid(True, axis="y", alpha=0.25)
    path = output_dir / "iec61400_3_ti_error_overview.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


def run_case(case: Case, cfg: CampaignConfig, exe: Path, output_dir: Path, case_dir: Path, plot_dir: Path, skip_run: bool) -> dict[str, Any]:
    qwd = case_dir / f"{case.name}.qwd"
    qwd.write_text(qwd_text(case, cfg, output_dir), encoding="utf-8")
    run: dict[str, Any] = {"qwd": str(qwd), "skipped": skip_run}

    if not skip_run:
        started = time.perf_counter()
        proc = subprocess.run(
            [str(exe), "--qwd", str(qwd)],
            cwd=str(REPO_ROOT),
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
        )
        run.update(
            {
                "returncode": proc.returncode,
                "elapsed_seconds": time.perf_counter() - started,
                "stdout_tail": proc.stdout[-4000:],
                "stderr_tail": proc.stderr[-4000:],
            }
        )
        if proc.returncode != 0:
            return {"base": str(output_dir / case.name), "case": case.__dict__, "run": run, "checks": [], "passed": False}

    base = output_dir / case.name
    report = validate(base, tolerance=0.50, plot_dir=plot_dir)
    report["case"] = case.__dict__
    report["run"] = run
    report["notes"] = list(case.notes)
    report["sum"] = parse_sum(base.with_suffix(".sum"))
    bts = read_bts(base.with_suffix(".bts"))
    report["bts_header"] = bts["header"]
    report["checks"] = list(report.get("checks", []))
    report["psd_checks"] = {}
    report["plots"] = list(report.get("plots", []))

    y, z = grid_coordinates(bts)
    time_s = time_axis(bts)
    u = hub_series(bts, 0)
    v = hub_series(bts, 1)
    w = hub_series(bts, 2)
    sigmas = sigma_targets(case, cfg)
    u_ref = reference_hub_speed(case, cfg)
    target_ti = [100.0 * sigma / max(u_ref, 1.0e-12) for sigma in sigmas]
    measured_ti = [100.0 * float(np.std(series)) / max(u_ref, 1.0e-12) for series in (u, v, w)]
    hub_stats = {
        "u_mean": float(np.mean(u)),
        "v_mean": float(np.mean(v)),
        "w_mean": float(np.mean(w)),
        "u_sigma": float(np.std(u)),
        "v_sigma": float(np.std(v)),
        "w_sigma": float(np.std(w)),
        "u_ti_percent_ref": measured_ti[0],
        "v_ti_percent_ref": measured_ti[1],
        "w_ti_percent_ref": measured_ti[2],
        "target_u_ti_percent": target_ti[0],
        "target_v_ti_percent": target_ti[1],
        "target_w_ti_percent": target_ti[2],
    }
    report["bts_hub_stats"] = hub_stats

    ti_plot = make_ti_plot(case.name, measured_ti, target_ti, plot_dir)
    if ti_plot:
        report["plots"].append(ti_plot)

    for comp, name, series in ((0, "u", u), (1, "v", v), (2, "w", w)):
        target_sigma = sigmas[comp]
        measured_sigma = float(np.std(series))
        sigma_rel = abs(measured_sigma - target_sigma) / target_sigma if target_sigma > 0.0 else 0.0
        target_ti_value = target_ti[comp]
        measured_ti_value = measured_ti[comp]
        ti_rel = abs(measured_ti_value - target_ti_value) / target_ti_value if target_ti_value > 0.0 else 0.0
        report["checks"].append(
            {
                "name": f"{name}_hub_sigma_matches_reference",
                "measured": measured_sigma,
                "target": target_sigma,
                "relative_error": sigma_rel,
                "passed": target_sigma == 0.0 or sigma_rel <= 0.05,
            }
        )
        report["checks"].append(
            {
                "name": f"{name}_hub_ti_matches_reference",
                "measured": measured_ti_value,
                "target": target_ti_value,
                "relative_error": ti_rel,
                "passed": target_ti_value == 0.0 or ti_rel <= 0.05,
            }
        )

        target_func = target_psd_function(case, cfg, comp)
        if target_func is not None:
            psd_report = psd_metrics(series, cfg.time_step, target_func)
            report["psd_checks"][name] = psd_report
            report["checks"].append(
                {
                    "name": f"{name}_psd_shape_matches_reference",
                    "measured": psd_report.get("shape_log_rmse"),
                    "target": "<= 0.35 log-RMSE and <= 20% band-energy error",
                    "passed": bool(psd_report.get("passed", False)),
                }
            )

    psd_plot = make_psd_plot(case.name, report["psd_checks"], plot_dir)
    if psd_plot:
        report["plots"].append(psd_plot)

    if case.profile_reference == "UNIFORM":
        profile = mean_profile_u(bts)
        target = steady_profile(case, cfg, z)
        max_abs = float(np.max(np.abs(profile - target)))
        report["checks"].append(
            {
                "name": "uniform_profile_matches_reference",
                "measured": max_abs,
                "target": "<= 0.05 m/s",
                "passed": max_abs <= 0.05,
            }
        )
    elif case.profile_reference == "EWM_STEADY":
        profile = mean_profile_u(bts)
        target = steady_profile(case, cfg, z)
        max_abs = float(np.max(np.abs(profile - target)))
        hub_mean_rel = abs(float(np.mean(u)) - u_ref) / max(u_ref, 1.0e-12)
        report["checks"].append(
            {
                "name": "steady_ewm_profile_matches_reference",
                "measured": max_abs,
                "target": "<= 0.15 m/s",
                "passed": max_abs <= 0.15,
            }
        )
        report["checks"].append(
            {
                "name": "steady_ewm_hub_mean_matches_reference",
                "measured": float(np.mean(u)),
                "target": u_ref,
                "relative_error": hub_mean_rel,
                "passed": hub_mean_rel <= 0.02,
            }
        )

    if case.event_reference == "EOG":
        target_u = eog_reference(case, cfg, time_s)
        max_abs = float(np.max(np.abs(u - target_u)))
        report["checks"].append(
            {
                "name": "eog_hub_timeseries_matches_reference",
                "measured": max_abs,
                "target": "<= 0.05 m/s",
                "passed": max_abs <= 0.05,
            }
        )
    elif case.event_reference == "EDC":
        target_theta = edc_reference(case, cfg, time_s)
        measured_theta = np.degrees(np.arctan2(v, u))
        max_abs = float(np.max(np.abs(measured_theta - target_theta)))
        report["checks"].append(
            {
                "name": "edc_direction_matches_reference",
                "measured": max_abs,
                "target": "<= 0.25 deg",
                "passed": max_abs <= 0.25,
            }
        )
    elif case.event_reference == "ECD":
        target_speed, target_theta = ecd_reference(case, cfg, time_s)
        measured_speed = np.sqrt(np.square(u) + np.square(v))
        measured_theta = np.degrees(np.arctan2(v, u))
        speed_err = float(np.max(np.abs(measured_speed - target_speed)))
        theta_err = float(np.max(np.abs(measured_theta - target_theta)))
        report["checks"].append(
            {
                "name": "ecd_speed_matches_reference",
                "measured": speed_err,
                "target": "<= 0.05 m/s",
                "passed": speed_err <= 0.05,
            }
        )
        report["checks"].append(
            {
                "name": "ecd_direction_matches_reference",
                "measured": theta_err,
                "target": "<= 0.25 deg",
                "passed": theta_err <= 0.25,
            }
        )
    elif case.event_reference == "EWS":
        peak_time = effective_event_start(cfg) + 0.5 * cfg.gust_period
        peak_index = int(np.argmin(np.abs(time_s - peak_time)))
        target_plane = ews_reference_plane(case, cfg, float(time_s[peak_index]), y, z)
        measured_plane = bts["data"][peak_index, :, :, 0]
        max_abs = float(np.max(np.abs(measured_plane - target_plane)))
        report["checks"].append(
            {
                "name": "ews_plane_matches_reference",
                "measured": max_abs,
                "target": "<= 0.10 m/s",
                "passed": max_abs <= 0.10,
            }
        )

    ti_errors = []
    psd_errors = []
    for item in report["checks"]:
        if item["name"].endswith("_hub_ti_matches_reference") and "relative_error" in item:
            ti_errors.append(100.0 * float(item["relative_error"]))
        if item["name"].endswith("_psd_shape_matches_reference"):
            comp = item["name"][0]
            psd_info = report["psd_checks"].get(comp, {})
            psd_errors.append(float(psd_info.get("shape_log_rmse", 0.0)))
    report["summary_metrics"] = {
        "max_ti_relative_error_percent": max(ti_errors) if ti_errors else 0.0,
        "max_psd_shape_log_rmse": max(psd_errors) if psd_errors else 0.0,
    }

    report["passed"] = bool(report["checks"]) and all(item["passed"] for item in report["checks"])
    return report


def markdown_table(headers: list[str], rows: list[list[str]]) -> str:
    lines = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join("---" for _ in headers) + " |",
    ]
    for row in rows:
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def write_markdown_report(summary: dict[str, Any], cfg: CampaignConfig, path: Path) -> None:
    lines: list[str] = []
    lines.append("# WindL IEC 61400-3 Validation Report")
    lines.append("")
    lines.append("## Scope")
    lines.append("")
    lines.append("This report validates the IEC wind-condition cases exposed by WindL and audits the built-in spectral models.")
    lines.append("")
    lines.append(f"- Grid: `{cfg.grid_points} x {cfg.grid_points}`")
    lines.append(f"- Time step: `{cfg.time_step} s`")
    lines.append(f"- Duration: `{cfg.duration} s`")
    lines.append(f"- Hub height: `{cfg.hub_height} m`")
    lines.append(f"- Rotor diameter / field size: `{cfg.rotor_diameter} m / {cfg.field_dim_y} x {cfg.field_dim_z} m`")
    lines.append(f"- Mean wind speed: `{cfg.mean_wind_speed} m/s`")
    lines.append(f"- ScaleIEC: `{cfg.scale_iec}`")
    lines.append("")
    lines.append("## Acceptance")
    lines.append("")
    lines.append("- Hub sigma and hub TI checks: relative error `<= 5%` for turbulent components.")
    lines.append("- PSD checks: log-shape RMSE `<= 0.35`, 90th-percentile absolute log-ratio `<= 0.75`, band-energy error `<= 20%`.")
    lines.append("- Deterministic event and steady-profile checks: direct time-history or field comparison against analytical reference.")
    lines.append("")
    lines.append("## Result")
    lines.append("")
    lines.append(f"- Overall pass: `{summary['passed']}`")
    lines.append(f"- Total cases: `{summary['case_count']}`")
    lines.append(f"- Certification-scope cases: `{summary['certification_case_count']}`")
    lines.append(f"- Wall time: `{summary['elapsed_seconds']:.2f} s`")
    lines.append("")

    rows: list[list[str]] = []
    for case in summary["cases"]:
        name = Path(case["base"]).name
        group = case["case"]["group"]
        runtime = f"{case.get('run', {}).get('elapsed_seconds', 0.0):.2f}"
        ti_err = f"{case.get('summary_metrics', {}).get('max_ti_relative_error_percent', 0.0):.2f}"
        psd_err = f"{case.get('summary_metrics', {}).get('max_psd_shape_log_rmse', 0.0):.3f}"
        rows.append([name, group, "PASS" if case.get("passed") else "FAIL", runtime, ti_err, psd_err])
    lines.append(markdown_table(["Case", "Group", "Status", "Runtime [s]", "Max TI Err [%]", "Max PSD Log-RMSE"], rows))
    lines.append("")

    lines.append("## Notes")
    lines.append("")
    lines.append("- Mann is included in the spectral-model audit, but it is not a normative IEC 61400-3 1D reference-spectrum case.")
    lines.append("- Formal certification still requires review against the licensed IEC standard, traceable requirements, and an independent certification workflow.")
    lines.append("- The analytical references used here follow the WindL implementation, the local TurbSim reference source, and the OpenFAST/TurbSim user guide.")
    lines.append("")
    lines.append("## References")
    lines.append("")
    lines.append("- OpenFAST TurbSim user documentation: <https://openfast.readthedocs.io/en/main/source/user/turbsim/>")
    lines.append("- TurbSim v2 user guide PDF: <https://openfast.readthedocs.io/en/main/_downloads/cb14d3e2d3533d76e405d730fea19846/TurbSim_v2.00.pdf>")
    lines.append("- Local TurbSim reference source: `F:\\HawtC3\\docs\\windL\\ref\\Source\\VelocitySpectra.f90`, `TSsubs.f90`")
    lines.append("- Local Bladed format reference: `F:\\HawtC3\\docs\\windL\\ref\\Bladed wind file format.md`")
    lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", default=None, help="Path to Qahse.exe")
    parser.add_argument("--grid-points", type=int, default=30)
    parser.add_argument("--dt", type=float, default=0.05)
    parser.add_argument("--duration", type=float, default=660.0)
    parser.add_argument("--output-dir", default=str(SCRIPT_DIR / "IEC61400_3" / "certification_result"))
    parser.add_argument("--case-dir", default=str(SCRIPT_DIR / "IEC61400_3" / "certification_cases"))
    parser.add_argument("--plot-dir", default=str(SCRIPT_DIR / "IEC61400_3" / "certification_result" / "plots"))
    parser.add_argument("--json", default=str(SCRIPT_DIR / "IEC61400_3" / "certification_result" / "validation_summary.json"))
    parser.add_argument("--report-md", default=str(SCRIPT_DIR / "IEC61400_3" / "certification_result" / "validation_report.md"))
    parser.add_argument("--skip-run", action="store_true", help="Validate existing outputs without running Qahse")
    args = parser.parse_args()

    cfg = CampaignConfig(grid_points=args.grid_points, time_step=args.dt, duration=args.duration)
    exe = resolve_exe(args.exe)
    output_dir = Path(args.output_dir).resolve()
    case_dir = Path(args.case_dir).resolve()
    plot_dir = Path(args.plot_dir).resolve()
    json_path = Path(args.json).resolve()
    report_md = Path(args.report_md).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    case_dir.mkdir(parents=True, exist_ok=True)
    plot_dir.mkdir(parents=True, exist_ok=True)

    if not args.skip_run and not exe.exists():
        raise FileNotFoundError(f"Qahse executable not found: {exe}")

    started = time.perf_counter()
    cases = default_cases()
    case_reports = [run_case(case, cfg, exe, output_dir, case_dir, plot_dir, args.skip_run) for case in cases]
    cert_cases = [item for item in case_reports if item["case"].get("include_in_certification", True)]
    overview = make_overview_plot(case_reports, plot_dir)

    summary = {
        "passed": all(item.get("passed", False) for item in cert_cases),
        "elapsed_seconds": time.perf_counter() - started,
        "case_count": len(case_reports),
        "certification_case_count": len(cert_cases),
        "failed_cases": [Path(item["base"]).name for item in cert_cases if not item.get("passed", False)],
        "overview_plot": overview,
        "campaign": cfg.__dict__,
        "cases": case_reports,
    }
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    write_markdown_report(summary, cfg, report_md)
    print(
        json.dumps(
            {
                "passed": summary["passed"],
                "failed_cases": summary["failed_cases"],
                "json": str(json_path),
                "report_md": str(report_md),
            },
            indent=2,
        )
    )
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
