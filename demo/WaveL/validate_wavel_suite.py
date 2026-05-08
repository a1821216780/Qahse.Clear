from __future__ import annotations

import argparse
import json
import math
import statistics
import struct
import subprocess
from dataclasses import dataclass
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


K_PI = math.pi
K_GRAVITY = 9.80665
K_DENSITY_WATER = 1025.0
K_DEEP_WATER_THRESHOLD = 20.0
K_MIN_POSITIVE = 1.0e-12


@dataclass(frozen=True)
class CaseSpec:
    name: str
    qoe: str
    kind: str
    dependency: str | None = None


CASE_SPECS = [
    CaseSpec("Qahse_WaveL_Main_DEMO", "Qahse_WaveL_Main_DEMO.qoe", "spectrum"),
    CaseSpec("Qahse_WaveL_PiersonMoskowitz_DEMO", "Qahse_WaveL_PiersonMoskowitz_DEMO.qoe", "spectrum"),
    CaseSpec("Qahse_WaveL_OchiHubble_DEMO", "Qahse_WaveL_OchiHubble_DEMO.qoe", "spectrum"),
    CaseSpec("Qahse_WaveL_Torsethaugen_DEMO", "Qahse_WaveL_Torsethaugen_DEMO.qoe", "spectrum"),
    CaseSpec("Qahse_WaveL_User_Spectrum_DEMO", "Qahse_WaveL_User_Spectrum_DEMO.qoe", "user_spectrum"),
    CaseSpec("Qahse_WaveL_User_TimeSeries_DEMO", "Qahse_WaveL_User_TimeSeries_DEMO.qoe", "user_timeseries"),
    CaseSpec("Qahse_WaveL_Regular_Kinematics_DEMO", "Qahse_WaveL_Regular_Kinematics_DEMO.qoe", "regular_kinematics"),
    CaseSpec("Qahse_WaveL_DenseGrid_Regular_DEMO", "Qahse_WaveL_DenseGrid_Regular_DEMO.qoe", "regular_dense_grid"),
    CaseSpec("Qahse_WaveL_Import_DEMO", "Qahse_WaveL_Import_DEMO.qoe", "import_cache", dependency="Qahse_WaveL_Main_DEMO"),
]


def parse_value(raw: str):
    text = raw.strip().strip('"')
    lower = text.lower()
    if lower == "true":
        return True
    if lower == "false":
        return False
    try:
        if any(ch in text for ch in ".eE"):
            return float(text)
        return int(text)
    except ValueError:
        return text


def parse_qoe(path: Path) -> dict[str, object]:
    config: dict[str, object] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("#") or line.startswith("--") or line.startswith("----------------------"):
            continue
        parts = raw.split("\t")
        if len(parts) < 2:
            parts = line.split()
        if len(parts) < 2:
            continue
        config[parts[1].strip()] = parse_value(parts[0])
    return config


def parse_summary(path: Path) -> dict[str, object]:
    summary: dict[str, object] = {}
    warnings: list[str] = []
    in_warnings = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.rstrip()
        if not line:
            continue
        if line.startswith("Warnings:"):
            in_warnings = True
            continue
        if in_warnings and line.startswith("  - "):
            warnings.append(line[4:].strip())
            continue
        in_warnings = False
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        value = value.strip()
        token = value.split()[0] if value else ""
        try:
            if any(ch in token for ch in ".eE"):
                summary[key.strip()] = float(token)
            else:
                summary[key.strip()] = int(token)
        except ValueError:
            summary[key.strip()] = value
    if warnings:
        summary["Warnings"] = warnings
    return summary


def read_wts(path: Path) -> tuple[list[float], list[float]]:
    times: list[float] = []
    eta: list[float] = []
    in_block = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.upper() == "!BEGIN":
            in_block = True
            continue
        if not in_block:
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        times.append(float(parts[0]))
        eta.append(float(parts[1]))
    return times, eta


def read_wvc(path: Path):
    header: dict[str, float] = {}
    freqs: list[float] = []
    amps: list[float] = []
    phases: list[float] = []
    dirs: list[float] = []
    ks: list[float] = []
    in_block = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.upper() == "!BEGIN":
            in_block = True
            continue
        if not in_block:
            parts = line.split()
            if len(parts) >= 2:
                try:
                    header[parts[1]] = float(parts[0])
                except ValueError:
                    pass
            continue
        parts = line.split()
        if len(parts) < 5:
            continue
        freqs.append(float(parts[0]))
        amps.append(float(parts[1]))
        phases.append(float(parts[2]))
        dirs.append(float(parts[3]))
        ks.append(float(parts[4]))
    return freqs, amps, phases, dirs, ks, header


def read_user_spectrum(path: Path) -> tuple[list[float], list[float]]:
    return read_wts_like_pairs(path)


def read_wts_like_pairs(path: Path) -> tuple[list[float], list[float]]:
    xs: list[float] = []
    ys: list[float] = []
    in_block = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.upper() == "!BEGIN":
            in_block = True
            continue
        if not in_block:
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        xs.append(float(parts[0]))
        ys.append(float(parts[1]))
    return xs, ys


def infer_uniform_dt(times: list[float]) -> float:
    if len(times) < 2:
        return 0.0
    return (times[-1] - times[0]) / max(len(times) - 1, 1)


