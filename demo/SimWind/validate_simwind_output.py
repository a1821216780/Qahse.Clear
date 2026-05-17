#!/usr/bin/env python3
"""Read and validate SimWind output files.

The script validates the generated wind statistically rather than requiring
pointwise agreement with TurbSim. It reads the BTS physical velocities, checks
the component standard deviations against the SimWind/IEC targets written in
the .sum file, and reads Bladed WND files to verify normalized mean/std data.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import struct
from pathlib import Path
from typing import BinaryIO

import numpy as np

try:
    import matplotlib.pyplot as plt
except Exception:  # pragma: no cover - plotting is optional in headless envs
    plt = None


COMPONENT_NAMES = ("u", "v", "w")


def read_scalar(fh: BinaryIO, fmt: str):
    data = fh.read(struct.calcsize(fmt))
    if len(data) != struct.calcsize(fmt):
        raise EOFError("Unexpected end of file while reading binary wind file")
    return struct.unpack("<" + fmt, data)[0]


def parse_sum(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="ignore")
    result: dict = {"stats": {}, "target_sigma": None, "warnings": [], "derived": {}, "input": {}}

    sigma_match = re.search(
        r"SigmaU/SigmaV/SigmaW:\s*([-+0-9.eE]+),\s*([-+0-9.eE]+),\s*([-+0-9.eE]+)",
        text,
    )
    if sigma_match:
        result["target_sigma"] = [float(sigma_match.group(i)) for i in range(1, 4)]

    for comp in ("u", "v", "w"):
        match = re.search(
            rf"\b{comp}:\s*mean=([-+0-9.eE]+)\s+sigma=([-+0-9.eE]+)\s+TI=([-+0-9.eE]+)%",
            text,
        )
        if match:
            result["stats"][comp] = {
                "mean": float(match.group(1)),
                "sigma": float(match.group(2)),
                "ti_percent": float(match.group(3)),
            }

    for key in ("TurbModel", "WindModel", "RandSeed"):
        match = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if match:
            result["input"][key] = int(float(match.group(1)))

    for key in ("MeanWindSpeed", "HubHt"):
        match = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if match:
            result["input"][key] = float(match.group(1))

    for warning in re.findall(r"^\s*-\s*(.+)$", text, flags=re.MULTILINE):
        result["warnings"].append(warning)

    for key in ("Lambda", "LC", "ZL", "L", "UStar", "UStarDiab", "ZI", "ScaleIEC", "MannGamma", "MannLength", "MannMaxL"):
        match = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if match:
            result["derived"][key] = float(match.group(1))

    scale_match = re.search(
        r"IntegralScaleU/V/W:\s*([-+0-9.eE]+),\s*([-+0-9.eE]+),\s*([-+0-9.eE]+)",
        text,
    )
    if scale_match:
        result["derived"]["IntegralScaleUVW"] = [float(scale_match.group(i)) for i in range(1, 4)]

    for key in ("AllowCohApprox", "UserDirectionProfile", "UserSigmaProfile", "UserLengthScaleProfile", "ReynoldsStressActive"):
        match = re.search(rf"\b{key}:\s*(true|false)", text, flags=re.IGNORECASE)
        if match:
            result["derived"][key] = match.group(1).lower() == "true"

    return result


def read_bts(path: Path) -> dict:
    with path.open("rb") as fh:
        file_id = read_scalar(fh, "h")
        nz = read_scalar(fh, "i")
        ny = read_scalar(fh, "i")
        ntwr = read_scalar(fh, "i")
        nt = read_scalar(fh, "i")
        dz = read_scalar(fh, "f")
        dy = read_scalar(fh, "f")
        dt = read_scalar(fh, "f")
        uhub = read_scalar(fh, "f")
        hub_height = read_scalar(fh, "f")
        zbottom = read_scalar(fh, "f")
        u_scale = read_scalar(fh, "f")
        u_offset = read_scalar(fh, "f")
        v_scale = read_scalar(fh, "f")
        v_offset = read_scalar(fh, "f")
        w_scale = read_scalar(fh, "f")
        w_offset = read_scalar(fh, "f")
        scales = [u_scale, v_scale, w_scale]
        offsets = [u_offset, v_offset, w_offset]
        desc_len = read_scalar(fh, "i")
        description = fh.read(desc_len).decode("utf-8", errors="ignore")
        raw = np.fromfile(fh, dtype="<i2", count=nt * nz * ny * 3)

    if raw.size != nt * nz * ny * 3:
        raise ValueError(f"{path} does not contain the expected BTS data size")

    raw = raw.reshape((nt, nz, ny, 3)).astype(np.float64)
    data = np.empty_like(raw)
    for comp in range(3):
        data[..., comp] = (raw[..., comp] - offsets[comp]) / scales[comp]

    return {
        "header": {
            "file_id": int(file_id),
            "nz": int(nz),
            "ny": int(ny),
            "ntwr": int(ntwr),
            "nt": int(nt),
            "dz": float(dz),
            "dy": float(dy),
            "dt": float(dt),
            "uhub": float(uhub),
            "hub_height": float(hub_height),
            "zbottom": float(zbottom),
            "description": description,
        },
        "data": data,
    }


def read_bladed_wnd(path: Path) -> dict:
    with path.open("rb") as fh:
        record1 = read_scalar(fh, "h")
        record2 = read_scalar(fh, "h")
        header_bytes = None
        ncomp = 3

        if record2 >= 7:
            header_bytes = read_scalar(fh, "i")
            ncomp = read_scalar(fh, "i")
        elif record2 == 4:
            ncomp = read_scalar(fh, "i")
            for _ in range(6):
                read_scalar(fh, "f")

        dz = read_scalar(fh, "f")
        dy = read_scalar(fh, "f")
        dx = read_scalar(fh, "f")
        half_nt = read_scalar(fh, "i")
        mean_wind = read_scalar(fh, "f")
        length_scales = [read_scalar(fh, "f") for _ in range(3)]
        max_freq_or_unused = read_scalar(fh, "f")
        seed = read_scalar(fh, "i")
        nz = read_scalar(fh, "i")
        ny = read_scalar(fh, "i")

        if ncomp == 3:
            length_scales.extend(read_scalar(fh, "f") for _ in range(6))
        if record2 >= 7 and header_bytes is not None:
            fh.seek(8 + header_bytes)

        nt = half_nt * 2
        raw = np.fromfile(fh, dtype="<i2", count=nt * nz * ny * ncomp)

    if raw.size != nt * nz * ny * ncomp:
        raise ValueError(f"{path} does not contain the expected Bladed WND data size")

    normalized = raw.reshape((nt, nz, ny, ncomp)).astype(np.float64) / 1000.0
    normalized = normalized[:, :, ::-1, :]
    return {
        "header": {
            "record1": int(record1),
            "record2": int(record2),
            "header_bytes": int(header_bytes) if header_bytes is not None else None,
            "ncomp": int(ncomp),
            "nz": int(nz),
            "ny": int(ny),
            "nt": int(nt),
            "dz": float(dz),
            "dy": float(dy),
            "dx": float(dx),
            "mean_wind": float(mean_wind),
            "length_scales": [float(x) for x in length_scales],
            "max_freq_or_unused": float(max_freq_or_unused),
            "seed": int(seed),
        },
        "normalized": normalized,
    }


def component_stats(data: np.ndarray, uhub: float) -> list[dict]:
    stats = []
    for comp in range(data.shape[-1]):
        values = data[..., comp].ravel()
        mean = float(values.mean())
        sigma = float(values.std())
        stats.append(
            {
                "mean": mean,
                "sigma": sigma,
                "ti_percent": float(100.0 * sigma / uhub) if uhub > 0.0 else math.nan,
            }
        )
    return stats


def hub_indices(header: dict) -> tuple[int, int]:
    nz = int(header["nz"])
    ny = int(header["ny"])
    dz = float(header["dz"])
    dy = float(header["dy"])
    z_bottom = float(header["zbottom"])
    hub_height = float(header["hub_height"])

    best_iz = 0
    best_z = float("inf")
    for iz in range(nz):
        z = z_bottom + dz * iz
        delta = abs(z - hub_height)
        if delta < best_z:
            best_z = delta
            best_iz = iz

    best_iy = 0
    best_y = float("inf")
    for iy in range(ny):
        y = -0.5 * dy * (ny - 1) + dy * iy
        delta = abs(y)
        if delta < best_y:
            best_y = delta
            best_iy = iy

    return best_iz, best_iy


def hub_series(data: np.ndarray, header: dict | None = None) -> np.ndarray:
    if header is None:
        mid_z, mid_y = data.shape[1] // 2, data.shape[2] // 2
    else:
        mid_z, mid_y = hub_indices(header)
    return np.asarray(data[:, mid_z, mid_y, :], dtype=np.float64)


def covariance_matrix(series: np.ndarray) -> np.ndarray:
    centered = np.asarray(series, dtype=np.float64) - np.mean(series, axis=0, keepdims=True)
    if centered.shape[0] <= 1:
        return np.zeros((centered.shape[1], centered.shape[1]), dtype=np.float64)
    return (centered.T @ centered) / float(centered.shape[0])


def power_spectral_density(series: np.ndarray, dt: float) -> tuple[np.ndarray, np.ndarray]:
    values = np.asarray(series, dtype=np.float64)
    centered = values - float(np.mean(values))
    if centered.size < 2 or dt <= 0.0:
        return np.zeros(0, dtype=np.float64), np.zeros(0, dtype=np.float64)

    freq = np.fft.rfftfreq(centered.size, d=dt)
    fft_values = np.fft.rfft(centered)
    psd = (dt / centered.size) * np.square(np.abs(fft_values))
    if psd.size > 2:
        psd[1:-1] *= 2.0
    return freq, psd


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


def mann_tensor_component(component: int, k1: float, k2: np.ndarray, k3: np.ndarray, gamma: float, length: float, alpha_eps: float) -> np.ndarray:
    k03 = k3 + gamma * k1
    k0sq = np.maximum(k1 * k1 + np.square(k2) + np.square(k03), 1.0e-30)
    k0 = np.sqrt(k0sq)
    k0l2 = np.square(k0 * max(length, 1.0e-12))
    energy = alpha_eps * np.power(max(length, 1.0e-12), 5.0 / 3.0) * np.square(k0l2) / np.power(1.0 + k0l2, 17.0 / 6.0)
    c0 = energy / (4.0 * math.pi * np.square(k0sq))

    phi00_iso = c0 * (np.square(k2) + np.square(k03))
    phi11_iso = c0 * (k1 * k1 + np.square(k03))
    phi22_iso = c0 * (k1 * k1 + np.square(k2))
    phi01_iso = -c0 * k1 * k2
    phi02_iso = -c0 * k1 * k03
    phi12_iso = -c0 * k2 * k03

    if component == 0:
        return phi00_iso + 2.0 * gamma * phi02_iso + gamma * gamma * phi22_iso
    if component == 1:
        return phi11_iso
    return phi22_iso


def mann_psd(freq: np.ndarray, sigma: float, length: float, u_hub: float, component: int, gamma: float, alpha_eps: float = 1.0) -> np.ndarray:
    freq = np.asarray(freq, dtype=np.float64)
    out = np.zeros_like(freq)
    if freq.size == 0 or sigma <= 0.0 or length <= 0.0:
        return out

    eta_limit = 18.0
    eta = np.linspace(-eta_limit, eta_limit, 121, dtype=np.float64)
    k2_grid, k3_grid = np.meshgrid(eta / length, eta / length, indexing="ij")

    for idx, value in enumerate(freq):
        k1 = 2.0 * math.pi * value / max(u_hub, 0.1)
        tensor = mann_tensor_component(component, k1, k2_grid, k3_grid, gamma, length, alpha_eps)
        integral = np.trapezoid(np.trapezoid(tensor, eta / length, axis=1), eta / length, axis=0)
        out[idx] = max(float(integral), 0.0)

    band_energy = float(np.trapezoid(out, freq))
    if band_energy > 0.0:
        out *= sigma * sigma / band_energy
    return out


def theoretical_psd_for_plot(sum_info: dict, header: dict, component: int, freq: np.ndarray) -> tuple[np.ndarray, np.ndarray, str] | None:
    positive_freq = np.asarray(freq, dtype=np.float64)
    positive_freq = positive_freq[np.isfinite(positive_freq) & (positive_freq > 0.0)]
    if positive_freq.size < 2:
        return None

    target_sigma = sum_info.get("target_sigma")
    scales = sum_info.get("derived", {}).get("IntegralScaleUVW")
    turb_model = int(sum_info.get("input", {}).get("TurbModel", -1))
    if not target_sigma or not scales or component >= len(target_sigma) or component >= len(scales):
        return None

    sigma = float(target_sigma[component])
    length_scale = float(scales[component])
    if sigma <= 0.0 or length_scale <= 0.0:
        return None

    u_hub = float(header.get("uhub", 0.0))
    if u_hub <= 0.0:
        u_hub = float(sum_info.get("input", {}).get("MeanWindSpeed", 0.0))
    if u_hub <= 0.0:
        return None

    theory_freq = np.geomspace(float(positive_freq[0]), float(positive_freq[-1]), min(240, positive_freq.size))

    if turb_model in (0, 3):
        return theory_freq, kaimal_psd(theory_freq, sigma, length_scale, u_hub), "Kaimal theory"
    if turb_model in (1, 4, 5):
        return theory_freq, von_karman_psd(theory_freq, sigma, length_scale, u_hub, component), "von Karman theory"
    if turb_model == 2:
        gamma = float(sum_info.get("derived", {}).get("MannGamma", 3.9))
        mann_length = float(sum_info.get("derived", {}).get("MannLength", length_scale))
        return theory_freq, mann_psd(theory_freq, sigma, mann_length, u_hub, component, gamma), "Mann tensor theory"
    return None


def make_plots(data: np.ndarray, header: dict, sum_info: dict, output_dir: Path, prefix: str) -> list[str]:
    if plt is None:
        return []

    output_dir.mkdir(parents=True, exist_ok=True)
    nt, nz, ny, _ = data.shape
    dt = float(header["dt"])
    mid_z, mid_y = hub_indices(header)
    time = np.arange(nt) * dt
    paths = []

    fig, ax = plt.subplots(figsize=(10, 4))
    for comp, name in enumerate(COMPONENT_NAMES):
        ax.plot(time, data[:, mid_z, mid_y, comp], label=name, linewidth=0.9)
    ax.set_xlabel("time [s]")
    ax.set_ylabel("wind speed [m/s]")
    ax.set_title("Hub grid-point time series")
    ax.grid(True, alpha=0.25)
    ax.legend()
    path = output_dir / f"{prefix}_hub_timeseries.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    paths.append(str(path))

    fig, ax = plt.subplots(figsize=(8, 4))
    hub = hub_series(data, header)
    for comp, name in enumerate(COMPONENT_NAMES):
        freq, psd = power_spectral_density(hub[:, comp], dt)
        mask = (freq > 0.0) & np.isfinite(psd) & (psd > 0.0)
        if np.any(mask):
            ax.loglog(freq[mask], psd[mask], label=f"{name} measured", linewidth=0.9)
            theory = theoretical_psd_for_plot(sum_info, header, comp, freq[mask])
            if theory is not None:
                theory_freq, theory_psd, theory_label = theory
                theory_mask = np.isfinite(theory_psd) & (theory_psd > 0.0)
                if np.any(theory_mask):
                    ax.loglog(theory_freq[theory_mask], theory_psd[theory_mask], linestyle="--", linewidth=1.0, label=f"{name} {theory_label}")
    ax.set_xlabel("frequency [Hz]")
    ax.set_ylabel("PSD [(m/s)^2/Hz]")
    ax.set_title("Hub-point one-sided PSD vs theoretical reference")
    ax.grid(True, which="both", alpha=0.25)
    handles, labels = ax.get_legend_handles_labels()
    if handles:
        ax.legend()
    path = output_dir / f"{prefix}_hub_psd.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    paths.append(str(path))

    fig, ax = plt.subplots(figsize=(6, 5))
    image = ax.imshow(data[0, :, :, 0], origin="lower", aspect="auto")
    ax.set_title("u component, first plane")
    ax.set_xlabel("lateral index")
    ax.set_ylabel("vertical index")
    fig.colorbar(image, ax=ax, label="m/s")
    path = output_dir / f"{prefix}_u_first_plane.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    paths.append(str(path))

    fig, ax = plt.subplots(figsize=(5, 4))
    cov = covariance_matrix(hub)
    image = ax.imshow(cov, cmap="coolwarm")
    ax.set_xticks(range(3), COMPONENT_NAMES)
    ax.set_yticks(range(3), COMPONENT_NAMES)
    ax.set_title("Hub covariance matrix")
    fig.colorbar(image, ax=ax, label="(m/s)^2")
    for i in range(3):
        for j in range(3):
            ax.text(j, i, f"{cov[i, j]:.3f}", ha="center", va="center", fontsize=8)
    path = output_dir / f"{prefix}_hub_covariance.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    paths.append(str(path))

    fig, ax = plt.subplots(figsize=(8, 4))
    for comp, name in enumerate(COMPONENT_NAMES):
        values = data[..., comp].ravel()
        ax.hist(values - values.mean(), bins=80, alpha=0.45, density=True, label=name)
    ax.set_xlabel("deviation from component mean [m/s]")
    ax.set_ylabel("density")
    ax.set_title("Component deviation histograms")
    ax.legend()
    path = output_dir / f"{prefix}_histograms.png"
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    paths.append(str(path))

    return paths


def validate(base: Path, tolerance: float, plot_dir: Path | None, check_sum_sigma: bool = True) -> dict:
    sum_path = base.with_suffix(".sum")
    bts_path = base.with_suffix(".bts")
    bladed_path = base.with_suffix(".wnd")
    turbsim_wnd_path = base.with_suffix(".ts.wnd")

    report: dict = {"base": str(base), "checks": [], "plots": []}
    if sum_path.exists():
        report["sum"] = parse_sum(sum_path)
    else:
        report["sum"] = {}

    if bts_path.exists():
        bts = read_bts(bts_path)
        report["bts_header"] = bts["header"]
        report["bts_stats"] = component_stats(bts["data"], bts["header"]["uhub"])
        hub = hub_series(bts["data"], bts["header"])
        hub_iz, hub_iy = hub_indices(bts["header"])
        report["hub_point_index"] = {"iy": int(hub_iy), "iz": int(hub_iz)}
        report["hub_stats"] = component_stats(hub.reshape((hub.shape[0], 1, 1, hub.shape[1])), bts["header"]["uhub"])
        report["hub_covariance"] = covariance_matrix(hub).tolist()
        target_sigma = report["sum"].get("target_sigma")
        if check_sum_sigma and target_sigma:
            for idx, name in enumerate(COMPONENT_NAMES):
                measured = report["bts_stats"][idx]["sigma"]
                target = target_sigma[idx]
                rel_error = abs(measured - target) / target if target > 0 else 0.0
                report["checks"].append(
                    {
                        "name": f"{name}_sigma_matches_sum_target",
                        "measured": measured,
                        "target": target,
                        "relative_error": rel_error,
                        "passed": rel_error <= tolerance,
                    }
                )
        if plot_dir is not None:
            report["plots"].extend(make_plots(bts["data"], bts["header"], report["sum"], plot_dir, base.name))

    if bladed_path.exists():
        wnd = read_bladed_wnd(bladed_path)
        report["bladed_wnd_header"] = wnd["header"]
        norm_stats = component_stats(wnd["normalized"], 1.0)
        report["bladed_wnd_normalized_stats"] = norm_stats
        for idx, name in enumerate(COMPONENT_NAMES):
            report["checks"].append(
                {
                    "name": f"bladed_{name}_normalized_unit_std",
                    "measured": norm_stats[idx]["sigma"],
                    "target": 1.0,
                    "relative_error": abs(norm_stats[idx]["sigma"] - 1.0),
                    "passed": abs(norm_stats[idx]["sigma"] - 1.0) <= tolerance,
                }
            )

    if turbsim_wnd_path.exists():
        ts_wnd = read_bladed_wnd(turbsim_wnd_path)
        report["turbsim_compatible_wnd_header"] = ts_wnd["header"]

    report["passed"] = all(check["passed"] for check in report["checks"])
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, help="Output base path without extension")
    parser.add_argument("--tolerance", type=float, default=0.15, help="Relative tolerance for sigma checks")
    parser.add_argument("--plot-dir", default=None, help="Directory for PNG plots")
    parser.add_argument("--json", default=None, help="Optional JSON report path")
    parser.add_argument("--skip-sum-sigma", action="store_true", help="Skip SUM target sigma checks when ScaleIEC=0 or the target sigma is not intended as a hard acceptance metric")
    args = parser.parse_args()

    base = Path(args.base)
    plot_dir = Path(args.plot_dir) if args.plot_dir else base.parent / f"{base.name}_validation"
    report = validate(base, args.tolerance, plot_dir, check_sum_sigma=not args.skip_sum_sigma)

    text = json.dumps(report, indent=2)
    print(text)
    if args.json:
        Path(args.json).write_text(text + "\n", encoding="utf-8")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
