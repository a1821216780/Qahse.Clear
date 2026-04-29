#!/usr/bin/env python3
"""Compare 100x100 large-grid SimWind cases across turbulence models."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

import numpy as np

try:
    import matplotlib.pyplot as plt
except Exception:  # pragma: no cover
    plt = None

try:
    from scipy.signal import welch
except Exception as exc:  # pragma: no cover
    raise RuntimeError("scipy is required for PSD comparison plots") from exc

SCRIPT_DIR = Path(__file__).resolve().parent

from validate_simwind_output import hub_series, parse_sum, read_bts, theoretical_psd_for_plot  # noqa: E402


COMPONENTS = ("u", "v", "w")
CASE_SPECS = (
    ("BladedVK", SCRIPT_DIR / "result" / "large_grid_compare" / "LG100_BladedVK"),
    ("BladedIVK", SCRIPT_DIR / "result" / "large_grid_compare" / "LG100_BladedIVK"),
    ("Mann", SCRIPT_DIR / "result" / "large_grid_compare" / "LG100_Mann"),
)
CASE_COLORS = {
    "BladedVK": "#1f77b4",
    "BladedIVK": "#d62728",
    "Mann": "#2ca02c",
}


def hub_stats(series: np.ndarray, uhub: float) -> list[dict[str, float]]:
    stats: list[dict[str, float]] = []
    for comp in range(series.shape[1]):
        values = np.asarray(series[:, comp], dtype=np.float64)
        sigma = float(values.std())
        stats.append(
            {
                "mean": float(values.mean()),
                "sigma": sigma,
                "ti_percent": float(100.0 * sigma / uhub) if uhub > 0.0 else math.nan,
            }
        )
    return stats


def psd_report(series: np.ndarray, dt: float) -> list[dict[str, Any]]:
    reports: list[dict[str, Any]] = []
    fs = 1.0 / dt
    nperseg = min(8192, series.shape[0])
    for comp in range(series.shape[1]):
        freq, psd = welch(
            np.asarray(series[:, comp], dtype=np.float64),
            fs=fs,
            window="hann",
            nperseg=nperseg,
            noverlap=nperseg // 2,
            detrend="constant",
            scaling="density",
        )
        reports.append(
            {
                "freq": freq.tolist(),
                "psd": psd.tolist(),
                "peak_frequency_hz": float(freq[int(np.argmax(psd[1:])) + 1]) if psd.size > 1 else 0.0,
                "peak_psd": float(np.max(psd[1:])) if psd.size > 1 else 0.0,
            }
        )
    return reports


def load_case(name: str, base: Path) -> dict[str, Any]:
    bts = read_bts(base.with_suffix(".bts"))
    sum_info = parse_sum(base.with_suffix(".sum"))
    series = hub_series(bts["data"], bts["header"])
    stats = hub_stats(series, bts["header"]["uhub"])
    target_sigma = sum_info.get("target_sigma") or [0.0, 0.0, 0.0]
    sigma_rel_error = []
    for idx in range(3):
        target = float(target_sigma[idx])
        measured = stats[idx]["sigma"]
        sigma_rel_error.append(abs(measured - target) / target if target > 0.0 else 0.0)
    return {
        "name": name,
        "base": str(base),
        "header": bts["header"],
        "hub_series": series,
        "hub_stats": stats,
        "sum": sum_info,
        "sigma_relative_error": sigma_rel_error,
        "psd": psd_report(series, bts["header"]["dt"]),
    }


def make_time_plot(cases: list[dict[str, Any]], output_dir: Path, window_seconds: float) -> str | None:
    if plt is None:
        return None
    dt = float(cases[0]["header"]["dt"])
    count = min(cases[0]["hub_series"].shape[0], max(1, int(round(window_seconds / dt))))
    time_axis = np.arange(count, dtype=np.float64) * dt

    fig, axes = plt.subplots(3, 1, figsize=(12, 9), sharex=True)
    for comp, ax in enumerate(axes):
        for case in cases:
            ax.plot(
                time_axis,
                case["hub_series"][:count, comp],
                label=case["name"],
                linewidth=0.9,
                color=CASE_COLORS.get(case["name"]),
            )
        ax.set_ylabel(f"{COMPONENTS[comp]} [m/s]")
        ax.grid(True, alpha=0.25)
        ax.legend(loc="upper right")
    axes[-1].set_xlabel("time [s]")
    fig.suptitle(f"Hub-point wind speed comparison ({window_seconds:.0f} s window)")
    path = output_dir / "LG100_hub_timeseries_overlay.png"
    fig.tight_layout()
    fig.savefig(path, dpi=170)
    plt.close(fig)
    return str(path)


def make_psd_plot(cases: list[dict[str, Any]], output_dir: Path) -> str | None:
    if plt is None:
        return None

    fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)
    for comp, ax in enumerate(axes):
        for case in cases:
            freq = np.asarray(case["psd"][comp]["freq"], dtype=np.float64)
            psd = np.asarray(case["psd"][comp]["psd"], dtype=np.float64)
            mask = (freq > 0.0) & np.isfinite(psd) & (psd > 0.0)
            color = CASE_COLORS.get(case["name"])
            ax.loglog(freq[mask], psd[mask], label=f"{case['name']} measured", linewidth=1.0, color=color)
            theory = theoretical_psd_for_plot(case["sum"], case["header"], comp, freq[mask])
            if theory is not None:
                theory_freq, theory_psd, _ = theory
                theory_mask = np.isfinite(theory_psd) & (theory_psd > 0.0)
                if np.any(theory_mask):
                    ax.loglog(
                        theory_freq[theory_mask],
                        theory_psd[theory_mask],
                        linestyle="--",
                        linewidth=1.0,
                        color=color,
                        alpha=0.9,
                        label=f"{case['name']} theory",
                    )
        ax.set_ylabel(f"{COMPONENTS[comp]} PSD")
        ax.grid(True, which="both", alpha=0.25)
        ax.legend(loc="upper right")
    axes[-1].set_xlabel("frequency [Hz]")
    fig.suptitle("Hub-point PSD comparison vs theoretical spectra")
    path = output_dir / "LG100_hub_psd_overlay.png"
    fig.tight_layout()
    fig.savefig(path, dpi=170)
    plt.close(fig)
    return str(path)


def make_sigma_plot(cases: list[dict[str, Any]], output_dir: Path) -> str | None:
    if plt is None:
        return None

    x = np.arange(len(COMPONENTS), dtype=np.float64)
    width = 0.22
    fig, axes = plt.subplots(1, 2, figsize=(13, 4.5))

    for idx, case in enumerate(cases):
        sigma = [case["hub_stats"][comp]["sigma"] for comp in range(3)]
        ti = [case["hub_stats"][comp]["ti_percent"] for comp in range(3)]
        offset = (idx - (len(cases) - 1) / 2.0) * width
        axes[0].bar(x + offset, sigma, width=width, label=case["name"], color=CASE_COLORS.get(case["name"]))
        axes[1].bar(x + offset, ti, width=width, label=case["name"], color=CASE_COLORS.get(case["name"]))

    axes[0].set_xticks(x, COMPONENTS)
    axes[0].set_ylabel("sigma [m/s]")
    axes[0].grid(True, axis="y", alpha=0.25)
    axes[0].legend(loc="upper right")

    axes[1].set_xticks(x, COMPONENTS)
    axes[1].set_ylabel("TI [% of Uhub]")
    axes[1].grid(True, axis="y", alpha=0.25)
    axes[1].legend(loc="upper right")

    fig.suptitle("Hub-point sigma and TI comparison")
    path = output_dir / "LG100_hub_sigma_ti_comparison.png"
    fig.tight_layout()
    fig.savefig(path, dpi=170)
    plt.close(fig)
    return str(path)


def write_report(cases: list[dict[str, Any]], plots: list[str], output_dir: Path) -> tuple[str, str]:
    summary = {
        "cases": [],
        "plots": plots,
    }

    lines = [
        "# Large-grid 100x100 model comparison",
        "",
        "Cases:",
        "- Bladed von Karman",
        "- Bladed improved von Karman",
        "- Mann",
        "",
        "Grid/time setup:",
        "- 100 x 100 grid",
        f"- dt = {cases[0]['header']['dt']:.5f} s",
        f"- Tmax = {cases[0]['header']['nt'] * cases[0]['header']['dt']:.2f} s",
        "",
        "| Case | u sigma | v sigma | w sigma | u TI [%] | v TI [%] | w TI [%] | u PSD peak [Hz] | v PSD peak [Hz] | w PSD peak [Hz] |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]

    for case in cases:
        stats = case["hub_stats"]
        peaks = case["psd"]
        lines.append(
            "| {name} | {su:.4f} | {sv:.4f} | {sw:.4f} | {tu:.3f} | {tv:.3f} | {tw:.3f} | {pu:.4f} | {pv:.4f} | {pw:.4f} |".format(
                name=case["name"],
                su=stats[0]["sigma"],
                sv=stats[1]["sigma"],
                sw=stats[2]["sigma"],
                tu=stats[0]["ti_percent"],
                tv=stats[1]["ti_percent"],
                tw=stats[2]["ti_percent"],
                pu=peaks[0]["peak_frequency_hz"],
                pv=peaks[1]["peak_frequency_hz"],
                pw=peaks[2]["peak_frequency_hz"],
            )
        )
        summary["cases"].append(
            {
                "name": case["name"],
                "base": case["base"],
                "header": case["header"],
                "hub_stats": case["hub_stats"],
                "target_sigma": case["sum"].get("target_sigma"),
                "sigma_relative_error": case["sigma_relative_error"],
                "psd_peaks": [
                    {
                        "component": COMPONENTS[idx],
                        "peak_frequency_hz": case["psd"][idx]["peak_frequency_hz"],
                        "peak_psd": case["psd"][idx]["peak_psd"],
                    }
                    for idx in range(3)
                ],
            }
        )

    lines.extend(
        [
            "",
            "Generated plots:",
        ]
    )
    for plot in plots:
        lines.append(f"- `{plot}`")

    json_path = output_dir / "LG100_model_compare_summary.json"
    md_path = output_dir / "LG100_model_compare_report.md"
    json_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return str(json_path), str(md_path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--window-seconds", type=float, default=120.0, help="Time window for overlay time-series plot")
    parser.add_argument(
        "--output-dir",
        default=str(SCRIPT_DIR / "result" / "large_grid_compare"),
        help="Directory for the comparison report and plots",
    )
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    cases = [load_case(name, base) for name, base in CASE_SPECS]
    plots = []
    for plot_path in (
        make_time_plot(cases, output_dir, args.window_seconds),
        make_psd_plot(cases, output_dir),
        make_sigma_plot(cases, output_dir),
    ):
        if plot_path:
            plots.append(plot_path)

    json_path, md_path = write_report(cases, plots, output_dir)
    print(
        json.dumps(
            {
                "passed": True,
                "summary": json_path,
                "report": md_path,
                "plots": plots,
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
