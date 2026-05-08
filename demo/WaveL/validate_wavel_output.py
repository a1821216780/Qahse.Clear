from __future__ import annotations

import argparse
from pathlib import Path
import math

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager


def configure_cjk_font():
    preferred = [
        "Microsoft YaHei",
        "SimHei",
        "Noto Sans CJK SC",
        "Source Han Sans SC",
        "PingFang SC",
    ]
    available = {f.name for f in font_manager.fontManager.ttflist}
    for name in preferred:
        if name in available:
            plt.rcParams["font.sans-serif"] = [name]
            plt.rcParams["axes.unicode_minus"] = False
            return


def read_wts(path: Path):
    times = []
    eta = []
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
    freqs = []
    amps = []
    phases = []
    dirs = []
    ks = []
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
        if len(parts) < 5:
            continue
        freqs.append(float(parts[0]))
        amps.append(float(parts[1]))
        phases.append(float(parts[2]))
        dirs.append(float(parts[3]))
        ks.append(float(parts[4]))
    return freqs, amps, phases, dirs, ks


def plot_case(result_dir: Path, case_name: str):
    wts = result_dir / f"{case_name}.wts"
    wvc = result_dir / f"{case_name}.wvc"
    if not wts.exists():
        raise FileNotFoundError(f"缺少时程文件: {wts}")
    if not wvc.exists():
        raise FileNotFoundError(f"缺少成分文件: {wvc}")

    times, eta = read_wts(wts)
    freqs, amps, phases, dirs, ks = read_wvc(wvc)
    energies = [0.5 * a * a for a in amps]

    fig, axes = plt.subplots(2, 1, figsize=(10, 8), constrained_layout=True)

    axes[0].plot(times, eta, color="#1f77b4", linewidth=1.5)
    axes[0].set_title(f"{case_name} 自由液面时程")
    axes[0].set_xlabel("时间 t (s)")
    axes[0].set_ylabel("eta (m)")
    axes[0].grid(True, alpha=0.3)

    markerline, stemlines, baseline = axes[1].stem(freqs, energies, basefmt=" ")
    plt.setp(markerline, marker="o", markersize=4, color="#d62728")
    plt.setp(stemlines, linewidth=1.2, color="#d62728")
    axes[1].set_title(f"{case_name} 离散谱线")
    axes[1].set_xlabel("频率 f (Hz)")
    axes[1].set_ylabel("每分量能量 0.5*a^2 (m^2)")
    axes[1].grid(True, alpha=0.3)

    out = result_dir / f"{case_name}_validation.png"
    fig.savefig(out, dpi=180)
    plt.close(fig)
    return out


def main():
    configure_cjk_font()
    parser = argparse.ArgumentParser(description="校核 WaveL 输出，绘制 eta(t) 和离散谱线。")
    parser.add_argument(
        "--result-dir",
        default="demo/WaveL/result",
        help="WaveL 结果目录，默认 demo/WaveL/result",
    )
    parser.add_argument(
        "--cases",
        nargs="+",
        required=True,
        help="案例基名列表，不含扩展名",
    )
    args = parser.parse_args()

    result_dir = Path(args.result_dir)
    outputs = []
    for case in args.cases:
        outputs.append(plot_case(result_dir, case))

    print("已生成校核图:")
    for out in outputs:
        print(out)


if __name__ == "__main__":
    main()