def read_wfc(path: Path) -> dict[str, object]:
    with path.open("rb") as stream:
        blob = stream.read()
    offset = 0
    magic = blob[offset:offset + 8]
    offset += 8
    if magic != b"QWFC0001":
        raise RuntimeError(f"Invalid wfc header: {path}")

    def unpack(fmt: str):
        nonlocal offset
        size = struct.calcsize(fmt)
        values = struct.unpack_from(fmt, blob, offset)
        offset += size
        return values if len(values) > 1 else values[0]

    version = unpack("<I")
    flags = unpack("<I")
    nx = unpack("<i")
    ny = unpack("<i")
    nz = unpack("<i")
    nt = unpack("<i")
    dx = unpack("<d")
    dy = unpack("<d")
    dz = unpack("<d")
    dt = unpack("<d")
    z_bottom = unpack("<d")
    water_depth = unpack("<d")
    hs = unpack("<d")
    tp = unpack("<d")
    fp = unpack("<d")
    m0 = unpack("<d")
    f_min = unpack("<d")
    f_max = unpack("<d")
    spectral_area = unpack("<d")
    wave_st_mod = unpack("<i")
    num_components = unpack("<i")

    total = nx * ny * nz * nt

    def read_array() -> list[float]:
        nonlocal offset
        size = 8 * total
        values = list(struct.unpack_from(f"<{total}d", blob, offset))
        offset += size
        return values

    return {
        "version": version,
        "flags": flags,
        "nx": nx,
        "ny": ny,
        "nz": nz,
        "nt": nt,
        "dx": dx,
        "dy": dy,
        "dz": dz,
        "dt": dt,
        "z_bottom": z_bottom,
        "water_depth": water_depth,
        "hs": hs,
        "tp": tp,
        "fp": fp,
        "m0": m0,
        "f_min": f_min,
        "f_max": f_max,
        "spectral_area": spectral_area,
        "wave_st_mod": wave_st_mod,
        "num_components": num_components,
        "eta": read_array(),
        "u": read_array(),
        "v": read_array(),
        "w": read_array(),
        "ax": read_array(),
        "ay": read_array(),
        "az": read_array(),
        "dynP": read_array(),
    }


def cache_index(cache: dict[str, object], ix: int, iy: int, iz: int, it: int) -> int:
    return (((it * cache["nz"] + iz) * cache["ny"] + iy) * cache["nx"] + ix)


def integrate_trapezoid(xs: list[float], ys: list[float]) -> float:
    if len(xs) < 2:
        return 0.0
    area = 0.0
    for i in range(1, len(xs)):
        area += 0.5 * (ys[i - 1] + ys[i]) * (xs[i] - xs[i - 1])
    return area


def interpolate_linear(x: float, xs: list[float], ys: list[float]) -> float:
    if not xs:
        return 0.0
    if x <= xs[0]:
        return ys[0]
    if x >= xs[-1]:
        return ys[-1]
    lo = 0
    hi = len(xs) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if xs[mid] <= x:
            lo = mid
        else:
            hi = mid
    w = (x - xs[lo]) / (xs[hi] - xs[lo])
    return ys[lo] + (ys[hi] - ys[lo]) * w


def rmse(a: list[float], b: list[float]) -> float:
    if len(a) != len(b) or not a:
        return float("inf")
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)) / len(a))


def demean(values: list[float]) -> list[float]:
    if not values:
        return []
    mean = sum(values) / len(values)
    return [v - mean for v in values]


def stddev(values: list[float]) -> float:
    return statistics.pstdev(values) if len(values) >= 2 else 0.0


def target_zero_moment(cfg: dict[str, object]) -> float:
    if str(cfg.get("FreqSpectrum", "")) == "OCHI_HUBBLE":
        hs1 = float(cfg.get("Hs1", 0.0))
        hs2 = float(cfg.get("Hs2", 0.0))
        return (hs1 * hs1 + hs2 * hs2) / 16.0
    hs = float(cfg.get("Hs", 0.0))
    return hs * hs / 16.0 if hs > 0.0 else 0.0


def eval_jonswap(f: float, cfg: dict[str, object]) -> float:
    hs = float(cfg["Hs"])
    tp = float(cfg["Tp"])
    if f <= 0.0 or hs <= 0.0 or tp <= 0.0:
        return 0.0
    fp = 1.0 / tp
    gamma = float(cfg.get("Gamma", 0.0))
    if bool(cfg.get("AutoGamma", True)):
        ratio = tp / math.sqrt(hs)
        if ratio <= 3.6:
            gamma = 5.0
        elif ratio <= 5.0:
            gamma = math.exp(5.75 - 1.15 * ratio)
        else:
            gamma = 1.0
    sigma = 0.07 if f <= fp else 0.09
    if not bool(cfg.get("AutoSigma", True)):
        sigma = float(cfg.get("Sigma1", 0.0)) if f <= fp else float(cfg.get("Sigma2", 0.0))
    fr = f / fp
    gamma = max(gamma, 1.0)
    return (
        0.3125
        * hs
        * hs
        * tp
        * pow(fr, -5.0)
        * math.exp(-1.25 * pow(fr, -4.0))
        * (1.0 - 0.287 * math.log(gamma))
        * pow(gamma, math.exp(-0.5 * ((fr - 1.0) / sigma) ** 2))
    )


def eval_torsethaugen(f: float, cfg: dict[str, object]) -> float:
    hs = float(cfg["Hs"])
    tp = float(cfg["Tp"])
    if f <= 0.0 or hs <= 0.0 or tp <= 0.0:
        return 0.0
    a_f = 6.6
    a_e = 2.0
    a_u = 25.0
    a_10 = 0.7
    a_1 = 0.5
    k_g = 35.0
    b_1 = 2.0
    a_20 = 0.6
    a_2 = 0.3
    a_3 = 6.0
    tpf = a_f * pow(hs, 1.0 / 3.0)
    tl = a_e * math.sqrt(hs)
    tu = a_u
    e_l = max(0.0, min(1.0, (tpf - tp) / max(tpf - tl, K_MIN_POSITIVE)))
    e_u = max(0.0, min(1.0, (tp - tpf) / max(tu - tpf, K_MIN_POSITIVE)))
    if tp <= tpf:
        r = (1.0 - a_10) * math.exp(-((e_l / a_1) ** 2)) + a_10
        h1 = r * hs
        tp1 = tp
        steepness = 2.0 * K_PI / K_GRAVITY * h1 / (tp1 * tp1)
        gamma1 = k_g * pow(steepness, 6.0 / 7.0)
        h2 = math.sqrt(max(0.0, 1.0 - r * r)) * hs
        tp2 = tpf + b_1
    else:
        r = (1.0 - a_20) * math.exp(-((e_u / a_2) ** 2)) + a_20
        h1 = r * hs
        tp1 = tp
        steepness = 2.0 * K_PI / K_GRAVITY * hs / (tpf * tpf)
        gamma1 = k_g * pow(steepness, 6.0 / 7.0) * (1.0 + a_3 * e_u)
        h2 = math.sqrt(max(0.0, 1.0 - r * r)) * hs
        tp2 = a_f * pow(max(h2, K_MIN_POSITIVE), 1.0 / 3.0)
    g0 = 3.26
    ay = (1.0 + 1.1 * pow(math.log(max(gamma1, 1.0)), 1.19)) / max(gamma1, 1.0)
    f1n = f * tp1
    f2n = f * tp2
    s1 = g0 * ay * pow(f1n, -4.0) * math.exp(-pow(f1n, -4.0)) * pow(
        max(gamma1, 1.0), math.exp(-((f1n - 1.0) ** 2) / (2.0 * 0.09 * 0.09))
    )
    s2 = g0 * pow(f2n, -4.0) * math.exp(-pow(f2n, -4.0))
    return s1 + s2 if bool(cfg.get("DoublePeak", False)) else s1


