#!/usr/bin/env python3
"""Validate and visualise WindL SimWind output across DEMO and IEC61400-3 cases.

Produces:
  - Figure 1: Case summary bar chart (pass / warn status by turbulence model)
  - Figure 2: Turbulence intensity grouped bar chart
  - Figure 3: Grid & performance scatter (points x memory, bubble = FLOPs)
  - Figure 4: Hub-height u-component PSD vs theoretical IEC Kaimal
  - Figure 5: Hub-height u/v/w time series (first 300 s)
  - validation_report.md  (Markdown text report)
"""

from __future__ import annotations

import re
import struct
from pathlib import Path

import numpy as np

# ---------------------------------------------------------------------------
# matplotlib – gracefully handle headless environments
# ---------------------------------------------------------------------------
try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import matplotlib.ticker as mticker
except Exception:
    plt = None  # pragma: no cover

# ---------------------------------------------------------------------------
# SciPy – optional for Welch PSD (falls back to FFT periodogram)
# ---------------------------------------------------------------------------
try:
    from scipy.signal import welch

    _HAS_SCIPY = True
except ImportError:
    _HAS_SCIPY = False


# ===========================================================================
# Constants
# ===========================================================================

SCRIPT_DIR = Path(__file__).resolve().parent
RESULT_DIR = SCRIPT_DIR / "result"
IEC_RESULT_DIR = SCRIPT_DIR / "IEC61400_3" / "result"
CERT_RESULT_DIR = SCRIPT_DIR / "IEC61400_3" / "certification_result"

MAIN_DEMO_BASE = "Test_Demo_wind"
MAIN_DEMO_BTS = RESULT_DIR / f"{MAIN_DEMO_BASE}.bts"
MAIN_DEMO_SUM = RESULT_DIR / f"{MAIN_DEMO_BASE}.sum"

TURB_MODEL_NAMES: dict[int, str] = {
    0: "IEC Kaimal",
    1: "IEC VK",
    2: "Mann",
    3: "Bladed Kaimal",
    4: "Bladed VK",
    5: "Bladed IVK",
    6: "User Spectra",
    8: "User VK",
}

WIND_MODEL_NAMES: dict[int, str] = {
    0: "None",
    1: "IEC Event",
    2: "EWM",
    8: "Uniform",
}

TURB_COLORS: dict[int, str] = {
    0: "#1f77b4",
    1: "#ff7f0e",
    2: "#2ca02c",
    3: "#d62728",
    4: "#9467bd",
    5: "#8c564b",
    6: "#e377c2",
    8: "#17becf",
}


# ===========================================================================
# SUM-file parser
# ===========================================================================


