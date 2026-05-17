#!/usr/bin/env python3
"""Independent literature-formula checks for WaveL demo outputs.

The script uses Airy finite-depth linear wave kinematics, the dispersion
relation omega^2 = g k tanh(k h), and spectral moment Hm0 = 4 sqrt(m0) as
the independent benchmark formulas. WaveL is sampled through the CLI, while
the reference values are reconstructed from the exported component files.
"""

from __future__ import annotations

import csv
import math
import os
import re
import shlex
import subprocess
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parents[3]
DEMO = ROOT / "demo" / "WaveL"
RESULT = DEMO / "result"
OUT = DEMO / "verification"
QAHSE_EXE = ROOT / "build" / "cli" / "Qahse.exe"
SAMPLE = (0.0, 0.0, -10.0, 12.0)
PI = math.pi
TINY = 1.0e-12


@dataclass
class Component:
    f: float
    amp: float
    phase: float
    direction: float
    k: float


def clean_ansi(text: str) -> str:
    return re.sub(r"\x1b\[[0-9;]*m", "", text)


def parse_value(text: str):
    text = text.strip().strip('"')
    if text.lower() in {"true", "false"}:
        return text.lower() == "true"
    try:
        if any(c in text for c in ".eE"):
            return float(text)
        return int(text)
    except ValueError:
        return text


def read_input(path: Path) -> dict:
    data: dict = {"input_path": path}
    for raw in path.read_text(encoding="utf-8-sig", errors="ignore").splitlines():
        line = raw.strip()
        if not line or line.startswith("--") or line.startswith("#") or line.upper() == "END":
            continue
        head = line.split(" - ", 1)[0].strip()
        if not head or head.startswith("-"):
            continue
        try:
            parts = shlex.split(head, posix=True)
        except ValueError:
            parts = head.split()
        if len(parts) < 2:
            continue
        data[parts[1]] = parse_value(parts[0])
    return data


def resolve_path(input_data: dict, key: str) -> Path:
    path = Path(str(input_data.get(key, "")))
    if not path.is_absolute():
        path = Path(input_data["input_path"]).parent / path
    return path.resolve()


def component_path(input_data: dict) -> Path:
    save_path = resolve_path(input_data, "SavePath")
    save_name = str(input_data.get("SaveName", Path(input_data["input_path"]).stem))
    return save_path / f"{save_name}_components.dat"


def read_components(path: Path) -> list[Component]:
    components: list[Component] = []
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        text = raw.strip()
        if not text or text.startswith("#"):
            continue
        cols = text.split()
        if len(cols) < 5:
            continue
        components.append(
            Component(
                f=float(cols[0]),
                amp=float(cols[1]),
                phase=math.radians(float(cols[2])),
                direction=math.radians(float(cols[3])),
                k=float(cols[4]),
            )
        )
    if not components:
        raise RuntimeError(f"no WaveL components in {path}")
    return components


def run_cli(input_file: Path) -> dict:
    if not QAHSE_EXE.exists():
        raise RuntimeError(f"CLI not found: {QAHSE_EXE}")
    env = os.environ.copy()
    env["PATH"] = (
        r"C:\Program Files (x86)\Intel\oneAPI\mkl\2025.3\bin;"
        r"C:\Program Files (x86)\Intel\oneAPI\compiler\2025.3\bin;"
        + str(ROOT / "lib" / "Debug_d")
        + ";"
        + env.get("PATH", "")
    )
    cmd = [
        str(QAHSE_EXE),
        "--wavel",
        str(input_file),
        "--sample",
        *(str(v) for v in SAMPLE),
    ]
    completed = subprocess.run(
        cmd,
        cwd=ROOT,
        env=env,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        errors="replace",
    )
    text = clean_ansi(completed.stdout)

    def scalar(label: str) -> float:
        match = re.search(rf"{label}:\s*([-+0-9.eE]+)", text)
        if not match:
            raise RuntimeError(f"CLI output missing {label} for {input_file.name}")
        return float(match.group(1))

    def vector(label: str) -> tuple[float, float, float]:
        match = re.search(rf"{label}:\s*([-+0-9.eE]+),\s*([-+0-9.eE]+),\s*([-+0-9.eE]+)", text)
        if not match:
            raise RuntimeError(f"CLI output missing {label} for {input_file.name}")
        return tuple(float(match.group(i)) for i in range(1, 4))

    return {
        "eta": scalar("Elevation"),
        "wave_velocity": vector("WaveVelocity"),
        "wave_acceleration": vector("WaveAcceleration"),
        "current_velocity": vector("CurrentVelocity"),
        "water_velocity": vector("WaterVelocity"),
        "dynp": scalar("DynamicPressureHead"),
    }


