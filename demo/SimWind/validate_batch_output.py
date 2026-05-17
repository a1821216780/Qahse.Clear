#!/usr/bin/env python3
"""Validate SimWind batch .sum output against IEC 61400-1 Ed.3 formulas.

Parses all .sum files from output_IEC3_1, computes IEC expected turbulence
statistics, generates validation plots, and writes a text report.
"""

from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except Exception:
    plt = None


# ────────────────────────────────── constants ──────────────────────────────────

OUTPUT_DIR = Path(r"D:\SimWind_SmokeTest\output_IEC3_1")
PLOT_DIR = Path(r"D:\SimWind_SmokeTest\validation_plots")
REPORT_PATH = Path(r"D:\SimWind_SmokeTest\validation_report.txt")

WIND_MODEL_MAP = {0: "NTM", 1: "ETM", 2: "EWM1", 3: "EWM50",
                  4: "EOG", 5: "EDC", 6: "ECD", 7: "EWS", 8: "UNIFORM"}

TURB_MODEL_MAP = {0: "IEC_KAIMAL", 1: "IEC_VKAIMAL", 2: "B_MANN",
                  3: "BLADED_KAIMAL", 4: "BLADED_VK", 5: "BLADED_IVK",
                  6: "USER_SPECTRA", 8: "USER_VK"}

# IEC 61400-1 Ed.3 Class I parameters
V_REF = 50.0       # Class I reference wind speed
I_REF_B = 0.14     # Class B turbulence intensity reference


# ─────────────────────────────── IEC formulas ────────────────────────────────

def iec_ntm_sigma_u(v_hub: float, iref: float = I_REF_B) -> float:
    """IEC 61400-1 Ed.3 Section 6.3.1.3: NTM σ_u."""
    return iref * (0.75 * v_hub + 5.6)


def iec_ntm_ti(v_hub: float, iref: float = I_REF_B) -> float:
    """IEC NTM turbulence intensity in percent."""
    sigma_u = iec_ntm_sigma_u(v_hub, iref)
    return 100.0 * sigma_u / v_hub if v_hub > 0 else 0.0


def iec_etm_sigma_u(v_hub: float, iref: float = I_REF_B,
                    c: float = 2.0, v_ref: float = V_REF) -> float:
    """IEC 61400-1 Ed.3 Section 6.3.2.3: ETM σ_u."""
    v_ave = 0.2 * v_ref
    return c * iref * (0.072 * (v_ave / c + 3.0) * (v_hub / c - 4.0) + 10.0)


def iec_etm_ti(v_hub: float, iref: float = I_REF_B) -> float:
    sigma_u = iec_etm_sigma_u(v_hub, iref)
    return 100.0 * sigma_u / v_hub if v_hub > 0 else 0.0


def iec_ewm_sigma_u(ewm_type: int) -> float:
    """IEC 61400-1 Ed.3 Section 6.3.2.1: EWM turbulent σ_u.
    ewm_type: 1 → EWM1, 50 → EWM50."""
    v_e50 = 1.4 * V_REF  # 70
    v_e1 = 0.8 * v_e50    # 56
    if ewm_type == 50:
        return 0.11 * v_e50
    return 0.11 * v_e1


# ──────────────────────────────── SUM parser ──────────────────────────────────

