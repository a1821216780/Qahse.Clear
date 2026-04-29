#!/usr/bin/env python3
"""Validation for SimWind phase-2 features.

This script exercises:
- USER_SPECTRA with SpecScale
- USRVKM with user shear/profile driven von Karman spectra
- stability soft-coupling through Rich_No/UStar/ZI and diabatic LOG profile
- explicit Reynolds-stress targets on the Mann model
- API coherence rejection on v/w components
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

import numpy as np

try:
    import matplotlib.pyplot as plt
except Exception:  # pragma: no cover
    plt = None

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from validate_iec61400_3_cases import psd_metrics, von_karman_psd  # noqa: E402
from validate_simwind_output import covariance_matrix, hub_series, parse_sum, read_bts, validate  # noqa: E402


COMPONENTS = ["u", "v", "w"]
LEGACY_WARNING_TOKENS = (
    "without inferred stability",
    "is ignored",
    "is ignored.",
    "skips this step",
    "not applied",
    "fallback",
)


@dataclass
class Phase2Case:
    name: str
    qwd_name: str
    check_sum_sigma: bool
    validator: Callable[[dict[str, Any], dict[str, Any], Path], dict[str, Any]]


def resolve_exe(path: str | None) -> Path:
    if path:
        return Path(path).resolve()
    for candidate in (REPO_ROOT / "build" / "release" / "Qahse.exe", REPO_ROOT / "build" / "debug" / "Qahse.exe"):
        if candidate.exists():
            return candidate
    return REPO_ROOT / "build" / "release" / "Qahse.exe"


def parse_scalar(text: str) -> Any:
    token = text.strip().strip('"')
    lower = token.lower()
    if lower == "true":
        return True
    if lower == "false":
        return False
    if lower == "default":
        return "default"
    try:
        if any(ch in token for ch in (".", "e", "E")):
            return float(token)
        return int(token)
    except ValueError:
        return token


def parse_qwd(path: Path) -> dict[str, Any]:
    values: dict[str, Any] = {}
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = raw.strip()
        if not line or line.startswith("--") or line.startswith("#"):
            continue
        if " - " in line:
            line = line.split(" - ", 1)[0].strip()
        parts = line.split(maxsplit=1)
        if len(parts) != 2:
            continue
        values[parts[1].strip()] = parse_scalar(parts[0])
    return values


def resolve_repo_path(value: str | Path) -> Path:
    path = Path(value)
    return path if path.is_absolute() else (REPO_ROOT / path).resolve()


def resolve_relative_to_qwd(qwd_path: Path, value: str | Path) -> Path:
    path = Path(value)
    return path if path.is_absolute() else (qwd_path.parent / path).resolve()


def output_base_from_qwd(qwd_path: Path, meta: dict[str, Any]) -> Path:
    return resolve_relative_to_qwd(qwd_path, meta["WrWndPath"]) / str(meta["WrWndName"])


def read_rows_after_begin(path: Path, columns: int) -> list[list[float]]:
    rows: list[list[float]] = []
    active = False
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = raw.strip()
        if not line or line.startswith("--") or line.startswith("#"):
            continue
        if line.startswith("!Begin"):
            active = True
            continue
        if not active:
            continue
        parts = line.split()
        if len(parts) >= columns:
            rows.append([float(value) for value in parts[:columns]])
    return rows


def read_keyword_value(path: Path, key: str, default: float) -> float:
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = raw.strip()
        if not line or line.startswith("--") or line.startswith("#"):
            continue
        if " - " in line:
            line = line.split(" - ", 1)[0].strip()
        parts = line.split()
        if len(parts) >= 2 and parts[1] == key:
            return float(parts[0])
    return default


def read_user_spectra(path: Path) -> dict[str, Any]:
    rows = read_rows_after_begin(path, 4)
    data = np.asarray(rows, dtype=float)
    return {
        "frequency": data[:, 0],
        "u": data[:, 1],
        "v": data[:, 2],
        "w": data[:, 3],
        "scales": np.asarray(
            [
                read_keyword_value(path, "SpecScale1", 1.0),
                read_keyword_value(path, "SpecScale2", 1.0),
                read_keyword_value(path, "SpecScale3", 1.0),
            ],
            dtype=float,
        ),
    }


def read_user_shear(path: Path) -> dict[str, Any]:
    rows = read_rows_after_begin(path, 5)
    data = np.asarray(rows, dtype=float)
    return {
        "height": data[:, 0],
        "speed": data[:, 1],
        "direction": data[:, 2],
        "sigma": data[:, 3],
        "length": data[:, 4],
        "std_scales": np.asarray(
            [
                read_keyword_value(path, "StdScale1", 1.0),
                read_keyword_value(path, "StdScale2", 1.0),
                read_keyword_value(path, "StdScale3", 1.0),
            ],
            dtype=float,
        ),
    }


def normalize_direction_deg(value: np.ndarray | float) -> np.ndarray | float:
    return np.mod(value, 360.0)


def angular_error_deg(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    delta = np.mod(a - b + 180.0, 360.0) - 180.0
    return np.abs(delta)


def interpolate_profile(x: np.ndarray, y: np.ndarray, xi: np.ndarray) -> np.ndarray:
    return np.interp(np.asarray(xi, dtype=float), np.asarray(x, dtype=float), np.asarray(y, dtype=float))


def interpolate_wrapped_direction(x: np.ndarray, direction: np.ndarray, xi: np.ndarray) -> np.ndarray:
    x = np.asarray(x, dtype=float)
    direction = np.asarray(direction, dtype=float)
    xi = np.asarray(xi, dtype=float)
    radians = np.deg2rad(direction)
    cos_values = np.interp(xi, x, np.cos(radians))
    sin_values = np.interp(xi, x, np.sin(radians))
    return normalize_direction_deg(np.rad2deg(np.arctan2(sin_values, cos_values)))


def endpoint_hold(freq: np.ndarray, values: np.ndarray, query: np.ndarray) -> np.ndarray:
    freq = np.asarray(freq, dtype=float)
    values = np.asarray(values, dtype=float)
    query = np.asarray(query, dtype=float)
    result = np.interp(query, freq, values)
    result = np.where(query <= freq[0], values[0], result)
    result = np.where(query >= freq[-1], values[-1], result)
    return result


def grid_coordinates(bts: dict[str, Any]) -> tuple[np.ndarray, np.ndarray]:
    header = bts["header"]
    z = float(header["zbottom"]) + float(header["dz"]) * np.arange(int(header["nz"]), dtype=float)
    y = -0.5 * float(header["dy"]) * (int(header["ny"]) - 1) + float(header["dy"]) * np.arange(int(header["ny"]), dtype=float)
    return y, z


def hub_index(bts: dict[str, Any]) -> tuple[int, int]:
    y, z = grid_coordinates(bts)
    iy = int(np.argmin(np.abs(y)))
    iz = int(np.argmin(np.abs(z - float(bts["header"]["hub_height"]))))
    return iy, iz


def stability_psi_m(z_over_l: np.ndarray | float) -> np.ndarray | float:
    value = np.asarray(z_over_l, dtype=float)
    stable = -5.0 * np.minimum(value, 1.0)
    tmp = np.power(1.0 - 15.0 * value, 0.25)
    psi = -np.log(0.125 * np.power(1.0 + tmp, 2.0) * (1.0 + tmp * tmp)) + 2.0 * np.arctan(tmp) - 0.5 * math.pi
    unstable = -psi
    return np.where(value >= 0.0, stable, unstable)


def derive_zl_from_richardson(richardson: float) -> float:
    if richardson <= 0.0:
        return richardson
    if richardson < 0.16667:
        return min(richardson / (1.0 - 5.0 * richardson), 1.0)
    return 1.0


def monin_length_from_zl(hub_height: float, z_over_l: float) -> float:
    if abs(z_over_l) <= 1.0e-12:
        return math.inf
    return hub_height / z_over_l


def z_over_l_at_height(height: np.ndarray, monin_length: float) -> np.ndarray:
    if not math.isfinite(monin_length) or abs(monin_length) <= 1.0e-12:
        return np.zeros_like(height, dtype=float)
    return height / monin_length


def diabatic_log_speed(height: np.ndarray, ref_height: float, ref_speed: float, roughness: float, monin_length: float) -> np.ndarray:
    z0 = min(max(roughness, 1.0e-5), ref_height * 0.95)
    psi_ht = stability_psi_m(z_over_l_at_height(height, monin_length))
    psi_ref = float(stability_psi_m(z_over_l_at_height(np.asarray([ref_height]), monin_length))[0])
    denom = math.log(ref_height / z0) - psi_ref
    return ref_speed * (np.log(height / z0) - psi_ht) / max(denom, 1.0e-12)


def legacy_warning_check(report: dict[str, Any]) -> dict[str, Any]:
    warnings = report.get("sum", {}).get("warnings", [])
    bad = [warning for warning in warnings if any(token in warning.lower() for token in LEGACY_WARNING_TOKENS)]
    return {
        "name": "no_legacy_unimplemented_warning",
        "passed": not bad,
        "warnings": warnings,
        "violations": bad,
    }


def make_psd_overlay_plot(case_name: str, metrics: dict[str, Any], output_dir: Path) -> str | None:
    if plt is None:
        return None
    fig, axes = plt.subplots(3, 1, figsize=(8, 10), sharex=True)
    for comp, ax in enumerate(axes):
        item = metrics[COMPONENTS[comp]]
        freq = np.asarray(item["freq"], dtype=float)
        measured = np.asarray(item["measured"], dtype=float)
        target = np.asarray(item["target"], dtype=float)
        mask_m = (freq > 0.0) & np.isfinite(measured) & (measured > 0.0)
        mask_t = (freq > 0.0) & np.isfinite(target) & (target > 0.0)
        ax.loglog(freq[mask_m], measured[mask_m], label="measured", linewidth=1.0)
        ax.loglog(freq[mask_t], target[mask_t], label="target", linewidth=1.0, linestyle="--")
        ax.set_ylabel(f"{COMPONENTS[comp]} PSD")
        ax.grid(True, which="both", alpha=0.25)
        ax.legend()
    axes[-1].set_xlabel("frequency [Hz]")
    fig.suptitle(case_name)
    path = output_dir / f"{case_name}_target_psd.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


def make_profile_plot(case_name: str,
                      z: np.ndarray,
                      measured: np.ndarray,
                      target: np.ndarray,
                      xlabel: str,
                      output_dir: Path,
                      stem: str) -> str | None:
    if plt is None:
        return None
    fig, ax = plt.subplots(figsize=(6, 6))
    for comp, name in enumerate(COMPONENTS):
        ax.plot(measured[:, comp], z, marker="o", linewidth=1.0, label=f"{name} measured")
        ax.plot(target[:, comp], z, linestyle="--", linewidth=1.0, label=f"{name} target")
    ax.set_xlabel(xlabel)
    ax.set_ylabel("z [m]")
    ax.grid(True, alpha=0.25)
    ax.legend()
    path = output_dir / f"{case_name}_{stem}.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


def make_direction_plot(case_name: str, z: np.ndarray, measured: np.ndarray, target: np.ndarray, output_dir: Path) -> str | None:
    if plt is None:
        return None
    fig, ax = plt.subplots(figsize=(6, 6))
    ax.plot(measured, z, marker="o", linewidth=1.0, label="measured")
    ax.plot(target, z, linestyle="--", linewidth=1.0, label="target")
    ax.set_xlabel("direction [deg]")
    ax.set_ylabel("z [m]")
    ax.grid(True, alpha=0.25)
    ax.legend()
    path = output_dir / f"{case_name}_direction_profile.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


def make_covariance_compare_plot(case_name: str, measured: np.ndarray, target: np.ndarray, output_dir: Path) -> str | None:
    if plt is None:
        return None
    fig, axes = plt.subplots(1, 2, figsize=(9, 4))
    for ax, matrix, title in zip(axes, (measured, target), ("measured", "target")):
        image = ax.imshow(matrix, cmap="coolwarm")
        ax.set_xticks(range(3), COMPONENTS)
        ax.set_yticks(range(3), COMPONENTS)
        ax.set_title(title)
        for i in range(3):
            for j in range(3):
                ax.text(j, i, f"{matrix[i, j]:.3f}", ha="center", va="center", fontsize=8)
        fig.colorbar(image, ax=ax, fraction=0.046, pad=0.04)
    path = output_dir / f"{case_name}_covariance_compare.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


def run_qwd(exe: Path, qwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(exe), "--qwd", str(qwd)],
        cwd=str(REPO_ROOT),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="ignore",
        check=False,
    )


def validate_user_spectra(case_report: dict[str, Any], meta: dict[str, Any], plot_dir: Path) -> dict[str, Any]:
    qwd_path = Path(case_report["case"]["qwd"])
    spectra = read_user_spectra(resolve_relative_to_qwd(qwd_path, str(meta["UserTurbFile"])))
    bts = read_bts(Path(case_report["base"]).with_suffix(".bts"))
    hub = hub_series(bts["data"])
    dt = float(bts["header"]["dt"])

    checks = [legacy_warning_check(case_report)]
    metrics: dict[str, Any] = {}
    for comp, name in enumerate(COMPONENTS):
        target_func = lambda freq, comp=comp, name=name: spectra["scales"][comp] * endpoint_hold(spectra["frequency"], spectra[name], freq)
        item = psd_metrics(hub[:, comp], dt, target_func)
        item["name"] = f"user_spectra_{name}_psd_shape"
        item["target_scale"] = float(spectra["scales"][comp])
        item_passed = float(item.get("shape_log_rmse", math.inf)) <= 0.50 and float(item.get("band_energy_relative_error", math.inf)) <= 0.30
        checks.append(
            {
                "name": item["name"],
                "passed": item_passed,
                "shape_log_rmse": item.get("shape_log_rmse"),
                "band_energy_relative_error": item.get("band_energy_relative_error"),
            }
        )
        metrics[name] = item

    plots: list[str] = []
    plot = make_psd_overlay_plot(case_report["case"]["name"], metrics, plot_dir)
    if plot:
        plots.append(plot)

    return {"checks": checks, "metrics": metrics, "plots": plots}


def validate_usrvkm(case_report: dict[str, Any], meta: dict[str, Any], plot_dir: Path) -> dict[str, Any]:
    qwd_path = Path(case_report["case"]["qwd"])
    profile = read_user_shear(resolve_relative_to_qwd(qwd_path, str(meta["UserShearFile"])))
    bts = read_bts(Path(case_report["base"]).with_suffix(".bts"))
    _, z = grid_coordinates(bts)
    iy, iz_hub = hub_index(bts)
    data = np.asarray(bts["data"][:, :, iy, :], dtype=float)

    measured_mean = np.mean(data, axis=0)
    measured_sigma = np.std(data, axis=0)
    target_u = interpolate_profile(profile["height"], profile["speed"], z)
    target_dir = interpolate_wrapped_direction(profile["height"], profile["direction"], z)
    sigma_base = interpolate_profile(profile["height"], profile["sigma"], z)
    length_base = interpolate_profile(profile["height"], profile["length"], z)

    lu = float(meta.get("VxLu", 1.0))
    lv = float(meta.get("VxLv", lu))
    lw = float(meta.get("VxLw", lu))
    length_ratios = np.asarray([1.0, lv / max(lu, 1.0e-12), lw / max(lu, 1.0e-12)], dtype=float)

    target_sigma = sigma_base[:, None] * profile["std_scales"][None, :]
    target_ti = target_sigma / np.maximum(target_u[:, None], 0.1)
    measured_ti = measured_sigma / np.maximum(measured_mean[:, [0]], 0.1)

    checks = [legacy_warning_check(case_report)]
    ti_rel = np.abs(measured_ti - target_ti) / np.maximum(target_ti, 1.0e-12)
    sigma_rel = np.abs(measured_sigma - target_sigma) / np.maximum(target_sigma, 1.0e-12)
    for comp, name in enumerate(COMPONENTS):
        checks.append(
            {
                "name": f"usrvkm_{name}_sigma_profile",
                "passed": float(np.max(sigma_rel[:, comp])) <= 0.30,
                "max_relative_error": float(np.max(sigma_rel[:, comp])),
                "rms_relative_error": float(np.sqrt(np.mean(np.square(sigma_rel[:, comp])))),
            }
        )
        checks.append(
            {
                "name": f"usrvkm_{name}_ti_profile",
                "passed": float(np.max(ti_rel[:, comp])) <= 0.30,
                "max_relative_error": float(np.max(ti_rel[:, comp])),
                "rms_relative_error": float(np.sqrt(np.mean(np.square(ti_rel[:, comp])))),
            }
        )

    measured_direction = normalize_direction_deg(np.degrees(np.arctan2(measured_mean[:, 1], measured_mean[:, 0])))
    dir_error = angular_error_deg(measured_direction, target_dir)
    checks.append(
        {
            "name": "usrvkm_direction_profile",
            "passed": float(np.max(dir_error)) <= 3.0,
            "max_abs_error_deg": float(np.max(dir_error)),
            "rms_error_deg": float(np.sqrt(np.mean(np.square(dir_error)))),
        }
    )

    dt = float(bts["header"]["dt"])
    hub_series_uvw = data[:, iz_hub, :]
    hub_u = float(target_u[iz_hub])
    hub_sigma = target_sigma[iz_hub]
    hub_length = float(length_base[iz_hub])
    hub_metrics: dict[str, Any] = {}
    for comp, name in enumerate(COMPONENTS):
        local_length = hub_length * length_ratios[comp]
        target_func = lambda freq, comp=comp, local_length=local_length: von_karman_psd(
            freq,
            float(hub_sigma[comp]),
            float(local_length),
            hub_u,
            comp,
        )
        item = psd_metrics(hub_series_uvw[:, comp], dt, target_func)
        item_passed = float(item.get("shape_log_rmse", math.inf)) <= 0.40
        checks.append(
            {
                "name": f"usrvkm_{name}_hub_psd",
                "passed": item_passed,
                "shape_log_rmse": item.get("shape_log_rmse"),
            }
        )
        hub_metrics[name] = item

    plots: list[str] = []
    ti_plot = make_profile_plot(case_report["case"]["name"], z, measured_ti, target_ti, "TI [-]", plot_dir, "ti_profile")
    if ti_plot:
        plots.append(ti_plot)
    sigma_plot = make_profile_plot(case_report["case"]["name"], z, measured_sigma, target_sigma, "sigma [m/s]", plot_dir, "sigma_profile")
    if sigma_plot:
        plots.append(sigma_plot)
    dir_plot = make_direction_plot(case_report["case"]["name"], z, measured_direction, target_dir, plot_dir)
    if dir_plot:
        plots.append(dir_plot)
    psd_plot = make_psd_overlay_plot(case_report["case"]["name"] + "_hub", hub_metrics, plot_dir)
    if psd_plot:
        plots.append(psd_plot)

    return {
        "checks": checks,
        "metrics": {"hub_psd": hub_metrics},
        "plots": plots,
    }


def validate_stability(case_report: dict[str, Any], meta: dict[str, Any], plot_dir: Path) -> dict[str, Any]:
    bts = read_bts(Path(case_report["base"]).with_suffix(".bts"))
    _, z = grid_coordinates(bts)
    data = np.asarray(bts["data"], dtype=float)
    mean_profile = np.mean(data[..., 0], axis=(0, 2))

    ref_height = float(meta["RefHt"])
    mean_wind = float(meta["MeanWindSpeed"])
    roughness = float(meta["Roughness"])
    hub_height = float(meta["HubHt"])
    z_over_l = float(meta.get("ZL", 0.0))
    if abs(z_over_l) <= 1.0e-12:
        z_over_l = derive_zl_from_richardson(float(meta["Rich_No"]))
    monin_length = monin_length_from_zl(hub_height, z_over_l)
    target_profile = diabatic_log_speed(np.maximum(z, 0.05), ref_height, mean_wind, roughness, monin_length)

    checks = [legacy_warning_check(case_report)]
    rel = np.abs(mean_profile - target_profile) / np.maximum(np.abs(target_profile), 1.0e-12)
    checks.append(
        {
            "name": "stability_mean_profile",
            "passed": float(np.max(rel)) <= 0.08,
            "max_relative_error": float(np.max(rel)),
            "rms_relative_error": float(np.sqrt(np.mean(np.square(rel)))),
        }
    )

    derived = case_report.get("sum", {}).get("derived", {})
    checks.append(
        {
            "name": "stability_summary_zl",
            "passed": abs(float(derived.get("ZL", 999.0)) - z_over_l) <= 1.0e-6,
            "measured": derived.get("ZL"),
            "target": z_over_l,
        }
    )
    checks.append(
        {
            "name": "stability_summary_zi",
            "passed": abs(float(derived.get("ZI", -1.0)) - float(meta["ZI"])) <= 1.0e-6,
            "measured": derived.get("ZI"),
            "target": float(meta["ZI"]),
        }
    )
    checks.append(
        {
            "name": "stability_reynolds_default_enabled",
            "passed": bool(derived.get("ReynoldsStressActive", False)),
        }
    )

    cov = np.asarray(case_report["hub_covariance"], dtype=float)
    target_uw = -float(meta["UStar"]) ** 2
    checks.append(
        {
            "name": "stability_default_uw_target",
            "passed": abs(cov[0, 2] - target_uw) / max(abs(target_uw), 1.0e-12) <= 0.20,
            "measured": float(cov[0, 2]),
            "target": target_uw,
            "relative_error": float(abs(cov[0, 2] - target_uw) / max(abs(target_uw), 1.0e-12)),
        }
    )

    plots: list[str] = []
    if plt is not None:
        fig, ax = plt.subplots(figsize=(6, 6))
        ax.plot(mean_profile, z, marker="o", linewidth=1.0, label="measured")
        ax.plot(target_profile, z, linestyle="--", linewidth=1.0, label="target")
        ax.set_xlabel("u [m/s]")
        ax.set_ylabel("z [m]")
        ax.grid(True, alpha=0.25)
        ax.legend()
        path = plot_dir / f"{case_report['case']['name']}_mean_profile.png"
        fig.tight_layout()
        fig.savefig(path, dpi=160)
        plt.close(fig)
        plots.append(str(path))

    target_cov = np.zeros((3, 3), dtype=float)
    target_cov[0, 2] = target_cov[2, 0] = target_uw
    cov_plot = make_covariance_compare_plot(case_report["case"]["name"], cov, target_cov, plot_dir)
    if cov_plot:
        plots.append(cov_plot)

    return {"checks": checks, "metrics": {}, "plots": plots}


def validate_mann_reynolds(case_report: dict[str, Any], meta: dict[str, Any], plot_dir: Path) -> dict[str, Any]:
    cov = np.asarray(case_report["hub_covariance"], dtype=float)
    target_cov = np.zeros((3, 3), dtype=float)
    target_cov[0, 1] = target_cov[1, 0] = float(meta["PC_UV"])
    target_cov[0, 2] = target_cov[2, 0] = float(meta["PC_UW"])
    target_cov[1, 2] = target_cov[2, 1] = float(meta["PC_VW"])

    checks = [legacy_warning_check(case_report)]
    for (i, j, key) in ((0, 2, "PC_UW"), (0, 1, "PC_UV"), (1, 2, "PC_VW")):
        target = float(meta[key])
        if abs(target) <= 1.0e-12:
            continue
        measured = float(cov[i, j])
        checks.append(
            {
                "name": f"mann_{key.lower()}",
                "passed": abs(measured - target) / max(abs(target), 1.0e-12) <= 0.25,
                "measured": measured,
                "target": target,
                "relative_error": float(abs(measured - target) / max(abs(target), 1.0e-12)),
            }
        )

    warnings = case_report.get("sum", {}).get("warnings", [])
    clipped = [warning for warning in warnings if "clipped" in warning.lower()]
    checks.append(
        {
            "name": "mann_reynolds_not_clipped",
            "passed": not clipped,
            "violations": clipped,
        }
    )

    plots: list[str] = []
    cov_plot = make_covariance_compare_plot(case_report["case"]["name"], cov, target_cov, plot_dir)
    if cov_plot:
        plots.append(cov_plot)

    return {"checks": checks, "metrics": {}, "plots": plots}


def validate_api_rejection(exe: Path, template_qwd: Path, output_dir: Path) -> dict[str, Any]:
    text = template_qwd.read_text(encoding="utf-8", errors="ignore")
    if "GENERAL CohMod2" in text:
        text = text.replace("GENERAL CohMod2", "API CohMod2", 1)
    else:
        text += "\nAPI CohMod2\n"
    bad_qwd = output_dir / "Qahse_WindL_API_Invalid_DEMO.qwd"
    bad_qwd.write_text(text, encoding="utf-8")
    proc = run_qwd(exe, bad_qwd)
    combined = (proc.stdout or "") + "\n" + (proc.stderr or "")
    message = "API coherence model is valid only for the u component"
    return {
        "name": "api_invalid_v_component_rejected",
        "passed": proc.returncode != 0 and message in combined,
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
    }


def run_case(case: Phase2Case, exe: Path, plot_root: Path, skip_run: bool) -> dict[str, Any]:
    qwd = SCRIPT_DIR / case.qwd_name
    meta = parse_qwd(qwd)
    base = output_base_from_qwd(qwd, meta)
    plot_dir = plot_root / case.name
    plot_dir.mkdir(parents=True, exist_ok=True)

    if not skip_run:
        proc = run_qwd(exe, qwd)
        if proc.returncode != 0:
            raise RuntimeError(f"{case.name} failed to run.\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}")

    report = validate(base, tolerance=0.50, plot_dir=plot_dir, check_sum_sigma=case.check_sum_sigma)
    report["case"] = {"name": case.name, "qwd": str(qwd)}
    extra = case.validator(report, meta, plot_dir)
    report["extra_checks"] = extra["checks"]
    report["extra_metrics"] = extra["metrics"]
    report["plots"] = list(report.get("plots", [])) + list(extra["plots"])
    report["passed"] = report["passed"] and all(check["passed"] for check in extra["checks"])
    return report


def write_markdown_report(path: Path, reports: list[dict[str, Any]], api_check: dict[str, Any]) -> None:
    total_checks = sum(len(report.get("checks", [])) + len(report.get("extra_checks", [])) for report in reports) + 1
    passed_checks = sum(
        sum(1 for check in report.get("checks", []) if check["passed"]) +
        sum(1 for check in report.get("extra_checks", []) if check["passed"])
        for report in reports
    ) + (1 if api_check["passed"] else 0)

    lines = [
        "# SimWind Phase-2 Validation Report",
        "",
        f"- Cases: {len(reports)} + 1 API rejection check",
        f"- Passed checks: {passed_checks}/{total_checks}",
        "",
        "## Case Results",
        "",
    ]
    for report in reports:
        lines.append(f"### {report['case']['name']}")
        lines.append(f"- Passed: {report['passed']}")
        lines.append(f"- Base: `{report['base']}`")
        for check in report.get("checks", []):
            lines.append(f"- Base check `{check['name']}`: {'PASS' if check['passed'] else 'FAIL'}")
        for check in report.get("extra_checks", []):
            lines.append(f"- Extra check `{check['name']}`: {'PASS' if check['passed'] else 'FAIL'}")
        if report.get("plots"):
            lines.append("- Plots:")
            for plot in report["plots"]:
                lines.append(f"  - `{plot}`")
        lines.append("")

    lines.extend(
        [
            "## API Rejection",
            "",
            f"- `{api_check['name']}`: {'PASS' if api_check['passed'] else 'FAIL'}",
            f"- Return code: {api_check['returncode']}",
            "",
        ]
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def json_default(value: Any) -> Any:
    if isinstance(value, (np.floating,)):
        return float(value)
    if isinstance(value, (np.integer,)):
        return int(value)
    if isinstance(value, (np.bool_,)):
        return bool(value)
    if isinstance(value, Path):
        return str(value)
    raise TypeError(f"Object of type {value.__class__.__name__} is not JSON serializable")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", default=None, help="Path to Qahse.exe")
    parser.add_argument("--skip-run", action="store_true", help="Reuse existing generated outputs")
    parser.add_argument("--artifact-dir", default=str(SCRIPT_DIR / "phase2_validation"))
    args = parser.parse_args()

    exe = resolve_exe(args.exe)
    artifact_dir = Path(args.artifact_dir).resolve()
    plot_root = artifact_dir / "plots"
    artifact_dir.mkdir(parents=True, exist_ok=True)
    plot_root.mkdir(parents=True, exist_ok=True)

    cases = [
        Phase2Case("UserSpectraScaled", "Qahse_WindL_User_Spectra_Scaled_DEMO.qwd", False, validate_user_spectra),
        Phase2Case("USRVKM", "Qahse_WindL_USRVKM_DEMO.qwd", True, validate_usrvkm),
        Phase2Case("IecKaimalStability", "Qahse_WindL_IEC_Kaimal_Stability_DEMO.qwd", True, validate_stability),
        Phase2Case("MannReynoldsStress", "Qahse_WindL_Mann_ReynoldsStress_DEMO.qwd", False, validate_mann_reynolds),
    ]

    reports = [run_case(case, exe, plot_root, args.skip_run) for case in cases]
    api_check = validate_api_rejection(exe, SCRIPT_DIR / "Qahse_WindL_IEC_Kaimal_Stability_DEMO.qwd", artifact_dir)

    summary = {
        "exe": str(exe),
        "reports": reports,
        "api_rejection": api_check,
        "passed": all(report["passed"] for report in reports) and api_check["passed"],
    }
    summary_path = artifact_dir / "phase2_validation_summary.json"
    report_path = artifact_dir / "phase2_validation_report.md"
    summary_path.write_text(json.dumps(summary, indent=2, default=json_default) + "\n", encoding="utf-8")
    write_markdown_report(report_path, reports, api_check)

    print(json.dumps({"passed": summary["passed"], "summary": str(summary_path), "report": str(report_path)}, indent=2))
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
