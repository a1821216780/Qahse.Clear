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


def read_scalar(fh: BinaryIO, fmt: str):
    data = fh.read(struct.calcsize(fmt))
    if len(data) != struct.calcsize(fmt):
        raise EOFError("Unexpected end of file while reading binary wind file")
    return struct.unpack("<" + fmt, data)[0]


def parse_sum(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="ignore")
    result: dict = {"stats": {}, "target_sigma": None, "warnings": []}

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

    for warning in re.findall(r"^\s*-\s*(.+)$", text, flags=re.MULTILINE):
        result["warnings"].append(warning)

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


def make_plots(data: np.ndarray, dt: float, output_dir: Path, prefix: str) -> list[str]:
    if plt is None:
        return []

    output_dir.mkdir(parents=True, exist_ok=True)
    nt, nz, ny, _ = data.shape
    mid_z = nz // 2
    mid_y = ny // 2
    time = np.arange(nt) * dt
    names = ["u", "v", "w"]
    paths = []

    fig, ax = plt.subplots(figsize=(10, 4))
    for comp, name in enumerate(names):
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

    fig, ax = plt.subplots(figsize=(8, 4))
    for comp, name in enumerate(names):
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


def validate(base: Path, tolerance: float, plot_dir: Path | None) -> dict:
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
        target_sigma = report["sum"].get("target_sigma")
        if target_sigma:
            for idx, name in enumerate(("u", "v", "w")):
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
            report["plots"].extend(make_plots(bts["data"], bts["header"]["dt"], plot_dir, base.name))

    if bladed_path.exists():
        wnd = read_bladed_wnd(bladed_path)
        report["bladed_wnd_header"] = wnd["header"]
        norm_stats = component_stats(wnd["normalized"], 1.0)
        report["bladed_wnd_normalized_stats"] = norm_stats
        for idx, name in enumerate(("u", "v", "w")):
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
    args = parser.parse_args()

    base = Path(args.base)
    plot_dir = Path(args.plot_dir) if args.plot_dir else base.parent / f"{base.name}_validation"
    report = validate(base, args.tolerance, plot_dir)

    text = json.dumps(report, indent=2)
    print(text)
    if args.json:
        Path(args.json).write_text(text + "\n", encoding="utf-8")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