def eval_ochi_hubble(f: float, cfg: dict[str, object]) -> float:
    if f <= 0.0:
        return 0.0
    hs1 = float(cfg["Hs1"])
    hs2 = float(cfg["Hs2"])
    f1 = float(cfg["F1"])
    f2 = float(cfg["F2"])
    lam1 = float(cfg["Lambda1"])
    lam2 = float(cfg["Lambda2"])
    w1 = f1 * 2.0 * K_PI
    w2 = f2 * 2.0 * K_PI
    w = f * 2.0 * K_PI
    s1 = (
        0.25
        * pow(((4.0 * lam1 + 1.0) / 4.0) * pow(w1, 4.0), lam1)
        / math.gamma(lam1)
        * hs1
        * hs1
        / pow(w, 4.0 * lam1 + 1.0)
        * math.exp(-((4.0 * lam1 + 1.0) / 4.0) * pow(w1 / w, 4.0))
    )
    s2 = (
        0.25
        * pow(((4.0 * lam2 + 1.0) / 4.0) * pow(w2, 4.0), lam2)
        / math.gamma(lam2)
        * hs2
        * hs2
        / pow(w, 4.0 * lam2 + 1.0)
        * math.exp(-((4.0 * lam2 + 1.0) / 4.0) * pow(w2 / w, 4.0))
    )
    return s1 + s2


def build_theory_curve(cfg: dict[str, object], kind: str) -> tuple[list[float], list[float]]:
    if kind == "user_spectrum":
        freqs, values = read_user_spectrum(Path(cfg["ImportedSpectrumPath"]))
        target = target_zero_moment(cfg)
        raw_area = integrate_trapezoid(freqs, values)
        if raw_area > K_MIN_POSITIVE and target > 0.0:
            scale = target / raw_area
            values = [value * scale for value in values]
        return freqs, values

    tp = float(cfg.get("Tp", 10.0))
    f_min = float(cfg.get("FCutIn", 0.0)) or 0.5 / tp
    f_max = float(cfg.get("FCutOut", 0.0)) or 10.0 / tp
    freqs = [f_min + (f_max - f_min) * i / 1023.0 for i in range(1024)]
    model = str(cfg["FreqSpectrum"])
    if model == "JONSWAP":
        values = [eval_jonswap(f, cfg) for f in freqs]
    elif model == "TORSETHAUGEN":
        values = [eval_torsethaugen(f, cfg) for f in freqs]
    elif model == "OCHI_HUBBLE":
        values = [eval_ochi_hubble(f, cfg) for f in freqs]
    else:
        values = [0.0 for _ in freqs]
    raw_area = integrate_trapezoid(freqs, values)
    target = target_zero_moment(cfg)
    if raw_area > K_MIN_POSITIVE and target > 0.0:
        scale = target / raw_area
        values = [value * scale for value in values]
    return freqs, values


def solve_wave_number(omega: float, depth: float) -> float:
    if omega <= 0.0 or depth <= 0.0:
        return 0.0
    deep = omega * omega / K_GRAVITY
    if depth * deep > K_DEEP_WATER_THRESHOLD:
        return deep
    seed = deep * (1.0 - math.exp(-pow(omega * math.sqrt(depth / K_GRAVITY), 2.5))) ** -0.4
    k = max(seed, deep * 0.5)
    for _ in range(32):
        kd = k * depth
        tanh_kd = math.tanh(kd)
        sech2 = 1.0 / math.cosh(kd) ** 2
        f = K_GRAVITY * k * tanh_kd - omega * omega
        df = K_GRAVITY * (tanh_kd + kd * sech2)
        delta = f / max(df, K_MIN_POSITIVE)
        k -= delta
        if abs(delta) < 1.0e-12 * max(1.0, k):
            break
    return k


def qoe_defaults() -> dict[str, object]:
    return {
        "AutoGamma": True,
        "AutoSigma": True,
        "AutoFreqRange": True,
        "Hs1": 2.0,
        "Hs2": 1.5,
        "F1": 0.10,
        "F2": 0.20,
        "Lambda1": 2.0,
        "Lambda2": 2.0,
        "DoublePeak": False,
        "RegularPhase": 0.0,
        "GridNX": 1,
        "GridNY": 1,
        "GridNZ": 12,
        "GridDX": 0.0,
        "GridDY": 0.0,
        "GridDZ": 2.0,
    }


def parse_stretching(value: object) -> str:
    return str(value).strip().upper()


def wrap_time(time: float, duration: float) -> float:
    if duration <= 0.0:
        return time
    wrapped = math.fmod(time, duration)
    if wrapped < 0.0:
        wrapped += duration
    return wrapped


def plot_spectrum_case(case_name: str, out_dir: Path, times: list[float], eta: list[float], theory_f: list[float], theory_s: list[float], comp_f: list[float], comp_s: list[float]) -> str:
    fig, axes = plt.subplots(2, 1, figsize=(10, 8), constrained_layout=True)
    axes[0].plot(times, eta, color="#1f77b4", linewidth=1.0)
    axes[0].set_title(f"{case_name}: eta(t)")
    axes[0].set_xlabel("t (s)")
    axes[0].set_ylabel("eta (m)")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(theory_f, theory_s, color="#2ca02c", linewidth=1.5, label="Theory")
    if comp_f:
        axes[1].stem(comp_f, comp_s, linefmt="#d62728", markerfmt="ro", basefmt=" ", label="Discrete")
    axes[1].set_title(f"{case_name}: spectrum")
    axes[1].set_xlabel("f (Hz)")
    axes[1].set_ylabel("S(f) or E/df")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend()

    out = out_dir / f"{case_name}_validation.png"
    fig.savefig(out, dpi=180)
    plt.close(fig)
    return str(out)