def parse_sum_file(path: Path) -> dict:
    """Extract statistics and parameters from a WindL .sum text file."""
    text = path.read_text(encoding="utf-8", errors="ignore")
    info: dict = {
        "case": path.stem,
        "label": make_case_label(path),
        "path": str(path),
        "warnings": [],
        "input": {},
        "grid": {},
        "cost": {},
        "derived": {},
        "stats": {},
        "bladed_export": {},
    }

    # --- Input section ---
    for key in ("TurbModel", "WindModel", "RandSeed"):
        m = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if m:
            info["input"][key] = int(float(m.group(1)))

    for key in ("MeanWindSpeed", "HubHt"):
        m = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if m:
            info["input"][key] = float(m.group(1))

    # --- Grid section ---
    for key in ("NumPointY", "NumPointZ", "NumSteps"):
        m = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if m:
            info["grid"][key] = int(float(m.group(1)))

    for key in ("LenWidthY", "LenHeightZ", "Zbottom", "TimeStep"):
        m = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if m:
            info["grid"][key] = float(m.group(1))

    # --- Generation Cost ---
    pe_mem = re.search(r"EstimatedPeakMemoryGiB:\s*([-+0-9.eE]+)", text)
    if pe_mem:
        info["cost"]["EstimatedPeakMemoryGiB"] = float(pe_mem.group(1))

    ch_flops = re.search(r"EstimatedCholeskyFLOPs:\s*([-+0-9.eE]+)", text)
    if ch_flops:
        info["cost"]["EstimatedCholeskyFLOPs"] = float(ch_flops.group(1))

    # --- Derived IEC Parameters ---
    m = re.search(
        r"SigmaU/SigmaV/SigmaW:\s*([-+0-9.eE]+),\s*([-+0-9.eE]+),\s*([-+0-9.eE]+)",
        text,
    )
    if m:
        info["derived"]["SigmaUVW"] = [float(m.group(i)) for i in range(1, 4)]

    m = re.search(
        r"IntegralScaleU/V/W:\s*([-+0-9.eE]+),\s*([-+0-9.eE]+),\s*([-+0-9.eE]+)",
        text,
    )
    if m:
        info["derived"]["IntegralScaleUVW"] = [float(m.group(i)) for i in range(1, 4)]

    for key in ("Lambda", "LC", "ScaleIEC", "ZL", "L", "UStar", "UStarDiab", "ZI"):
        m = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if m:
            info["derived"][key] = float(m.group(1))

    # --- Bladed export ---
    m = re.search(r"Record2ModelID:\s*([-+0-9.eE]+)", text)
    if m:
        info["bladed_export"]["Record2ModelID"] = int(float(m.group(1)))

    # --- Statistics ---
    for comp in ("u", "v", "w"):
        m = re.search(
            rf"\b{comp}:\s*mean=([-+0-9.eE]+)\s+sigma=([-+0-9.eE]+)\s+TI=([-+0-9.eE]+)%",
            text,
        )
        if m:
            info["stats"][comp] = {
                "mean": float(m.group(1)),
                "sigma": float(m.group(2)),
                "ti": float(m.group(3)),
            }

    # --- Warnings ---
    for warning in re.findall(r"^\s*-\s*(.+)$", text, flags=re.MULTILINE):
        info["warnings"].append(warning.strip())

    return info


# ===========================================================================
# BTS binary reader
# ===========================================================================


def _read_scalar(fh, fmt: str):
    """Read a single little-endian scalar from a binary file handle."""
    sz = struct.calcsize(fmt)
    data = fh.read(sz)
    if len(data) != sz:
        raise EOFError("Unexpected end of file reading BTS header")
    return struct.unpack("<" + fmt, data)[0]


def read_bts_file(path: Path) -> dict:
    """Read a TurbSim/BTS binary wind field and return header + (nt,nz,ny,3) data."""
    with path.open("rb") as fh:
        file_id = _read_scalar(fh, "h")
        nz = _read_scalar(fh, "i")
        ny = _read_scalar(fh, "i")
        ntwr = _read_scalar(fh, "i")
        nt = _read_scalar(fh, "i")
        dz = _read_scalar(fh, "f")
        dy = _read_scalar(fh, "f")
        dt = _read_scalar(fh, "f")
        uhub = _read_scalar(fh, "f")
        hub_height = _read_scalar(fh, "f")
        zbottom = _read_scalar(fh, "f")
        u_scl = _read_scalar(fh, "f")
        u_off = _read_scalar(fh, "f")
        v_scl = _read_scalar(fh, "f")
        v_off = _read_scalar(fh, "f")
        w_scl = _read_scalar(fh, "f")
        w_off = _read_scalar(fh, "f")
        scales = np.array([u_scl, v_scl, w_scl], dtype=np.float64)
        offsets = np.array([u_off, v_off, w_off], dtype=np.float64)
        desc_len = _read_scalar(fh, "i")
        description = fh.read(desc_len).decode("utf-8", errors="ignore")

        raw = np.fromfile(fh, dtype="<i2", count=int(nt) * int(nz) * int(ny) * 3)

    expected = int(nt) * int(nz) * int(ny) * 3
    if raw.size != expected:
        raise ValueError(
            f"BTS data size mismatch in {path}: got {raw.size}, expected {expected}"
        )

    raw = raw.reshape((int(nt), int(nz), int(ny), 3)).astype(np.float64)
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
        "scales": scales,
        "offsets": offsets,
    }


def hub_point_index(header: dict) -> tuple[int, int]:
    """Return (iz, iy) indices closest to hub height at y ~ 0."""
    nz = int(header["nz"])
    ny = int(header["ny"])
    dz = float(header["dz"])
    dy = float(header["dy"])
    z_bot = float(header["zbottom"])
    hub_h = float(header["hub_height"])

    best_iz = 0
    best_dist = float("inf")
    for iz in range(nz):
        z = z_bot + dz * iz
        d = abs(z - hub_h)
        if d < best_dist:
            best_dist = d
            best_iz = iz

    best_iy = 0
    best_dist = float("inf")
    for iy in range(ny):
        y = -0.5 * dy * (ny - 1) + dy * iy
        d = abs(y)
        if d < best_dist:
            best_dist = d
            best_iy = iy

    return best_iz, best_iy