def rotate_z(speed: float, direction_deg: float) -> tuple[float, float, float]:
    angle = math.radians(direction_deg)
    return speed * math.cos(angle), speed * math.sin(angle), 0.0


def add3(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return a[0] + b[0], a[1] + b[1], a[2] + b[2]


def reconstruct_state(input_data: dict, components: list[Component]) -> dict:
    x, y, z, time = SAMPLE
    depth = float(input_data.get("WaterDepth", 0.0))
    stretching = int(input_data.get("WaveStretching", 1))
    time_offset = float(input_data.get("TimeOffset", 0.0))
    eta = 0.0
    for c in components:
        omega = 2.0 * PI * c.f
        projected = x * math.cos(c.direction) + y * math.sin(c.direction)
        theta = c.k * projected - omega * (time + time_offset) + c.phase
        eta += c.amp * math.sin(theta)

    wave_velocity = [0.0, 0.0, 0.0]
    wave_acceleration = [0.0, 0.0, 0.0]
    dynp = 0.0
    if depth > TINY and z + depth >= 0.0:
        eval_z = z
        free_surface = 0.0 if stretching == 3 else eta
        if eval_z <= free_surface:
            if stretching == 0 and eval_z > 0.0:
                eval_z = 0.0
            elif stretching == 1:
                eval_z = (eval_z - eta) * depth / (eta + depth)
            if eval_z + depth >= 0.0:
                for c in components:
                    omega = 2.0 * PI * c.f
                    projected = x * math.cos(c.direction) + y * math.sin(c.direction)
                    theta = c.k * projected - omega * (time + time_offset) + c.phase
                    if depth > 100.0:
                        depth_xy = math.exp(c.k * eval_z)
                        depth_z = depth_xy
                        if stretching == 2 and eval_z > 0.0:
                            depth_xy = 1.0 + c.k * eval_z
                            depth_z = depth_xy
                    else:
                        sinh_kd = math.sinh(c.k * depth)
                        if abs(sinh_kd) <= TINY:
                            continue
                        depth_xy = math.cosh(c.k * (eval_z + depth)) / sinh_kd
                        depth_z = math.sinh(c.k * (eval_z + depth)) / sinh_kd
                        if stretching == 2 and eval_z > 0.0:
                            depth_xy = math.cosh(c.k * depth) / sinh_kd + eval_z * c.k
                            depth_z = 1.0 + eval_z * c.k * math.cosh(c.k * depth) / sinh_kd
                    aomega = c.amp * omega
                    aomega2 = aomega * omega
                    dirx = math.cos(c.direction)
                    diry = math.sin(c.direction)
                    wave_velocity[0] += aomega * dirx * depth_xy * math.sin(theta)
                    wave_velocity[1] += aomega * diry * depth_xy * math.sin(theta)
                    wave_velocity[2] += -aomega * depth_z * math.cos(theta)
                    wave_acceleration[0] += -aomega2 * dirx * depth_xy * math.cos(theta)
                    wave_acceleration[1] += -aomega2 * diry * depth_xy * math.cos(theta)
                    wave_acceleration[2] += -aomega2 * depth_z * math.sin(theta)
                    dynp += math.tanh(c.k * depth) * depth_xy * c.amp * math.sin(theta)

    current = current_at(input_data, z, eta)
    water = add3(tuple(wave_velocity), current)
    return {
        "eta": eta,
        "wave_velocity": tuple(wave_velocity),
        "wave_acceleration": tuple(wave_acceleration),
        "current_velocity": current,
        "water_velocity": water,
        "dynp": dynp,
    }


def current_at(input_data: dict, z: float, eta: float) -> tuple[float, float, float]:
    depth = float(input_data.get("WaterDepth", 0.0))
    stretching = int(input_data.get("WaveStretching", 1))
    if depth <= TINY or z + depth < 0.0:
        return 0.0, 0.0, 0.0
    if stretching == 3 and z > 0.0:
        return 0.0, 0.0, 0.0
    if stretching != 3 and z > eta:
        return 0.0, 0.0, 0.0

    eval_z = z
    shear_extrapolation = 0.0
    if stretching == 0:
        eval_z = min(eval_z, 0.0)
    elif stretching == 1:
        eval_z = -depth * (eval_z - eta) / (eta - depth)
    elif stretching == 2:
        shear_depth = float(input_data.get("ShearCurrentDepth", 0.0))
        if shear_depth > TINY:
            shear_extrapolation = eval_z * float(input_data.get("ShearCurrent", 0.0)) / shear_depth
        eval_z = 0.0
    if eval_z + depth < 0.0:
        return 0.0, 0.0, 0.0

    result = rotate_z(float(input_data.get("ConstCurrent", 0.0)), float(input_data.get("ConstCurrentDir", 0.0)))
    shear_depth = float(input_data.get("ShearCurrentDepth", 0.0))
    if shear_depth > TINY and -eval_z < shear_depth:
        speed = (shear_depth + eval_z) / shear_depth * float(input_data.get("ShearCurrent", 0.0)) + shear_extrapolation
        result = add3(result, rotate_z(speed, float(input_data.get("ShearCurrentDir", 0.0))))

    profile_base = max((depth + eval_z) / depth, 0.0)
    profile_speed = (profile_base ** float(input_data.get("ProfileCurrentExponent", 1.0))) * float(
        input_data.get("ProfileCurrent", 0.0)
    )
    result = add3(result, rotate_z(profile_speed, float(input_data.get("ProfileCurrentDir", 0.0))))
    return result


def dispersion_residual(input_data: dict, components: list[Component]) -> float:
    depth = float(input_data.get("WaterDepth", 0.0))
    gravity = float(input_data.get("Gravity", 9.81))
    residual = 0.0
    for c in components:
        omega = 2.0 * PI * c.f
        expected = gravity * c.k * math.tanh(c.k * depth)
        scale = max(omega * omega, abs(expected), 1.0)
        residual = max(residual, abs(omega * omega - expected) / scale)
    return residual


def discrete_hs(components: list[Component]) -> float:
    m0 = 0.5 * sum(c.amp * c.amp for c in components)
    return 4.0 * math.sqrt(max(m0, 0.0))


def jonswap_spectrum(f: np.ndarray, hs: float, tp: float, gamma: float = 3.3) -> np.ndarray:
    fp = 1.0 / tp
    sigma = np.where(f <= fp, 0.07, 0.09)
    ratio = f / fp
    peak = np.exp(-0.5 * ((ratio - 1.0) / sigma) ** 2)
    return (
        0.3125
        * hs
        * hs
        * tp
        * ratio ** -5
        * np.exp(-1.25 * ratio ** -4)
        * (1.0 - 0.287 * math.log(gamma))
        * gamma ** peak
    )


def component_density(components: list[Component]) -> tuple[np.ndarray, np.ndarray]:
    freqs = np.array([c.f for c in components], dtype=float)
    energy = np.array([0.5 * c.amp * c.amp for c in components], dtype=float)
    order = np.argsort(freqs)
    freqs = freqs[order]
    energy = energy[order]
    if len(freqs) == 1:
        return freqs, energy
    mids = 0.5 * (freqs[:-1] + freqs[1:])
    edges = np.empty(len(freqs) + 1)
    edges[1:-1] = mids
    edges[0] = max(0.0, freqs[0] - (mids[0] - freqs[0]))
    edges[-1] = freqs[-1] + (freqs[-1] - mids[-1])
    widths = np.maximum(np.diff(edges), TINY)
    return freqs, energy / widths


def max_state_error(a: dict, b: dict) -> float:
    values = [abs(a["eta"] - b["eta"]), abs(a["dynp"] - b["dynp"])]
    for key in ["wave_velocity", "wave_acceleration", "current_velocity", "water_velocity"]:
        values.extend(abs(a[key][i] - b[key][i]) for i in range(3))
    return max(values)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    input_files = sorted(DEMO.glob("Qahse_WaveL_*_DEMO.dat"))
    state_rows = []
    summary_rows = []
    spectra_for_plot = {}

    for input_file in input_files:
        input_data = read_input(input_file)
        cpath = component_path(input_data)
        if not cpath.exists():
            run_cli(input_file)
        components = read_components(cpath)
        cli = run_cli(input_file)
        ref = reconstruct_state(input_data, components)
        case = input_file.stem.replace("Qahse_WaveL_", "").replace("_DEMO", "")
        max_err = max_state_error(cli, ref)

        state_rows.append(
            {
                "case": case,
                "eta_cli": cli["eta"],
                "eta_reference": ref["eta"],
                "u_cli": cli["wave_velocity"][0],
                "u_reference": ref["wave_velocity"][0],
                "w_cli": cli["wave_velocity"][2],
                "w_reference": ref["wave_velocity"][2],
                "ax_cli": cli["wave_acceleration"][0],
                "ax_reference": ref["wave_acceleration"][0],
                "az_cli": cli["wave_acceleration"][2],
                "az_reference": ref["wave_acceleration"][2],
                "dynp_cli": cli["dynp"],
                "dynp_reference": ref["dynp"],
                "max_abs_state_error": max_err,
            }
        )

        hs_d = discrete_hs(components)
        hs_input = float(input_data.get("Hs", 0.0))
        summary_rows.append(
            {
                "case": case,
                "wave_type": input_data.get("WaveType", ""),
                "components": len(components),
                "hs_input": hs_input,
                "hs_from_components": hs_d,
                "hs_abs_error": abs(hs_d - hs_input) if hs_input > 0.0 else "",
                "max_dispersion_relative_residual": dispersion_residual(input_data, components),
                "max_abs_state_error": max_err,
            }
        )
        spectra_for_plot[case] = (input_data, components)

    with (OUT / "wavel_literature_state_samples.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(state_rows[0].keys()))
        writer.writeheader()
        writer.writerows(state_rows)

    with (OUT / "wavel_literature_benchmark_summary.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(summary_rows[0].keys()))
        writer.writeheader()
        writer.writerows(summary_rows)

    with (OUT / "wavel_literature_benchmark_summary.txt").open("w", encoding="utf-8") as f:
        f.write("WaveL literature-formula benchmark\n")
        f.write("Reference formulas: Airy finite-depth linear wave kinematics, omega^2=g*k*tanh(k*h), Hm0=4*sqrt(m0).\n")
        f.write(f"Sample point: x={SAMPLE[0]}, y={SAMPLE[1]}, z={SAMPLE[2]}, t={SAMPLE[3]}\n\n")
        for row in summary_rows:
            f.write(
                f"{row['case']}: components={row['components']}, "
                f"Hs_components={float(row['hs_from_components']):.12g}, "
                f"dispersion_residual={float(row['max_dispersion_relative_residual']):.3e}, "
                f"state_error={float(row['max_abs_state_error']):.3e}\n"
            )

    plot_state_errors(state_rows)
    plot_jonswap_spectrum(spectra_for_plot)
    plot_airy_reference()


def plot_state_errors(rows: list[dict]) -> None:
    cases = [row["case"] for row in rows]
    errors = [float(row["max_abs_state_error"]) for row in rows]
    plt.figure(figsize=(10, 4.8))
    plt.bar(cases, errors)
    plt.yscale("log")
    plt.ylabel("max absolute error")
    plt.title("WaveL CLI sample vs independent Airy superposition reconstruction")
    plt.xticks(rotation=25, ha="right")
    plt.grid(axis="y", alpha=0.3)
    plt.tight_layout()
    plt.savefig(OUT / "wavel_literature_state_error.png", dpi=160)
    plt.close()


def plot_jonswap_spectrum(spectra_for_plot: dict) -> None:
    if "JONSWAP" not in spectra_for_plot:
        return
    input_data, components = spectra_for_plot["JONSWAP"]
    hs = float(input_data.get("Hs", 0.0))
    tp = float(input_data.get("Tp", 0.0))
    gamma = float(input_data.get("Gamma", 3.3))
    f = np.linspace(0.5 / tp, 1.0 / tp * 10.0, 1500)
    s = jonswap_spectrum(f, hs, tp, gamma)
    fc, sc = component_density(components)

    plt.figure(figsize=(10, 5.5))
    plt.plot(f, s, label="JONSWAP literature formula", lw=2)
    plt.scatter(fc, sc, s=18, label="WaveL discrete component energy density", alpha=0.75)
    plt.xlabel("frequency (Hz)")
    plt.ylabel("S(f) (m^2/Hz)")
    plt.title("JONSWAP continuous spectrum vs WaveL discretization")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(OUT / "wavel_literature_jonswap_spectrum.png", dpi=160)
    plt.close()


def plot_airy_reference() -> None:
    hs = 6.0
    tp = 10.0
    depth = 87.0
    z = -10.0
    gravity = 9.81
    amp = 0.5 * hs
    omega = 2.0 * PI / tp
    k = omega * omega / gravity
    for _ in range(32):
        kd = k * depth
        f = gravity * k * math.tanh(kd) - omega * omega
        df = gravity * (math.tanh(kd) + k * depth / math.cosh(kd) ** 2)
        k -= f / df
    t = np.linspace(0.0, 2.0 * tp, 500)
    theta = -omega * t
    depth_xy = math.cosh(k * (z + depth)) / math.sinh(k * depth)
    depth_z = math.sinh(k * (z + depth)) / math.sinh(k * depth)
    eta = amp * np.sin(theta)
    u = amp * omega * depth_xy * np.sin(theta)
    w = -amp * omega * depth_z * np.cos(theta)
    ax = -amp * omega * omega * depth_xy * np.cos(theta)
    az = -amp * omega * omega * depth_z * np.sin(theta)

    fig, axes = plt.subplots(3, 1, figsize=(10, 7), sharex=True)
    axes[0].plot(t, eta)
    axes[0].set_ylabel("eta (m)")
    axes[1].plot(t, u, label="u")
    axes[1].plot(t, w, label="w")
    axes[1].set_ylabel("velocity (m/s)")
    axes[1].legend()
    axes[2].plot(t, ax, label="ax")
    axes[2].plot(t, az, label="az")
    axes[2].set_ylabel("acc. (m/s^2)")
    axes[2].set_xlabel("time (s)")
    axes[2].legend()
    for ax_i in axes:
        ax_i.grid(True, alpha=0.3)
    fig.suptitle("Airy finite-depth reference: H=6 m, T=10 s, h=87 m, z=-10 m")
    fig.tight_layout()
    fig.savefig(OUT / "wavel_literature_airy_reference.png", dpi=160)
    plt.close(fig)


if __name__ == "__main__":
    main()