def plot_user_timeseries_case(
    case_name: str,
    out_dir: Path,
    out_t: list[float],
    out_eta_zero: list[float],
    src_t: list[float],
    src_eta_zero: list[float],
    src_f: list[float],
    src_s: list[float],
    out_f: list[float],
    out_s: list[float],
) -> str:
    fig, axes = plt.subplots(2, 1, figsize=(10, 8), constrained_layout=True)
    axes[0].plot(src_t, src_eta_zero, color="#2ca02c", linewidth=1.2, linestyle="--", label="Source demeaned")
    axes[0].plot(out_t, out_eta_zero, color="#1f77b4", linewidth=1.0, label="Output demeaned")
    axes[0].set_title(f"{case_name}: eta(t)")
    axes[0].set_xlabel("t (s)")
    axes[0].set_ylabel("eta (m)")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()

    axes[1].plot(src_f, src_s, color="#2ca02c", linewidth=1.5, label="Source decomposition")
    if out_f:
        axes[1].stem(out_f, out_s, linefmt="#d62728", markerfmt="ro", basefmt=" ", label="Output reconstruction")
    axes[1].set_title(f"{case_name}: spectrum")
    axes[1].set_xlabel("f (Hz)")
    axes[1].set_ylabel("S(f) or E/df")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend()

    out = out_dir / f"{case_name}_validation.png"
    fig.savefig(out, dpi=180)
    plt.close(fig)
    return str(out)


def plot_regular_kinematics(case_name: str, out_dir: Path, times: list[float], series: dict[str, tuple[list[float], list[float]]], submerged: list[int]) -> str:
    fig, axes = plt.subplots(4, 1, figsize=(10, 12), constrained_layout=True)
    for ax, key, label in zip(
        axes[:3],
        ["eta", "u", "w"],
        ["eta (m)", "u (m/s)", "w (m/s)"],
    ):
        measured, theory = series[key]
        ax.plot(times, measured, label="Cache", color="#1f77b4")
        ax.plot(times, theory, label="Theory", color="#d62728", linestyle="--")
        ax.set_ylabel(label)
        ax.grid(True, alpha=0.3)
        ax.legend()
    measured_dp, theory_dp = series["dynP"]
    axes[3].step(times, submerged, where="mid", color="#2ca02c", label="Submerged")
    axes[3].plot(times, [1.0 if x > 0.0 else 0.0 for x in theory_dp], color="#ff7f0e", linestyle=":", label="Pressure sign")
    axes[3].set_ylabel("0/1")
    axes[3].set_xlabel("t (s)")
    axes[3].grid(True, alpha=0.3)
    axes[3].legend()
    axes[0].set_title(f"{case_name}: kinematics and submergence")
    out = out_dir / f"{case_name}_validation.png"
    fig.savefig(out, dpi=180)
    plt.close(fig)
    return str(out)


def plot_dense_regular_case(
    case_name: str,
    out_dir: Path,
    eta_xy_cache: list[list[float]],
    eta_xy_error: list[list[float]],
    u_xz_cache: list[list[float]],
    u_xz_error: list[list[float]],
    w_xz_cache: list[list[float]],
    w_xz_error: list[list[float]],
    u_yz_cache: list[list[float]],
    u_yz_error: list[list[float]],
    extent_xy: tuple[float, float, float, float],
    extent_xz: tuple[float, float, float, float],
    extent_yz: tuple[float, float, float, float],
) -> str:
    fig, axes = plt.subplots(4, 2, figsize=(12, 15), constrained_layout=True)
    plots = [
        (axes[0, 0], eta_xy_cache, extent_xy, "eta(x,y) cache", "x (m)", "y (m)"),
        (axes[0, 1], eta_xy_error, extent_xy, "|eta error|(x,y)", "x (m)", "y (m)"),
        (axes[1, 0], u_xz_cache, extent_xz, "u(x,z) cache", "x (m)", "z (m)"),
        (axes[1, 1], u_xz_error, extent_xz, "|u error|(x,z)", "x (m)", "z (m)"),
        (axes[2, 0], w_xz_cache, extent_xz, "w(x,z) cache", "x (m)", "z (m)"),
        (axes[2, 1], w_xz_error, extent_xz, "|w error|(x,z)", "x (m)", "z (m)"),
        (axes[3, 0], u_yz_cache, extent_yz, "u(y,z) cache", "y (m)", "z (m)"),
        (axes[3, 1], u_yz_error, extent_yz, "|u error|(y,z)", "y (m)", "z (m)"),
    ]
    for ax, matrix, extent, title, xlabel, ylabel in plots:
        image = ax.imshow(matrix, origin="lower", aspect="auto", extent=extent)
        ax.set_title(title)
        ax.set_xlabel(xlabel)
        ax.set_ylabel(ylabel)
        fig.colorbar(image, ax=ax, shrink=0.85)
    axes[0, 0].figure.suptitle(f"{case_name}: 2D slice validation", fontsize=16)
    out = out_dir / f"{case_name}_validation.png"
    fig.savefig(out, dpi=180)
    plt.close(fig)
    return str(out)


def plot_dense_regular_sequence(
    case_name: str,
    out_dir: Path,
    eta_xy_frames: list[list[list[float]]],
    frame_times: list[float],
    extent_xy: tuple[float, float, float, float],
) -> str:
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), constrained_layout=True)
    flat_axes = axes.flatten()
    vmin = min(min(min(row) for row in frame) for frame in eta_xy_frames)
    vmax = max(max(max(row) for row in frame) for frame in eta_xy_frames)
    last_image = None
    for ax, frame, t in zip(flat_axes, eta_xy_frames, frame_times):
        last_image = ax.imshow(frame, origin="lower", aspect="auto", extent=extent_xy, vmin=vmin, vmax=vmax)
        ax.set_title(f"eta(x,y), t = {t:.2f} s")
        ax.set_xlabel("x (m)")
        ax.set_ylabel("y (m)")
    if last_image is not None:
        fig.colorbar(last_image, ax=flat_axes.tolist(), shrink=0.9)
    fig.suptitle(f"{case_name}: eta(x,y) sequence")
    out = out_dir / f"{case_name}_sequence.png"
    fig.savefig(out, dpi=180)
    plt.close(fig)
    return str(out)