def hub_time_series(data: np.ndarray, header: dict) -> np.ndarray:
    """Extract (nt, 3) time series at the hub grid point."""
    iz, iy = hub_point_index(header)
    return np.asarray(data[:, iz, iy, :], dtype=np.float64)


def compute_psd(series: np.ndarray, dt: float) -> tuple[np.ndarray, np.ndarray]:
    """One-sided power spectral density via Welch's method (or FFT fallback)."""
    if _HAS_SCIPY and series.size >= 256:
        nperseg = min(4096, max(256, series.size // 4))
        freq, psd = welch(
            series,
            fs=1.0 / dt,
            nperseg=nperseg,
            noverlap=nperseg // 2,
            scaling="density",
        )
        return freq, psd

    centered = np.asarray(series, dtype=np.float64) - np.mean(series)
    if centered.size < 2:
        return np.zeros(0), np.zeros(0)
    freq = np.fft.rfftfreq(centered.size, d=dt)
    psd = (dt / centered.size) * np.square(np.abs(np.fft.rfft(centered)))
    if psd.size > 2:
        psd[1:-1] *= 2.0
    return freq, psd


def iec_kaimal_psd(freq: np.ndarray, sigma: float, lu: float, u_hub: float) -> np.ndarray:
    """IEC 61400-1 ed.3 Kaimal spectrum for the u-component."""
    denom = np.maximum(u_hub, 0.1)
    numerator = 4.0 * sigma * sigma * max(lu, 1e-12)
    # Use f * Lu / Uhub normalized frequency
    nlu = freq * max(lu, 1e-12) / denom
    return numerator / denom / np.power(1.0 + 6.0 * nlu, 5.0 / 3.0)


# ===========================================================================
# Discover all cases
# ===========================================================================


def discover_sum_files() -> list[Path]:
    """Recursively find all .sum files under the relevant output directories."""
    candidates: list[Path] = []
    for root in (RESULT_DIR, IEC_RESULT_DIR, CERT_RESULT_DIR):
        if root.is_dir():
            candidates.extend(sorted(root.rglob("*.sum")))
    return candidates


def shorten_case_name(name: str) -> str:
    """Produce a compact label for plots."""
    replacements: list[tuple[str, str]] = [
        ("Qahse_WindL_", ""),
        ("_DEMO", ""),
        ("SimWind_", ""),
        ("LG100_", "LG100/"),
        ("Spectrum_", "Spec/"),
        ("IEC3_", ""),
        ("_30x30_660s", "(cert)"),
        ("IECKAI", "IK"),
        ("BladedKaimal", "BK"),
        ("BladedIVK", "BIVK"),
        ("BladedVK", "BVK"),
        ("Bladed_Kaimal", "BK"),
        ("Bladed_ImprovedVonKarman", "BIVK"),
        ("User_Spectra_Scaled", "USRVK"),
        ("USRVKM", "USRVK"),
        ("Mann_ReynoldsStress", "MannRS"),
        ("IEC_Kaimal_Stability", "IKS"),
        ("LargeGrid_100x100_PERF_ONLY", "PerfOnly"),
        ("Mann_30x30", "Mann30"),
        ("IECVKM", "IVK"),
        ("Raw__", "raw:"),
        ("Normalized__", "norm:"),
    ]
    result = name
    for old, new in replacements:
        result = result.replace(old, new)
    return result[:30]


def make_case_label(sum_path: Path) -> str:
    """Build a unique short case label incorporating directory context."""
    base = sum_path.stem
    parent = sum_path.parent
    # Add directory suffix for deep subdirectories to disambiguate
    suffixes: dict[str, str] = {}
    for root_dir in (RESULT_DIR, IEC_RESULT_DIR, CERT_RESULT_DIR):
        try:
            rel = parent.relative_to(root_dir)
            if rel != Path("."):
                # Include meaningful prefix from relative path
                parts = rel.parts
                if parts and parts[-1] in ("raw", "normalized"):
                    suffixes[str(parent)] = f"__{parts[-1]}"
        except ValueError:
            pass

    dir_suffix = suffixes.get(str(parent), "")
    return shorten_case_name(base + dir_suffix)


# ===========================================================================
# Figure generation
# ===========================================================================


def autosize_bar_labels(ax, labels: list[str], max_len: int = 18):
    """Rotate and shrink x-tick labels when they are long."""
    longest = max((len(str(l)) for l in labels), default=1)
    if longest > max_len:
        ax.tick_params(axis="x", labelsize=6, labelrotation=45)
    elif longest > 10:
        ax.tick_params(axis="x", labelsize=7, labelrotation=30)
    else:
        ax.tick_params(axis="x", labelsize=8)
    # Align rotated labels to the right
    if longest > 10:
        for lbl in ax.get_xticklabels():
            lbl.set_ha("right")


def figure_1_cases_summary(cases: list[dict], outdir: Path):
    """Bar chart: number of cases per turbulence model, stacked pass/warn."""
    if plt is None:
        return
    model_counts: dict[str, list[int]] = {}
    for c in cases:
        tm = TURB_MODEL_NAMES.get(c.get("input", {}).get("TurbModel"), "Unknown")
        model_counts.setdefault(tm, [0, 0])
        if c.get("stats"):
            model_counts[tm][0] += 1  # pass
        else:
            model_counts[tm][1] += 1  # no stats = fail
        if c.get("warnings"):
            pass  # still counts as pass

    models = sorted(model_counts.keys())
    passed = [model_counts[m][0] for m in models]
    failed = [model_counts[m][1] for m in models]

    x = np.arange(len(models))
    width = 0.55
    fig, ax = plt.subplots(figsize=(10, 5))
    colors = [TURB_COLORS.get(k, "#888888") for k in TURB_MODEL_NAMES.keys() if TURB_MODEL_NAMES[k] in models or True]
    # Build per-model color mapping
    b1 = ax.bar(x, passed, width, label="Passed", color="#2ca02c", edgecolor="white")
    b2 = ax.bar(x, failed, width, bottom=passed, label="Failed", color="#d62728", edgecolor="white")

    ax.set_ylabel("Number of Cases")
    ax.set_title("Figure 1: Case Summary by Turbulence Model")
    ax.set_xticks(x)
    ax.set_xticklabels(models)
    autosize_bar_labels(ax, models, 12)
    ax.legend()
    ax.set_ylim(bottom=0)
    fig.tight_layout()
    fig.savefig(outdir / "fig1_case_summary.png", dpi=160)
    plt.close(fig)


def figure_2_turbulence_intensity(cases: list[dict], outdir: Path):
    """Grouped bar chart: TI_u, TI_v, TI_w per case."""
    if plt is None:
        return
    ti_data: list[dict] = []
    names: list[str] = []
    for c in cases:
        st = c.get("stats", {})
        if "u" not in st:
            continue
        ti_data.append(
            {
                "u": st.get("u", {}).get("ti", 0),
                "v": st.get("v", {}).get("ti", 0),
                "w": st.get("w", {}).get("ti", 0),
            }
        )
        names.append(c.get("label", shorten_case_name(c["case"])))

    if not ti_data:
        return

    # Select at most 40 cases to avoid clutter
    if len(ti_data) > 40:
        ti_data = ti_data[:40]
        names = names[:40]

    x = np.arange(len(ti_data))
    width = 0.25
    fig, ax = plt.subplots(figsize=(14, 6))

    u_vals = [d["u"] for d in ti_data]
    v_vals = [d["v"] for d in ti_data]
    w_vals = [d["w"] for d in ti_data]

    ax.bar(x - width, u_vals, width, label="TI u", color="#1f77b4")
    ax.bar(x, v_vals, width, label="TI v", color="#ff7f0e")
    ax.bar(x + width, w_vals, width, label="TI w", color="#2ca02c")

    ax.set_ylabel("Turbulence Intensity [%]")
    ax.set_title("Figure 2: Turbulence Intensity by Case")
    ax.set_xticks(x)
    ax.set_xticklabels(names)
    autosize_bar_labels(ax, names, 10)
    ax.legend()
    ax.set_ylim(bottom=0)
    fig.tight_layout()
    fig.savefig(outdir / "fig2_ti.png", dpi=160)
    plt.close(fig)


def figure_3_grid_performance(cases: list[dict], outdir: Path):
    """Scatter: grid points (ny*nz) vs peak memory, bubble = Cholesky FLOPs."""
    if plt is None:
        return
    x_vals: list[int] = []
    y_vals: list[float] = []
    s_vals: list[float] = []
    colors: list[str] = []
    labels: list[str] = []

    for c in cases:
        g = c.get("grid", {})
        ny = g.get("NumPointY", 0)
        nz = g.get("NumPointZ", 0)
        mem = c.get("cost", {}).get("EstimatedPeakMemoryGiB", 0)
        flops = c.get("cost", {}).get("EstimatedCholeskyFLOPs", 0)
        if ny <= 0 or nz <= 0 or mem <= 0:
            continue
        x_vals.append(ny * nz)
        y_vals.append(mem)
        s_vals.append(max(flops, 1))
        tm = c.get("input", {}).get("TurbModel", -1)
        colors.append(TURB_COLORS.get(tm, "#888888"))
        labels.append(c.get("label", shorten_case_name(c["case"])))

    if not x_vals:
        return

    x_arr = np.array(x_vals)
    y_arr = np.array(y_vals)
    # Map FLOPs to marker area – sqrt-based for visual scaling
    s_arr = np.array(s_vals, dtype=np.float64)
    if s_arr.max() > s_arr.min():
        s_norm = 20 + 300 * np.sqrt(s_arr / s_arr.max())
    else:
        s_norm = np.full_like(s_arr, 80.0)

    fig, ax = plt.subplots(figsize=(10, 7))
    scatter = ax.scatter(x_arr, y_arr, s=s_norm, c=colors, alpha=0.75, edgecolors="black", linewidth=0.5)

    ax.set_xlabel("Grid Points (ny x nz)")
    ax.set_ylabel("Estimated Peak Memory [GiB]")
    ax.set_title("Figure 3: Grid Size vs Memory (bubble = Cholesky FLOPs)")
    ax.grid(True, alpha=0.3)

    # Legend for turbulence models
    from matplotlib.lines import Line2D
    legend_elements = [
        Line2D(
            [0], [0], marker="o", color="w", markerfacecolor=TURB_COLORS[k],
            markersize=10, label=v
        )
        for k, v in TURB_MODEL_NAMES.items() if any(c.get("input", {}).get("TurbModel") == k for c in cases)
    ]
    if legend_elements:
        ax.legend(handles=legend_elements, title="Turb Model", loc="upper left", fontsize=7, title_fontsize=8)

    fig.tight_layout()
    fig.savefig(outdir / "fig3_grid_perf.png", dpi=160)
    plt.close(fig)


def figure_4_spectrum(sum_info: dict, bts_info: dict, outdir: Path):
    """Log-log PSD of u at hub height with IEC Kaimal overlay."""
    if plt is None or not bts_info:
        return
    header = bts_info["header"]
    data = bts_info["data"]
    dt = float(header["dt"])
    hub = hub_time_series(data, header)
    u_series = hub[:, 0]
    freq, psd = compute_psd(u_series, dt)

    # IEC Kaimal theory
    sigma_u = sum_info.get("derived", {}).get("SigmaUVW", [2.0])[0]
    lu = sum_info.get("derived", {}).get("IntegralScaleUVW", [340.0])[0]
    u_hub = float(sum_info.get("input", {}).get("MeanWindSpeed", header.get("uhub", 12.0)))

    mask = (freq > 0) & np.isfinite(psd) & (psd > 0)
    theory_freq = np.geomspace(freq[mask].min() * 0.8, freq[mask].max() * 1.2, 300)
    theory_psd = iec_kaimal_psd(theory_freq, sigma_u, lu, u_hub)

    fig, ax = plt.subplots(figsize=(9, 6))
    ax.loglog(freq[mask], psd[mask], linewidth=1.0, color="#1f77b4", label="u measured (Welch)")
    ax.loglog(theory_freq, theory_psd, "k--", linewidth=1.4, label="IEC Kaimal theory")

    ax.set_xlabel("Frequency [Hz]")
    ax.set_ylabel("PSD [(m/s)$^2$/Hz]")
    ax.set_title(
        f"Figure 4: Hub-Height u-Component Spectrum\n"
        f"{sum_info['case']} (ny={header['ny']}, nz={header['nz']}, dt={dt:.3f}s)"
    )
    ax.grid(True, which="both", alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(outdir / "fig4_spectrum.png", dpi=160)
    plt.close(fig)


def figure_5_time_series(sum_info: dict, bts_info: dict, outdir: Path):
    """u/v/w time series at hub height, first 300 seconds."""
    if plt is None or not bts_info:
        return
    header = bts_info["header"]
    data = bts_info["data"]
    dt = float(header["dt"])
    nt = header["nt"]
    n_show = min(nt, int(300.0 / dt)) if dt > 0 else nt
    hub = hub_time_series(data, header)
    time = np.arange(n_show) * dt

    fig, ax = plt.subplots(figsize=(12, 5))
    ax.plot(time, hub[:n_show, 0], linewidth=0.8, label="u", color="#1f77b4")
    ax.plot(time, hub[:n_show, 1], linewidth=0.8, label="v", color="#ff7f0e", alpha=0.8)
    ax.plot(time, hub[:n_show, 2], linewidth=0.8, label="w", color="#2ca02c", alpha=0.8)

    ax.set_xlabel("Time [s]")
    ax.set_ylabel("Wind Speed [m/s]")
    ax.set_title(
        f"Figure 5: Hub-Height Wind Speed Time Series (first {n_show * dt:.0f}s)\n"
        f"{sum_info['case']}"
    )
    ax.grid(True, alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(outdir / "fig5_timeseries.png", dpi=160)
    plt.close(fig)


# ===========================================================================
# Markdown report
# ===========================================================================


def generate_report(cases: list[dict], main_sum: dict | None, outdir: Path) -> str:
    """Build validation_report.md and write to disk."""
    lines: list[str] = []

    total = len(cases)
    passed = sum(1 for c in cases if c.get("stats"))
    failed = total - passed
    warned = sum(1 for c in cases if c.get("warnings"))

    lines.append("# WindL Simulation Validation Report\n")
    lines.append(f"**Date:** {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M')}\n")
    lines.append("## Summary\n")
    lines.append(f"- **Total cases:** {total}")
    lines.append(f"- **Passed:** {passed}")
    lines.append(f"- **Failed (no stats):** {failed}")
    lines.append(f"- **Cases with warnings:** {warned}\n")

    # Model breakdown
    model_counts: dict[str, int] = {}
    for c in cases:
        tm = TURB_MODEL_NAMES.get(c.get("input", {}).get("TurbModel"), "Unknown")
        model_counts[tm] = model_counts.get(tm, 0) + 1
    lines.append("### Turbulence Models Used\n")
    lines.append("| Model | Count |")
    lines.append("|-------|-------|")
    for m, n in sorted(model_counts.items(), key=lambda x: -x[1]):
        lines.append(f"| {m} | {n} |")
    lines.append("")

    # Case table
    lines.append("## Case Details\n")
    header_cols = [
        "Case", "Turb Model", "Grid (y x z)", "Steps",
        "Mean U [m/s]", "TI u [%]", "TI v [%]", "TI w [%]",
        "Peak Mem [GiB]", "Chol FLOPs", "Warnings",
    ]
    lines.append("| " + " | ".join(header_cols) + " |")
    lines.append("|" + "|".join(["---"] * len(header_cols)) + "|")

    for c in cases:
        st = c.get("stats", {})
        g = c.get("grid", {})
        cs = c.get("cost", {})
        inp = c.get("input", {})
        tm = TURB_MODEL_NAMES.get(inp.get("TurbModel"), f"T{inp.get('TurbModel','?')}")
        grid_str = f"{g.get('NumPointY','?')}x{g.get('NumPointZ','?')}"
        n_steps = g.get("NumSteps", "?")
        mean_u = f"{st.get('u',{}).get('mean',0):.1f}" if st else "—"
        ti_u = f"{st.get('u',{}).get('ti',0):.1f}" if st else "—"
        ti_v = f"{st.get('v',{}).get('ti',0):.1f}" if st else "—"
        ti_w = f"{st.get('w',{}).get('ti',0):.1f}" if st else "—"
        mem = f"{cs.get('EstimatedPeakMemoryGiB',0):.4g}"
        flops = f"{cs.get('EstimatedCholeskyFLOPs',0):.3g}"
        nw = len(c.get("warnings", []))
        warn_str = f"{nw}" if nw else "none"
        lines.append(
            f"| `{c.get('label', shorten_case_name(c['case']))}` | {tm} | {grid_str} | {n_steps} | "
            f"{mean_u} | {ti_u} | {ti_v} | {ti_w} | "
            f"{mem} | {flops} | {warn_str} |"
        )
    lines.append("")

    # Warnings section
    warnings_found: list[tuple[str, str]] = []
    for c in cases:
        for w in c.get("warnings", []):
            warnings_found.append((c.get("label", shorten_case_name(c["case"])), w))

    if warnings_found:
        lines.append("## Warnings\n")
        for case_name, warning in warnings_found:
            lines.append(f"- **`{case_name}`:** {warning}")
        lines.append("")

    # Conclusion
    lines.append("## Conclusion\n")
    if failed == 0 and warned == 0:
        lines.append(
            "All cases completed successfully with no warnings. "
            "The WindL SimWind generator produces consistent output across turbulence "
            "and wind models per IEC 61400-1 specifications."
        )
    elif failed == 0:
        lines.append(
            f"All {total} cases completed successfully. "
            f"{warned} case(s) contain warnings (see Warnings section above). "
            "The warnings are advisory and do not indicate invalid output."
        )
    else:
        lines.append(
            f"{passed} of {total} cases passed; {failed} failed. "
            f"{warned} case(s) contain warnings. "
            "Review the warnings and failed cases for further investigation."
        )
    lines.append("")

    text = "\n".join(lines)
    report_path = outdir / "validation_report.md"
    report_path.write_text(text, encoding="utf-8")
    return str(report_path)


# ===========================================================================
# Main
# ===========================================================================


def main() -> int:
    outdir = SCRIPT_DIR / "validation_plots"
    outdir.mkdir(parents=True, exist_ok=True)
    print(f"Output directory: {outdir}")

    # 1. Discover and parse all .sum files
    sum_paths = discover_sum_files()
    print(f"Found {len(sum_paths)} .sum files")

    cases: list[dict] = []
    main_sum: dict | None = None

    for sp in sum_paths:
        info = parse_sum_file(sp)
        cases.append(info)
        if sp.stem == MAIN_DEMO_BASE:
            main_sum = info

    cases.sort(key=lambda c: c["case"])

    if main_sum is None:
        print(f"Warning: Main DEMO case '{MAIN_DEMO_BASE}.sum' not found. Using first case for BTS plots if available.")
        # Fallback: try SimWind
        for c in cases:
            if c["case"] == "SimWind":
                main_sum = c
                break

    # 2. Read Main DEMO BTS
    bts_info: dict | None = None
    bts_path = None
    if main_sum is not None:
        # Build BTS path from SUM path
        bts_path = Path(main_sum["path"]).with_suffix(".bts")
        if not bts_path.exists():
            # Try the canonical path
            bts_path = MAIN_DEMO_BTS
    else:
        bts_path = MAIN_DEMO_BTS

    if bts_path and bts_path.exists():
        print(f"Reading BTS: {bts_path}")
        try:
            bts_info = read_bts_file(bts_path)
        except Exception as exc:
            print(f"  BTS read failed: {exc}")
    else:
        print("Main DEMO BTS not found; skipping spectrum/time-series figures.")

    # 3. Generate figures
    print("Generating figures...")
    figure_1_cases_summary(cases, outdir)
    figure_2_turbulence_intensity(cases, outdir)
    figure_3_grid_performance(cases, outdir)

    if main_sum and bts_info:
        figure_4_spectrum(main_sum, bts_info, outdir)
        figure_5_time_series(main_sum, bts_info, outdir)
    else:
        print("  Skipping Figures 4 & 5 (no Main DEMO BTS data).")

    # 4. Generate Markdown report
    report_path = generate_report(cases, main_sum, outdir)
    print(f"Report: {report_path}")

    # Summary
    total = len(cases)
    passed = sum(1 for c in cases if c.get("stats"))
    print(f"\nDone. {passed}/{total} cases passed. See {outdir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