def parse_sum_file(path: Path) -> dict | None:
    """Parse a SimWind .sum file; returns dict or None on failure."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return None

    result: dict = {
        "path": str(path),
        "case_name": path.parent.name,
        "file_name": path.stem,
        "input": {},
        "grid": {},
        "derived": {},
        "stats": {},
    }

    # Input section
    for key in ("TurbModel", "WindModel", "RandSeed"):
        m = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if m:
            result["input"][key] = int(float(m.group(1)))

    for key in ("MeanWindSpeed", "HubHt"):
        m = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if m:
            result["input"][key] = float(m.group(1))

    # Grid section
    for key in ("NumPointY", "NumPointZ", "NumSteps"):
        m = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if m:
            result["grid"][key] = int(float(m.group(1)))

    for key in ("TimeStep",):
        m = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if m:
            result["grid"][key] = float(m.group(1))

    # Derived IEC parameters
    m = re.search(
        r"SigmaU/SigmaV/SigmaW:\s*([-+0-9.eE]+),\s*([-+0-9.eE]+),\s*([-+0-9.eE]+)",
        text,
    )
    if m:
        result["derived"]["SigmaU"] = float(m.group(1))
        result["derived"]["SigmaV"] = float(m.group(2))
        result["derived"]["SigmaW"] = float(m.group(3))

    for key in ("ScaleIEC", "Lambda", "LC"):
        m = re.search(rf"\b{key}:\s*([-+0-9.eE]+)", text)
        if m:
            result["derived"][key] = float(m.group(1))

    # Statistics
    for comp in ("u", "v", "w"):
        m = re.search(
            rf"\b{comp}:\s*mean=([-+0-9.eE]+)\s+sigma=([-+0-9.eE]+)\s+TI=([-+0-9.eE]+)%",
            text,
        )
        if m:
            result["stats"][comp] = {
                "mean": float(m.group(1)),
                "sigma": float(m.group(2)),
                "ti": float(m.group(3)),
            }

    return result


# ──────────────────────────────── validation ─────────────────────────────────

NEGLIGIBLE_SIGMA = 1e-6  # threshold for "zero" sigma check


def classify_wind_case(rec: dict) -> str:
    """Return category label for the wind model + steady/turbulent distinction."""
    wm = rec.get("input", {}).get("WindModel")
    if wm is None:
        return "Unknown"
    name = WIND_MODEL_MAP.get(wm, f"WM{wm}")
    if wm in (2, 3):  # EWM1 or EWM50
        sigma_u_derived = rec.get("derived", {}).get("SigmaU", -1)
        if sigma_u_derived is not None and sigma_u_derived < NEGLIGIBLE_SIGMA:
            return f"{name}_Steady"
        return f"{name}_Turbulent"
    return name


def is_deterministic(wind_model: int | None) -> bool:
    """True for EOG, EDC, ECD, EWS."""
    return wind_model in (4, 5, 6, 7)


def is_steady_ewm(rec: dict) -> bool:
    """Detect EWM-steady (SigmaU=0 in derived)."""
    wm = rec.get("input", {}).get("WindModel")
    sigma_u = rec.get("derived", {}).get("SigmaU", -1)
    return wm in (2, 3) and sigma_u is not None and sigma_u < NEGLIGIBLE_SIGMA


def validate_case(rec: dict) -> dict:
    """Compute IEC expected values and pass/fail flags for a single case."""
    inp = rec.get("input", {})
    st = rec.get("stats", {})
    der = rec.get("derived", {})
    wm = inp.get("WindModel")
    v_hub = inp.get("MeanWindSpeed", 0.0)
    sigma_u_meas = st.get("u", {}).get("sigma")
    sigma_v_meas = st.get("v", {}).get("sigma")
    sigma_w_meas = st.get("w", {}).get("sigma")
    ti_u_meas = st.get("u", {}).get("ti")
    sigma_u_drv = der.get("SigmaU")

    iec_expected: dict = {
        "sigma_u": None,
        "sigma_v_min": None,
        "sigma_w_min": None,
        "ti_u": None,
        "sigma_should_be_zero": False,
        "formula": "",
    }

    checks: dict = {
        "sigma_u_pct_err": None,
        "sigma_u_pass": None,
        "sigma_v_ratio": None,
        "sigma_v_pass": None,
        "sigma_w_ratio": None,
        "sigma_w_pass": None,
        "sigma_zero_pass": None,
    }

    if wm is None:
        return {"iec_expected": iec_expected, "checks": checks}

    tol = 0.10  # ±10%

    def _ratio_checks(exp_su: float):
        """Check derived target Sigma ratios against IEC minima (design validation).
        These use the DERIVED SigmaU/V/W (what SimWind intends to produce), not the
        measured statistics (which contain sampling noise from finite duration)."""
        su_d = der.get("SigmaU")
        sv_d = der.get("SigmaV")
        sw_d = der.get("SigmaW")
        if su_d is not None and su_d > 0:
            if sv_d is not None:
                checks["sigma_v_ratio"] = sv_d / su_d
                checks["sigma_v_pass"] = checks["sigma_v_ratio"] >= 0.7
            if sw_d is not None:
                checks["sigma_w_ratio"] = sw_d / su_d
                checks["sigma_w_pass"] = checks["sigma_w_ratio"] >= 0.5

    # NTM
    if wm == 0:
        exp_su = iec_ntm_sigma_u(v_hub)
        iec_expected["sigma_u"] = exp_su
        iec_expected["sigma_v_min"] = 0.7 * exp_su
        iec_expected["sigma_w_min"] = 0.5 * exp_su
        iec_expected["ti_u"] = iec_ntm_ti(v_hub)
        iec_expected["formula"] = "Iref*(0.75*Vhub+5.6)"
        if sigma_u_meas is not None and exp_su > 0:
            pct = (sigma_u_meas - exp_su) / exp_su * 100.0
            checks["sigma_u_pct_err"] = pct
            checks["sigma_u_pass"] = abs(pct) <= tol * 100.0
        _ratio_checks(exp_su)

    # ETM
    elif wm == 1:
        exp_su = iec_etm_sigma_u(v_hub)
        iec_expected["sigma_u"] = exp_su
        iec_expected["sigma_v_min"] = 0.7 * exp_su
        iec_expected["sigma_w_min"] = 0.5 * exp_su
        iec_expected["ti_u"] = iec_etm_ti(v_hub)
        iec_expected["formula"] = "c*Iref*(0.072*(Vave/c+3)*(Vhub/c-4)+10)"
        if sigma_u_meas is not None and exp_su > 0:
            pct = (sigma_u_meas - exp_su) / exp_su * 100.0
            checks["sigma_u_pct_err"] = pct
            checks["sigma_u_pass"] = abs(pct) <= tol * 100.0
        _ratio_checks(exp_su)

    # EWM1 Turbulent
    elif wm == 2 and not is_steady_ewm(rec):
        exp_su = iec_ewm_sigma_u(1)
        iec_expected["sigma_u"] = exp_su
        iec_expected["ti_u"] = 100.0 * exp_su / v_hub if v_hub > 0 else 0.0
        iec_expected["formula"] = "0.11*Ve1 (Ve1=56m/s)"
        if sigma_u_meas is not None:
            pct = (sigma_u_meas - exp_su) / exp_su * 100.0
            checks["sigma_u_pct_err"] = pct
            checks["sigma_u_pass"] = abs(pct) <= tol * 100.0

    # EWM50 Turbulent
    elif wm == 3 and not is_steady_ewm(rec):
        exp_su = iec_ewm_sigma_u(50)
        iec_expected["sigma_u"] = exp_su
        iec_expected["ti_u"] = 100.0 * exp_su / v_hub if v_hub > 0 else 0.0
        iec_expected["formula"] = "0.11*Ve50 (Ve50=70m/s)"
        if sigma_u_meas is not None:
            pct = (sigma_u_meas - exp_su) / exp_su * 100.0
            checks["sigma_u_pct_err"] = pct
            checks["sigma_u_pass"] = abs(pct) <= tol * 100.0

    # EWM Steady
    elif is_steady_ewm(rec):
        iec_expected["sigma_u"] = 0.0
        iec_expected["sigma_should_be_zero"] = True
        iec_expected["formula"] = "0 (steady)"
        checks["sigma_zero_pass"] = (
            sigma_u_drv is not None and sigma_u_drv < NEGLIGIBLE_SIGMA
        )

    # Deterministic events (EOG, EDC, ECD, EWS)
    elif is_deterministic(wm):
        iec_expected["sigma_u"] = 0.0
        iec_expected["sigma_should_be_zero"] = True
        iec_expected["formula"] = "0 (deterministic event)"
        checks["sigma_zero_pass"] = (
            sigma_u_drv is not None and sigma_u_drv < NEGLIGIBLE_SIGMA
        )

    # UNIFORM
    elif wm == 8:
        iec_expected["sigma_u"] = 0.0
        iec_expected["sigma_should_be_zero"] = True
        iec_expected["formula"] = "0 (uniform)"
        checks["sigma_zero_pass"] = (
            sigma_u_drv is not None and sigma_u_drv < NEGLIGIBLE_SIGMA
        )

    return {"iec_expected": iec_expected, "checks": checks}


# ────────────────────────────────── plots ────────────────────────────────────

def figure_1_ntm_ti(cases: list[dict]) -> str | None:
    """NTM turbulence intensity vs wind speed."""
    if plt is None:
        return None
    ntm = [c for c in cases
           if c.get("input", {}).get("WindModel") == 0 and c.get("stats", {}).get("u")]
    if not ntm:
        return None

    speeds: list[float] = []
    ti_vals: list[float] = []
    for c in ntm:
        v = c["input"]["MeanWindSpeed"]
        ti = c["stats"]["u"]["ti"]
        speeds.append(v)
        ti_vals.append(ti)

    speeds_arr = np.array(speeds)
    ti_arr = np.array(ti_vals)
    v_range = np.linspace(max(1.0, speeds_arr.min() - 1), speeds_arr.max() + 1, 200)

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.scatter(speeds_arr, ti_arr, c="#1f77b4", s=30, alpha=0.7, label="Generated u.TI (6 seeds/speed)")

    # IEC NTM curve
    iec_ti = np.array([iec_ntm_ti(v) for v in v_range])
    ax.plot(v_range, iec_ti, "r-", linewidth=2, label="IEC NTM TI (Iref=0.14 Class B)")

    # ±10% tolerance band
    ax.plot(v_range, iec_ti * 1.10, "r--", linewidth=1, alpha=0.6, label="+10% / -10% tolerance")
    ax.plot(v_range, iec_ti * 0.90, "r--", linewidth=1, alpha=0.6)

    ax.set_xlabel("Mean Wind Speed [m/s]")
    ax.set_ylabel("Turbulence Intensity [%]")
    ax.set_title("Figure 1: NTM Turbulence Intensity Validation")
    ax.legend(fontsize=8, loc="upper right")
    ax.grid(True, alpha=0.3)
    ax.set_xlim(left=0)

    path = PLOT_DIR / "fig1_ntm_ti_validation.png"
    PLOT_DIR.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


def figure_2_all_models_ti(cases: list[dict]) -> str | None:
    """Grouped bar chart: generated vs IEC expected TI."""
    if plt is None:
        return None

    groups_order = [
        ("NTM", 0, [4, 6, 8, 10, 11.4, 12, 14, 16, 18, 20, 22, 24]),
        ("ETM", 1, [4, 6, 8, 10, 11.4, 12, 14, 16, 18, 20, 22, 24]),
        ("EWM1_Turb", 2, [11.4, 25]),
        ("EWM50_Turb", 3, [11.4, 25]),
    ]

    group_labels: list[str] = []
    gen_means: list[float] = []
    gen_stds: list[float] = []
    iec_vals: list[float] = []

    for label, wm, speeds in groups_order:
        for sp in speeds:
            subset = [c for c in cases
                      if c.get("input", {}).get("WindModel") == wm
                      and abs(c.get("input", {}).get("MeanWindSpeed", 0) - sp) < 0.01
                      and c.get("stats", {}).get("u") is not None]
            # For EWM, exclude steady cases
            if wm in (2, 3):
                subset = [c for c in subset if not is_steady_ewm(c)]

            if not subset:
                continue

            ti_vals = [c["stats"]["u"]["ti"] for c in subset]
            v_hub = subset[0]["input"]["MeanWindSpeed"]
            gm = float(np.mean(ti_vals))
            gs = float(np.std(ti_vals)) if len(ti_vals) > 1 else 0.0

            if wm == 0:
                iec_ti = iec_ntm_ti(v_hub)
            else:
                iec_ti = iec_etm_ti(v_hub) if wm == 1 else (
                    100.0 * iec_ewm_sigma_u(1 if wm == 2 else 50) / v_hub
                )

            group_labels.append(f"{label}_U{sp}")
            gen_means.append(gm)
            gen_stds.append(gs)
            iec_vals.append(iec_ti)

    if not group_labels:
        return None

    x = np.arange(len(group_labels))
    width = 0.35

    fig, ax = plt.subplots(figsize=(18, 7))
    ax.bar(x - width / 2, gen_means, width, yerr=gen_stds,
           label="Generated u.TI (mean±std over 6 seeds)", color="#1f77b4",
           capsize=3, error_kw={"elinewidth": 0.8})
    ax.bar(x + width / 2, iec_vals, width,
           label="IEC Expected TI", color="#d62728", alpha=0.7)

    ax.set_xlabel("Case Group")
    ax.set_ylabel("Turbulence Intensity [%]")
    ax.set_title("Figure 2: Generated vs IEC Expected Turbulence Intensity")
    ax.set_xticks(x)
    ax.set_xticklabels(group_labels, rotation=45, ha="right", fontsize=7)
    ax.legend(fontsize=8)
    ax.grid(True, axis="y", alpha=0.3)

    path = PLOT_DIR / "fig2_all_models_ti.png"
    PLOT_DIR.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


def figure_3_ntm_ratios(cases: list[dict]) -> str | None:
    """σ_v/σ_u and σ_w/σ_u ratios for NTM cases."""
    if plt is None:
        return None
    ntm = [c for c in cases
           if c.get("input", {}).get("WindModel") == 0 and c.get("stats", {}).get("u")]
    if not ntm:
        return None

    speeds: list[float] = []
    vr: list[float] = []
    wr: list[float] = []
    for c in ntm:
        st = c["stats"]
        su = st["u"]["sigma"]
        sv = st["v"]["sigma"]
        sw = st["w"]["sigma"]
        v_hub = c["input"]["MeanWindSpeed"]
        if su > 0:
            speeds.append(v_hub)
            vr.append(sv / su)
            wr.append(sw / su)

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.scatter(speeds, vr, c="#ff7f0e", s=25, alpha=0.7, marker="s", label="σ_v / σ_u")
    ax.scatter(speeds, wr, c="#2ca02c", s=25, alpha=0.7, marker="^", label="σ_w / σ_u")

    ax.axhline(y=0.7, color="#ff7f0e", linestyle="--", linewidth=1.5, alpha=0.6, label="IEC min: σ_v/σ_u ≥ 0.7")
    ax.axhline(y=0.5, color="#2ca02c", linestyle="--", linewidth=1.5, alpha=0.6, label="IEC min: σ_w/σ_u ≥ 0.5")

    ax.set_xlabel("Mean Wind Speed [m/s]")
    ax.set_ylabel("Ratio")
    ax.set_title("Figure 3: Turbulence Standard Deviation Ratios (NTM)")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)
    ax.set_ylim(bottom=0)

    path = PLOT_DIR / "fig3_ntm_sigma_ratios.png"
    PLOT_DIR.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


def figure_4_dlc_compliance(cases: list[dict]) -> str | None:
    """Horizontal bar chart of pass rates per DLC group."""
    if plt is None:
        return None

    # Build DLC groups with validation results
    dlc_groups: dict[str, list[dict]] = defaultdict(list)
    for c in cases:
        v = c.get("_validation", {}).get("checks", {})
        dlc_groups.setdefault("all", []).append(v)

        wm = c.get("input", {}).get("WindModel")
        if wm == 0:
            if is_steady_ewm(c):
                dlc_groups["EWM Steady"].append(v)
            else:
                dlc_groups["NTM"].append(v)
        elif wm == 1:
            dlc_groups["ETM"].append(v)
        elif wm in (2, 3):
            if is_steady_ewm(c):
                dlc_groups["EWM Steady"].append(v)
            else:
                dlc_groups["EWM Turbulent"].append(v)
        elif wm == 4:
            dlc_groups["EOG"].append(v)
        elif wm in (5, 6, 7):
            dlc_groups["EDC/ECD/EWS"].append(v)
        elif wm == 8:
            dlc_groups["UNIFORM"].append(v)

    # Compute pass rates
    group_names: list[str] = []
    pass_rates: list[float] = []
    total_counts: list[int] = []

    for gname in ["NTM", "ETM", "EWM Turbulent", "EWM Steady",
                   "EOG", "EDC/ECD/EWS", "UNIFORM"]:
        grp = dlc_groups.get(gname, [])
        if not grp:
            continue

        n = len(grp)
        # Determine pass criterion per group
        if gname == "NTM" or gname == "ETM":
            n_pass = sum(
                1 for v in grp
                if v.get("sigma_u_pass") is True and v.get("sigma_v_pass") is True and v.get("sigma_w_pass") is True
            )
        elif gname == "EWM Turbulent":
            n_pass = sum(1 for v in grp if v.get("sigma_u_pass") is True)
        elif gname in ("EWM Steady", "EOG", "EDC/ECD/EWS", "UNIFORM"):
            n_pass = sum(1 for v in grp if v.get("sigma_zero_pass") is True)
        else:
            n_pass = 0

        rate = 100.0 * n_pass / n if n > 0 else 0.0
        group_names.append(gname)
        pass_rates.append(rate)
        total_counts.append(n)

    if not group_names:
        return None

    # Plot horizontal bar chart
    colors = []
    for r in pass_rates:
        if r >= 95:
            colors.append("#2ca02c")
        elif r >= 80:
            colors.append("#ff7f0e")
        else:
            colors.append("#d62728")

    fig, ax = plt.subplots(figsize=(10, 6))
    y_pos = np.arange(len(group_names))
    bars = ax.barh(y_pos, pass_rates, height=0.6, color=colors, edgecolor="white")

    for i, (rate, cnt) in enumerate(zip(pass_rates, total_counts)):
        ax.text(rate + 1, i, f"{rate:.1f}% ({cnt} cases)", va="center", fontsize=9)

    ax.set_yticks(y_pos)
    ax.set_yticklabels(group_names)
    ax.set_xlabel("Pass Rate [%]")
    ax.set_title("Figure 4: IEC Compliance by DLC Group")
    ax.set_xlim(0, 115)
    ax.grid(True, axis="x", alpha=0.3)
    ax.invert_yaxis()

    path = PLOT_DIR / "fig4_dlc_compliance.png"
    PLOT_DIR.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return str(path)


# ──────────────────────────────── text report ────────────────────────────────

def generate_report(cases: list[dict]) -> str:
    """Build the validation report text."""
    lines: list[str] = []
    sep = "=" * 48

    lines.append(sep)
    lines.append("SimWind Batch Output — IEC 61400-1 Validation Report")
    lines.append(sep)
    lines.append(f"Total cases: {len(cases)}")
    lines.append("")

    # Group cases by wind model type
    def grouper(wm_val: int | list[int], exclude_steady: bool = False,
                steady_only: bool = False):
        result: list[dict] = []
        if isinstance(wm_val, int):
            wm_val = [wm_val]
        for c in cases:
            wm = c.get("input", {}).get("WindModel")
            if wm not in wm_val:
                continue
            if steady_only and not is_steady_ewm(c):
                continue
            if exclude_steady and is_steady_ewm(c):
                continue
            result.append(c)
        return result

    # ── NTM ──
    ntm_cases = grouper(0)
    if ntm_cases:
        su_pass = sum(1 for c in ntm_cases
                      if c.get("_validation", {}).get("checks", {}).get("sigma_u_pass") is True)
        sv_pass = sum(1 for c in ntm_cases
                      if c.get("_validation", {}).get("checks", {}).get("sigma_v_pass") is True)
        sw_pass = sum(1 for c in ntm_cases
                      if c.get("_validation", {}).get("checks", {}).get("sigma_w_pass") is True)
        ti_errs = [c.get("_validation", {}).get("checks", {}).get("sigma_u_pct_err")
                   for c in ntm_cases
                   if c.get("_validation", {}).get("checks", {}).get("sigma_u_pct_err") is not None]
        mean_ti_err = float(np.mean(np.abs(ti_errs))) if ti_errs else 0.0

        n = len(ntm_cases)
        lines.append("--- NTM Cases ---")
        lines.append(f"Count: {n}")
        lines.append(f"σ_u within ±10% of expected: {su_pass}/{n} ({100.0*su_pass/n:.1f}%)")
        lines.append(f"σ_v ≥ 0.7·σ_u: {sv_pass}/{n} ({100.0*sv_pass/n:.1f}%)")
        lines.append(f"σ_w ≥ 0.5·σ_u: {sw_pass}/{n} ({100.0*sw_pass/n:.1f}%)")
        lines.append(f"Mean |TI error|: {mean_ti_err:.2f}%")
        lines.append("")

    # ── ETM ──
    etm_cases = grouper(1)
    if etm_cases:
        su_pass = sum(1 for c in etm_cases
                      if c.get("_validation", {}).get("checks", {}).get("sigma_u_pass") is True)
        sv_pass = sum(1 for c in etm_cases
                      if c.get("_validation", {}).get("checks", {}).get("sigma_v_pass") is True)
        sw_pass = sum(1 for c in etm_cases
                      if c.get("_validation", {}).get("checks", {}).get("sigma_w_pass") is True)
        ti_errs = [c.get("_validation", {}).get("checks", {}).get("sigma_u_pct_err")
                   for c in etm_cases
                   if c.get("_validation", {}).get("checks", {}).get("sigma_u_pct_err") is not None]
        mean_ti_err = float(np.mean(np.abs(ti_errs))) if ti_errs else 0.0

        n = len(etm_cases)
        lines.append("--- ETM Cases ---")
        lines.append(f"Count: {n}")
        lines.append(f"σ_u within ±10% of expected: {su_pass}/{n} ({100.0*su_pass/n:.1f}%)")
        lines.append(f"σ_v ≥ 0.7·σ_u: {sv_pass}/{n} ({100.0*sv_pass/n:.1f}%)")
        lines.append(f"σ_w ≥ 0.5·σ_u: {sw_pass}/{n} ({100.0*sw_pass/n:.1f}%)")
        lines.append(f"Mean |TI error|: {mean_ti_err:.2f}%")
        lines.append("")

    # ── EOG ──
    eog_cases = grouper(4)
    if eog_cases:
        sp = sum(1 for c in eog_cases
                 if c.get("_validation", {}).get("checks", {}).get("sigma_zero_pass") is True)
        n = len(eog_cases)
        lines.append("--- EOG Cases ---")
        lines.append(f"Count: {n}")
        lines.append(f"σ = 0 (deterministic): {sp}/{n} ({100.0*sp/n:.1f}%)")
        lines.append("")

    # ── EDC/ECD/EWS ──
    det_cases = grouper([5, 6, 7])
    if det_cases:
        sp = sum(1 for c in det_cases
                 if c.get("_validation", {}).get("checks", {}).get("sigma_zero_pass") is True)
        n = len(det_cases)
        lines.append("--- EDC/ECD/EWS Cases ---")
        lines.append(f"Count: {n}")
        lines.append(f"σ = 0 (deterministic): {sp}/{n} ({100.0*sp/n:.1f}%)")
        lines.append("")

    # ── EWM Turbulent ──
    ewm_turb = grouper([2, 3], exclude_steady=False)
    ewm_turb = [c for c in ewm_turb if not is_steady_ewm(c)]
    if ewm_turb:
        su_pass = sum(1 for c in ewm_turb
                      if c.get("_validation", {}).get("checks", {}).get("sigma_u_pass") is True)
        n = len(ewm_turb)
        lines.append("--- EWM Turbulent Cases ---")
        lines.append(f"Count: {n}")
        lines.append(f"σ_u within ±10% of expected: {su_pass}/{n} ({100.0*su_pass/n:.1f}%)")
        lines.append("")

    # ── EWM Steady ──
    ewm_steady = grouper([2, 3], steady_only=True)
    if ewm_steady:
        sp = sum(1 for c in ewm_steady
                 if c.get("_validation", {}).get("checks", {}).get("sigma_zero_pass") is True)
        n = len(ewm_steady)
        lines.append("--- EWM Steady Cases ---")
        lines.append(f"Count: {n}")
        lines.append(f"σ = 0 (steady): {sp}/{n} ({100.0*sp/n:.1f}%)")
        lines.append("")

    # ── UNIFORM ──
    uni_cases = grouper(8)
    if uni_cases:
        sp = sum(1 for c in uni_cases
                 if c.get("_validation", {}).get("checks", {}).get("sigma_zero_pass") is True)
        n = len(uni_cases)
        lines.append("--- UNIFORM Cases ---")
        lines.append(f"Count: {n}")
        lines.append(f"σ = 0 (uniform): {sp}/{n} ({100.0*sp/n:.1f}%)")
        lines.append("")

    lines.append(sep)

    text = "\n".join(lines)
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(text, encoding="utf-8")
    return str(REPORT_PATH)


# ────────────────────────────────── main ────────────────────────────────────

def main() -> int:
    # 1. Discover all .sum files
    sum_paths = sorted(OUTPUT_DIR.rglob("*.sum"))
    print(f"Found {len(sum_paths)} .sum files in {OUTPUT_DIR}")

    # 2. Parse and validate
    cases: list[dict] = []
    parse_failures = 0
    for sp in sum_paths:
        rec = parse_sum_file(sp)
        if rec is None:
            parse_failures += 1
            continue
        # Run validation
        rec["_validation"] = validate_case(rec)
        cases.append(rec)

    print(f"Parsed {len(cases)} cases ({parse_failures} failures)")
    if not cases:
        print("ERROR: No valid .sum files found.")
        return 1

    # 3. Quick sanity print of first and last case
    first = cases[0]
    print(f"\n  Example: {first['case_name']}  WindModel={first['input'].get('WindModel')}"
          f"  Speed={first['input'].get('MeanWindSpeed')} m/s"
          f"  SigmaU(derived)={first['derived'].get('SigmaU')}")

    # 4. Generate plots
    print("\nGenerating validation plots...")
    fig_paths = []
    for fn in (figure_1_ntm_ti, figure_2_all_models_ti,
               figure_3_ntm_ratios, figure_4_dlc_compliance):
        p = fn(cases)
        if p:
            fig_paths.append(p)
            print(f"  -> {p}")
        else:
            print(f"  (skipped {fn.__name__})")

    # 5. Generate text report
    report = generate_report(cases)
    print(f"\nReport: {report}")

    # 6. Terminal summary
    print("\n" + "=" * 48)
    print("  VALIDATION SUMMARY")
    print("=" * 48)

    def rate(num: float, den: float) -> str:
        return f"{100.0*num/den:.1f}%" if den > 0 else "N/A"

    for label, wm_vals, criterion in [
        ("NTM               ", [0], "sigma_u"),
        ("ETM               ", [1], "sigma_u"),
        ("EWM Turbulent     ", [2, 3], "sigma_u_turb"),
        ("EWM Steady        ", [2, 3], "sigma_zero"),
        ("EOG               ", [4], "sigma_zero"),
        ("EDC/ECD/EWS       ", [5, 6, 7], "sigma_zero"),
        ("UNIFORM           ", [8], "sigma_zero"),
    ]:
        subset = [c for c in cases if c.get("input", {}).get("WindModel") in wm_vals]
        if criterion == "sigma_u_turb":
            subset = [c for c in subset if not is_steady_ewm(c)]
            n = len(subset)
            n_pass = sum(1 for c in subset if c.get("_validation", {}).get("checks", {}).get("sigma_u_pass") is True)
        elif criterion == "sigma_zero":
            if wm_vals == [2, 3]:
                subset = [c for c in subset if is_steady_ewm(c)]
            n = len(subset)
            n_pass = sum(1 for c in subset if c.get("_validation", {}).get("checks", {}).get("sigma_zero_pass") is True)
        elif criterion == "sigma_u":
            n = len(subset)
            n_pass = sum(1 for c in subset
                         if c.get("_validation", {}).get("checks", {}).get("sigma_u_pass") is True
                         and c.get("_validation", {}).get("checks", {}).get("sigma_v_pass") is True
                         and c.get("_validation", {}).get("checks", {}).get("sigma_w_pass") is True)
        else:
            n = len(subset)
            n_pass = 0

        print(f"  {label}: {n_pass:>3}/{n:<3}  {rate(n_pass, n)}")

    print("=" * 48)
    print(f"\nPlots saved to: {PLOT_DIR}")
    print(f"Report saved to: {REPORT_PATH}")
    print(f"Total cases validated: {len(cases)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