def evaluate_regular_wave_theory(cfg: dict[str, object], x: float, y: float, z: float, t: float) -> tuple[dict[str, float], int]:
    amplitude = 0.5 * float(cfg["Hs"])
    omega = 2.0 * K_PI / float(cfg["Tp"])
    phase0 = float(cfg.get("RegularPhase", 0.0))
    depth = float(cfg["WaterDepth"])
    direction = math.radians(float(cfg.get("DirMean", 0.0)))
    cos_dir = math.cos(direction)
    sin_dir = math.sin(direction)
    local_time = wrap_time(t + float(cfg.get("TimeOffset", 0.0)), float(cfg.get("SimDuration", 0.0)))
    X = x * cos_dir + y * sin_dir
    k = solve_wave_number(omega, depth)
    phase = k * X - omega * local_time + phase0
    eta = amplitude * math.sin(phase)

    result = {"eta": eta, "u": 0.0, "v": 0.0, "w": 0.0, "ax": 0.0, "ay": 0.0, "az": 0.0, "dynP": 0.0}
    submerged = 1 if z <= eta else 0
    stretching = parse_stretching(cfg.get("Stretching", "EXTRAPOLATION"))
    if stretching == "NONE":
        if z > 0.0:
            return result, submerged
    elif z > eta:
        return result, submerged

    if stretching == "WHEELER":
        eval_z = depth * (z - eta) / (eta + depth)
    elif stretching == "VERTICAL":
        eval_z = 0.0 if z > 0.0 else z
    else:
        eval_z = z
    if eval_z + depth < 0.0:
        return result, submerged

    kd = k * depth
    deep_water = kd > K_DEEP_WATER_THRESHOLD or depth > 100.0
    if deep_water:
        depth_var_xy = math.exp(k * eval_z)
        depth_var_z = depth_var_xy
    else:
        sinh_den = math.sinh(kd)
        depth_var_xy = math.cosh(k * (eval_z + depth)) / sinh_den
        depth_var_z = math.sinh(k * (eval_z + depth)) / sinh_den
        if stretching == "EXTRAPOLATION" and z > 0.0:
            z0xy = math.cosh(kd) / sinh_den
            z0z = 1.0
            dxy = k
            dz_linear = k * z0xy
            depth_var_xy = z0xy + z * dxy
            depth_var_z = z0z + z * dz_linear

    sin_phase = math.sin(phase)
    cos_phase = math.cos(phase)
    result["u"] = amplitude * omega * cos_dir * depth_var_xy * sin_phase
    result["v"] = amplitude * omega * sin_dir * depth_var_xy * sin_phase
    result["w"] = -amplitude * omega * depth_var_z * cos_phase
    result["ax"] = -amplitude * omega * omega * cos_dir * depth_var_xy * cos_phase
    result["ay"] = -amplitude * omega * omega * sin_dir * depth_var_xy * cos_phase
    result["az"] = -amplitude * omega * omega * depth_var_z * sin_phase
    pressure_factor = math.exp(k * eval_z) if deep_water else math.cosh(k * (eval_z + depth)) / math.cosh(kd)
    result["dynP"] = K_DENSITY_WATER * K_GRAVITY * amplitude * pressure_factor * sin_phase
    return result, submerged


