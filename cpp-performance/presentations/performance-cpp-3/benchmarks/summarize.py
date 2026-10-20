#!/usr/bin/env python3
"""Summarize recorded samples; optionally generate the presentation's SVG chart."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path
from statistics import median

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT / "results/raw.csv")
    parser.add_argument("--output", type=Path, default=ROOT / "results/summary.csv")
    parser.add_argument("--plot", action="store_true", help="requires matplotlib")
    args = parser.parse_args()
    groups = defaultdict(list)
    with args.input.open(newline="") as source:
        for row in csv.DictReader(source):
            groups[row["case"], int(row["footprint_bytes"])].append(row)
    summaries = []
    for (name, footprint), rows in groups.items():
        values = [float(row["ns_per_op"]) for row in rows]
        faults = [int(row["minor_faults"]) for row in rows]
        summaries.append(dict(case=name, footprint_bytes=footprint, samples=len(rows),
                              median_ns_per_op=median(values), min_ns_per_op=min(values),
                              max_ns_per_op=max(values), min_minor_faults=min(faults),
                              max_minor_faults=max(faults)))
    if not summaries:
        parser.error("input has no samples")
    with args.output.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=summaries[0].keys(), lineterminator="\n")
        writer.writeheader()
        writer.writerows(summaries)
    for row in summaries:
        print(f"{row['case']:18} {row['footprint_bytes']:10} B "
              f"{row['median_ns_per_op']:10.3f} ns/op "
              f"[{row['min_ns_per_op']:.3f}, {row['max_ns_per_op']:.3f}]")
    if args.plot:
        plot(summaries)


def plot(summaries):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import FixedLocator, FixedFormatter

    plt.rcParams.update({"font.size": 12, "font.family": "DejaVu Sans",
                         "text.color": "#e7edf5", "axes.labelcolor": "#e7edf5",
                         "xtick.color": "#c9dbe8", "ytick.color": "#c9dbe8",
                         "svg.hashsalt": "performance-cpp-3"})
    fig, ax = plt.subplots(figsize=(10.4, 3.7), layout="constrained")
    fig.patch.set_facecolor("#07111c")
    ax.set_facecolor("#101b2a")
    for case, color, label in [("chase1", "#71c6ff", "One dependent stream"),
                               ("chase8", "#52c7ad", "Eight independent streams")]:
        points = sorted((r for r in summaries if r["case"] == case), key=lambda r: r["footprint_bytes"])
        x = [r["footprint_bytes"] for r in points]
        ax.plot(x, [r["median_ns_per_op"] for r in points], "o-", color=color, label=label, linewidth=2)
        ax.fill_between(x, [r["min_ns_per_op"] for r in points],
                        [r["max_ns_per_op"] for r in points], color=color, alpha=0.18)
    ax.set_xscale("log", base=2)
    ticks = [4096, 32768, 262144, 2*1024**2, 8*1024**2, 64*1024**2]
    ax.xaxis.set_major_locator(FixedLocator(ticks))
    ax.xaxis.set_major_formatter(FixedFormatter(["4 KiB", "32 KiB", "256 KiB", "2 MiB", "8 MiB", "64 MiB"]))
    ax.set_ylabel("ns / dereference (lower is better)")
    ax.set_xlabel("Allocated node footprint · 64-byte nodes")
    ax.set_ylim(bottom=0)
    ax.grid(alpha=0.16, color="#a8bed0")
    for spine in ax.spines.values():
        spine.set_color("#39759c")
    legend = ax.legend(facecolor="#101b2a", edgecolor="#39759c", loc="upper left")
    for text in legend.get_texts():
        text.set_color("#e7edf5")
    dest = ROOT.parent / "public/figures/pointer-chase.svg"
    dest.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(dest, metadata={"Date": None, "Description": "Median and min-max of seven measured repetitions; WSL2 guest. Source: benchmarks/results/raw.csv."})
    plt.close(fig)
    dest.write_text("\n".join(line.rstrip() for line in dest.read_text().splitlines()) + "\n")


if __name__ == "__main__":
    main()