def run_case(exe: Path, qoe: Path, cwd: Path) -> None:
    completed = subprocess.run(
        [str(exe), "--qod", str(qoe)],
        cwd=cwd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if completed.returncode != 0:
        raise RuntimeError(f"Case failed: {qoe.name}\nSTDOUT:\n{completed.stdout}\nSTDERR:\n{completed.stderr}")


def spectrum_density_from_components(freqs: list[float], amps: list[float]) -> tuple[list[float], list[float], float]:
    grouped: list[tuple[float, float]] = []
    for f, a in sorted(zip(freqs, amps), key=lambda item: item[0]):
        energy = 0.5 * a * a
        if grouped and abs(grouped[-1][0] - f) < 1.0e-9:
            grouped[-1] = (grouped[-1][0], grouped[-1][1] + energy)
        else:
            grouped.append((f, energy))
    if not grouped:
        return [], [], 0.0
    unique_freqs = [item[0] for item in grouped]
    energies = [item[1] for item in grouped]
    widths: list[float] = []
    if len(unique_freqs) == 1:
        widths = [max(unique_freqs[0], 0.05)]
    else:
        for i in range(len(unique_freqs)):
            if i == 0:
                width = unique_freqs[1] - unique_freqs[0]
            elif i == len(unique_freqs) - 1:
                width = unique_freqs[-1] - unique_freqs[-2]
            else:
                width = 0.5 * (unique_freqs[i + 1] - unique_freqs[i - 1])
            widths.append(max(width, 1.0e-6))
    density = [e / w for e, w in zip(energies, widths)]
    return unique_freqs, density, sum(energies)


def log_rmse(theory: list[float], measured: list[float]) -> float:
    if not theory or len(theory) != len(measured):
        return float("inf")
    err = 0.0
    for t, m in zip(theory, measured):
        err += (math.log10(max(t, 1.0e-8)) - math.log10(max(m, 1.0e-8))) ** 2
    return math.sqrt(err / len(theory))


def focused_log_rmse(theory: list[float], measured: list[float], floor_ratio: float = 0.02) -> float:
    if not theory or len(theory) != len(measured):
        return float("inf")
    peak = max(max(theory), max(measured), K_MIN_POSITIVE)
    cutoff = peak * floor_ratio
    filtered_theory: list[float] = []
    filtered_measured: list[float] = []
    for t, m in zip(theory, measured):
        if max(t, m) >= cutoff:
            filtered_theory.append(t)
            filtered_measured.append(m)
    if not filtered_theory:
        return log_rmse(theory, measured)
    return log_rmse(filtered_theory, filtered_measured)


def build_decomposition_spectrum(times: list[float], eta: list[float]) -> tuple[list[float], list[float], float]:
    count = min(len(times), len(eta))
    if count < 2:
        return [], [], 0.0
    dt = infer_uniform_dt(times[:count])
    if dt <= 0.0:
        return [], [], 0.0
    mean_eta = sum(eta[:count]) / count
    demeaned = [value - mean_eta for value in eta[:count]]
    total_duration = count * dt
    df = 1.0 / total_duration
    max_index = count // 2
    freqs: list[float] = []
    density: list[float] = []
    total_energy = 0.0
    for k in range(1, max_index + 1):
        real = 0.0
        imag = 0.0
        for n, sample in enumerate(demeaned):
            angle = -2.0 * K_PI * k * n / count
            real += sample * math.cos(angle)
            imag += sample * math.sin(angle)
        amplitude = 2.0 * math.sqrt(real * real + imag * imag) / count
        energy = 0.5 * amplitude * amplitude
        freqs.append(k * df)
        density.append(energy / df)
        total_energy += energy
    return freqs, density, total_energy


def validate_spectrum_case(result_dir: Path, out_dir: Path, spec: CaseSpec, cfg: dict[str, object]) -> dict[str, object]:
    times, eta = read_wts(result_dir / f"{spec.name}.wts")
    freqs, amps, _, _, _, _ = read_wvc(result_dir / f"{spec.name}.wvc")
    comp_f, comp_s, m0 = spectrum_density_from_components(freqs, amps)
    theory_f, theory_s = build_theory_curve(cfg, "user_spectrum" if spec.kind == "user_spectrum" else "spectrum")
    sampled_theory = [interpolate_linear(f, theory_f, theory_s) for f in comp_f]
    expected_m0 = integrate_trapezoid(theory_f, theory_s)
    plot = plot_spectrum_case(spec.name, out_dir, times, eta, theory_f, theory_s, comp_f, comp_s)
    log_error = log_rmse(sampled_theory, comp_s)
    m0_error = abs(m0 - expected_m0) / max(expected_m0, K_MIN_POSITIVE)
    threshold = 0.25 if spec.kind == "user_spectrum" else 0.45
    passed = m0_error < 0.15 and log_error < threshold
    return {
        "case": spec.name,
        "type": spec.kind,
        "status": "PASS" if passed else "FAIL",
        "metrics": {
            "m0_discrete": m0,
            "m0_expected": expected_m0,
            "m0_rel_error": m0_error,
            "spectrum_log_rmse": log_error,
            "plot": plot,
        },
        "summary": parse_summary(result_dir / f"{spec.name}.wfm"),
    }


def validate_user_timeseries_case(result_dir: Path, out_dir: Path, spec: CaseSpec, cfg: dict[str, object]) -> dict[str, object]:
    src_t, src_eta = read_wts(Path(cfg["ImportedTimeSeriesPath"]))
    out_t, out_eta = read_wts(result_dir / f"{spec.name}.wts")
    out_freqs, out_amps, _, _, _, _ = read_wvc(result_dir / f"{spec.name}.wvc")
    src_freqs, src_density, src_m0 = build_decomposition_spectrum(src_t, src_eta)
    out_comp_f, out_comp_s, out_m0 = spectrum_density_from_components(out_freqs, out_amps)
    sampled_source = [interpolate_linear(freq, src_freqs, src_density) for freq in out_comp_f]
    theory = [interpolate_linear(t, src_t, src_eta) for t in out_t]
    theory_zero = demean(theory)
    out_zero = demean(out_eta)
    plot = plot_user_timeseries_case(spec.name, out_dir, out_t, out_zero, src_t, demean(src_eta), src_freqs, src_density, out_comp_f, out_comp_s)
    error = rmse(out_zero, theory_zero)
    spec_error = focused_log_rmse(sampled_source, out_comp_s)
    return {
        "case": spec.name,
        "type": spec.kind,
        "status": "PASS" if error < 0.12 and spec_error < 0.30 else "FAIL",
        "metrics": {
            "eta_rmse_zero_mean": error,
            "source_mean_eta": sum(src_eta) / max(len(src_eta), 1),
            "output_mean_eta": sum(out_eta) / max(len(out_eta), 1),
            "m0_source_decomposition": src_m0,
            "m0_output_reconstruction": out_m0,
            "spectrum_log_rmse": spec_error,
            "plot": plot,
        },
        "summary": parse_summary(result_dir / f"{spec.name}.wfm"),
    }


def validate_regular_case(result_dir: Path, out_dir: Path, spec: CaseSpec, cfg: dict[str, object]) -> dict[str, object]:
    cache = read_wfc(result_dir / f"{spec.name}.wfc")
    ix = cache["nx"] // 2
    iy = cache["ny"] // 2
    iz = cache["nz"] // 2
    z = cache["z_bottom"] + iz * cache["dz"]
    depth = cache["water_depth"]
    times = [i * cache["dt"] for i in range(cache["nt"])]
    eta = [cache["eta"][cache_index(cache, ix, iy, iz, it)] for it in range(cache["nt"])]
    u = [cache["u"][cache_index(cache, ix, iy, iz, it)] for it in range(cache["nt"])]
    w = [cache["w"][cache_index(cache, ix, iy, iz, it)] for it in range(cache["nt"])]
    dynp = [cache["dynP"][cache_index(cache, ix, iy, iz, it)] for it in range(cache["nt"])]

    amplitude = 0.5 * float(cfg["Hs"])
    omega = 2.0 * K_PI / float(cfg["Tp"])
    phase0 = float(cfg.get("RegularPhase", 0.0))
    k = solve_wave_number(omega, depth)
    kd = k * depth
    theory_eta: list[float] = []
    theory_u: list[float] = []
    theory_w: list[float] = []
    theory_dp: list[float] = []
    submerged: list[int] = []
    for t in times:
        eta_surface = amplitude * math.sin(-omega * t + phase0)
        eval_z = depth * (z - eta_surface) / (eta_surface + depth)
        xy = math.cosh(k * (eval_z + depth)) / math.sinh(kd)
        zf = math.sinh(k * (eval_z + depth)) / math.sinh(kd)
        pressure_factor = math.cosh(k * (eval_z + depth)) / math.cosh(kd)
        theory_eta.append(eta_surface)
        theory_u.append(amplitude * omega * xy * math.sin(-omega * t + phase0))
        theory_w.append(-amplitude * omega * zf * math.cos(-omega * t + phase0))
        theory_dp.append(K_DENSITY_WATER * K_GRAVITY * amplitude * pressure_factor * math.sin(-omega * t + phase0))
        submerged.append(1 if 1.0 <= eta_surface else 0)

    plot = plot_regular_kinematics(
        spec.name,
        out_dir,
        times,
        {"eta": (eta, theory_eta), "u": (u, theory_u), "w": (w, theory_w), "dynP": (dynp, theory_dp)},
        submerged,
    )
    metrics = {
        "eta_rmse": rmse(eta, theory_eta),
        "u_rmse": rmse(u, theory_u),
        "w_rmse": rmse(w, theory_w),
        "dynp_rmse": rmse(dynp, theory_dp),
        "plot": plot,
    }
    passed = metrics["eta_rmse"] < 1.0e-5 and metrics["u_rmse"] < 1.0e-5 and metrics["w_rmse"] < 1.0e-5 and metrics["dynp_rmse"] < 1.0e-3
    return {"case": spec.name, "type": spec.kind, "status": "PASS" if passed else "FAIL", "metrics": metrics, "summary": parse_summary(result_dir / f"{spec.name}.wfm")}


def validate_dense_regular_case(result_dir: Path, out_dir: Path, spec: CaseSpec, cfg: dict[str, object]) -> dict[str, object]:
    cache = read_wfc(result_dir / f"{spec.name}.wfc")
    time_indices = sorted(set([0, max(0, cache["nt"] // 8), max(0, cache["nt"] // 4), max(0, cache["nt"] // 2), max(0, 3 * cache["nt"] // 4), cache["nt"] - 1]))
    max_errors = {key: 0.0 for key in ("eta", "u", "v", "w", "ax", "ay", "az", "dynP")}
    submerged_mismatch_count = 0
    sample_ix = cache["nx"] // 2
    sample_iy = cache["ny"] // 2
    sample_iz = max(1, min(cache["nz"] - 2, cache["nz"] // 2))
    for it in time_indices:
        t = it * cache["dt"]
        for iz in range(cache["nz"]):
            z = cache["z_bottom"] + iz * cache["dz"]
            for iy in range(cache["ny"]):
                y = 0.0 if cache["ny"] == 1 else (iy - 0.5 * (cache["ny"] - 1)) * cache["dy"]
                for ix in range(cache["nx"]):
                    x = 0.0 if cache["nx"] == 1 else (ix - 0.5 * (cache["nx"] - 1)) * cache["dx"]
                    idx = cache_index(cache, ix, iy, iz, it)
                    theory, submerged = evaluate_regular_wave_theory(cfg, x, y, z, t)
                    cache_values = {
                        "eta": cache["eta"][idx],
                        "u": cache["u"][idx],
                        "v": cache["v"][idx],
                        "w": cache["w"][idx],
                        "ax": cache["ax"][idx],
                        "ay": cache["ay"][idx],
                        "az": cache["az"][idx],
                        "dynP": cache["dynP"][idx],
                    }
                    for key in max_errors:
                        max_errors[key] = max(max_errors[key], abs(cache_values[key] - theory[key]))
                    if (z <= cache_values["eta"]) != bool(submerged):
                        submerged_mismatch_count += 1

    slice_it = cache["nt"] // 4
    slice_t = slice_it * cache["dt"]
    x_values = [0.0 if cache["nx"] == 1 else (ix - 0.5 * (cache["nx"] - 1)) * cache["dx"] for ix in range(cache["nx"])]
    y_values = [0.0 if cache["ny"] == 1 else (iy - 0.5 * (cache["ny"] - 1)) * cache["dy"] for iy in range(cache["ny"])]
    z_values = [cache["z_bottom"] + iz * cache["dz"] for iz in range(cache["nz"])]
    eta_xy_cache: list[list[float]] = []
    eta_xy_error: list[list[float]] = []
    for iy, y in enumerate(y_values):
        row_cache: list[float] = []
        row_error: list[float] = []
        for ix, x in enumerate(x_values):
            idx = cache_index(cache, ix, iy, 0, slice_it)
            theory, _ = evaluate_regular_wave_theory(cfg, x, y, 0.0, slice_t)
            row_cache.append(cache["eta"][idx])
            row_error.append(abs(cache["eta"][idx] - theory["eta"]))
        eta_xy_cache.append(row_cache)
        eta_xy_error.append(row_error)

    u_xz_cache: list[list[float]] = []
    u_xz_error: list[list[float]] = []
    w_xz_cache: list[list[float]] = []
    w_xz_error: list[list[float]] = []
    u_yz_cache: list[list[float]] = []
    u_yz_error: list[list[float]] = []
    y_mid = y_values[sample_iy]
    for iz, z in enumerate(z_values):
        row_u_cache: list[float] = []
        row_u_error: list[float] = []
        row_w_cache: list[float] = []
        row_w_error: list[float] = []
        for ix, x in enumerate(x_values):
            idx = cache_index(cache, ix, sample_iy, iz, slice_it)
            theory, _ = evaluate_regular_wave_theory(cfg, x, y_mid, z, slice_t)
            row_u_cache.append(cache["u"][idx])
            row_u_error.append(abs(cache["u"][idx] - theory["u"]))
            row_w_cache.append(cache["w"][idx])
            row_w_error.append(abs(cache["w"][idx] - theory["w"]))
        u_xz_cache.append(row_u_cache)
        u_xz_error.append(row_u_error)
        w_xz_cache.append(row_w_cache)
        w_xz_error.append(row_w_error)

    x_mid = x_values[sample_ix]
    for iz, z in enumerate(z_values):
        row_u_cache: list[float] = []
        row_u_error: list[float] = []
        for iy, y in enumerate(y_values):
            idx = cache_index(cache, sample_ix, iy, iz, slice_it)
            theory, _ = evaluate_regular_wave_theory(cfg, x_mid, y, z, slice_t)
            row_u_cache.append(cache["u"][idx])
            row_u_error.append(abs(cache["u"][idx] - theory["u"]))
        u_yz_cache.append(row_u_cache)
        u_yz_error.append(row_u_error)

    sequence_indices = [0, cache["nt"] // 4, cache["nt"] // 2, (3 * cache["nt"]) // 4]
    eta_xy_frames: list[list[list[float]]] = []
    frame_times: list[float] = []
    for frame_it in sequence_indices:
        frame_it = min(max(frame_it, 0), cache["nt"] - 1)
        frame_t = frame_it * cache["dt"]
        frame_times.append(frame_t)
        frame_matrix: list[list[float]] = []
        for iy in range(cache["ny"]):
            row: list[float] = []
            for ix in range(cache["nx"]):
                idx = cache_index(cache, ix, iy, 0, frame_it)
                row.append(cache["eta"][idx])
            frame_matrix.append(row)
        eta_xy_frames.append(frame_matrix)

    plot = plot_dense_regular_case(
        spec.name,
        out_dir,
        eta_xy_cache,
        eta_xy_error,
        u_xz_cache,
        u_xz_error,
        w_xz_cache,
        w_xz_error,
        u_yz_cache,
        u_yz_error,
        (x_values[0], x_values[-1], y_values[0], y_values[-1]),
        (x_values[0], x_values[-1], z_values[0], z_values[-1]),
        (y_values[0], y_values[-1], z_values[0], z_values[-1]),
    )
    sequence_plot = plot_dense_regular_sequence(
        spec.name,
        out_dir,
        eta_xy_frames,
        frame_times,
        (x_values[0], x_values[-1], y_values[0], y_values[-1]),
    )
    metrics = dict(max_errors)
    metrics["submerged_mismatch_count"] = submerged_mismatch_count
    metrics["plot"] = plot
    metrics["sequence_plot"] = sequence_plot
    passed = (
        max_errors["eta"] < 1.0e-12 and
        max_errors["u"] < 1.0e-12 and
        max_errors["v"] < 1.0e-12 and
        max_errors["w"] < 1.0e-12 and
        max_errors["ax"] < 1.0e-11 and
        max_errors["ay"] < 1.0e-11 and
        max_errors["az"] < 1.0e-11 and
        max_errors["dynP"] < 1.0e-8 and
        submerged_mismatch_count == 0
    )
    return {"case": spec.name, "type": spec.kind, "status": "PASS" if passed else "FAIL", "metrics": metrics, "summary": parse_summary(result_dir / f"{spec.name}.wfm")}


def validate_import_cache(result_dir: Path, spec: CaseSpec, dependency: str) -> dict[str, object]:
    src = parse_summary(result_dir / f"{dependency}.wfm")
    imp = parse_summary(result_dir / f"{spec.name}.wfm")
    metrics = {
        "hs_abs_error": abs(float(src["Significant wave height Hs"]) - float(imp["Significant wave height Hs"])),
        "tp_abs_error": abs(float(src["Peak period Tp"]) - float(imp["Peak period Tp"])),
        "m0_abs_error": abs(float(src["m0"]) - float(imp["m0"])),
        "component_count_error": abs(int(src["Component count"]) - int(imp["Component count"])),
    }
    passed = metrics["hs_abs_error"] < 1.0e-9 and metrics["tp_abs_error"] < 1.0e-9 and metrics["m0_abs_error"] < 1.0e-9 and metrics["component_count_error"] == 0
    return {"case": spec.name, "type": spec.kind, "status": "PASS" if passed else "FAIL", "metrics": metrics, "summary": imp}


def render_report(results: list[dict[str, object]], json_path: Path, md_path: Path) -> None:
    pass_count = sum(1 for item in results if item["status"] == "PASS")
    json_path.write_text(json.dumps({"cases": results, "pass_count": pass_count, "total_count": len(results)}, ensure_ascii=False, indent=2), encoding="utf-8")
    lines = [
        "# WaveL Validation Report",
        "",
        f"- Total cases: {len(results)}",
        f"- Passed: {pass_count}",
        f"- Failed: {len(results) - pass_count}",
        "",
        "| Case | Type | Status | Key metrics |",
        "| --- | --- | --- | --- |",
    ]
    for item in results:
        metric_text = ", ".join(f"{k}={v:.6g}" for k, v in item["metrics"].items() if isinstance(v, (int, float)))
        lines.append(f"| {item['case']} | {item['type']} | {item['status']} | {metric_text} |")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Run WaveL validation suite for the internal .wfc/.wfm service path.")
    parser.add_argument("--exe", default="build/debug/Qahse.exe")
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--result-dir", default="demo/WaveL/result")
    parser.add_argument("--output-dir", default="demo/WaveL/validation_result")
    parser.add_argument("--skip-run", action="store_true")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    exe = (repo_root / args.exe).resolve()
    result_dir = (repo_root / args.result_dir).resolve()
    out_dir = (repo_root / args.output_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if not args.skip_run:
        for spec in CASE_SPECS:
            run_case(exe, repo_root / "demo" / "WaveL" / spec.qoe, repo_root)

    results: list[dict[str, object]] = []
    defaults = qoe_defaults()
    for spec in CASE_SPECS:
        cfg = parse_qoe(repo_root / "demo" / "WaveL" / spec.qoe)
        for key, value in defaults.items():
            cfg.setdefault(key, value)
        for key in ("ImportedSpectrumPath", "ImportedTimeSeriesPath", "ImportedComponentsPath", "ImportedCachePath"):
            value = cfg.get(key)
            if isinstance(value, str) and value and not Path(value).is_absolute():
                cfg[key] = str((repo_root / "demo" / "WaveL" / value).resolve())

        if spec.kind in ("spectrum", "user_spectrum"):
            result = validate_spectrum_case(result_dir, out_dir, spec, cfg)
        elif spec.kind == "user_timeseries":
            result = validate_user_timeseries_case(result_dir, out_dir, spec, cfg)
        elif spec.kind == "regular_kinematics":
            result = validate_regular_case(result_dir, out_dir, spec, cfg)
        elif spec.kind == "regular_dense_grid":
            result = validate_dense_regular_case(result_dir, out_dir, spec, cfg)
        elif spec.kind == "import_cache":
            result = validate_import_cache(result_dir, spec, spec.dependency or "")
        else:
            raise RuntimeError(f"Unhandled case kind: {spec.kind}")
        results.append(result)

    json_path = out_dir / "validation_summary.json"
    md_path = out_dir / "validation_report.md"
    render_report(results, json_path, md_path)

    failures = [item["case"] for item in results if item["status"] != "PASS"]
    print(f"WaveL validation cases: {len(results)}")
    print(f"Passed: {len(results) - len(failures)}")
    print(f"Failed: {len(failures)}")
    print(f"Report: {md_path}")
    if failures:
        print("Failed cases:")
        for case in failures:
            print(f"  - {case}")


if __name__ == "__main__":
    main()
