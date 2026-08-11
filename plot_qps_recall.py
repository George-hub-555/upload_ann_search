#!/usr/bin/env python3
"""Plot one logarithmic QPS-Recall figure per top-k/thread combination."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="results/all_results.csv")
    parser.add_argument("--output-dir", default="results/plots")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    input_path = Path(args.input)
    output_dir = Path(args.output_dir)
    if input_path.is_absolute() or output_dir.is_absolute():
        raise ValueError("input and output paths must be relative to test_all")

    try:
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise SystemExit("matplotlib is not installed; CSV generation is unaffected") from error

    grouped: dict[tuple[int, int], dict[str, list[tuple[float, float]]]] = defaultdict(
        lambda: defaultdict(list)
    )
    with input_path.open(newline="", encoding="utf-8") as source:
        for row in csv.DictReader(source):
            if row.get("status") != "ok" or not row.get("recall_at_k") or not row.get("qps"):
                continue
            key = (int(row["top_k"]), int(row["threads"]))
            grouped[key][row["algorithm"]].append(
                (float(row["recall_at_k"]), float(row["qps"]))
            )

    output_dir.mkdir(parents=True, exist_ok=True)
    for (top_k, threads), algorithms in sorted(grouped.items()):
        figure, axis = plt.subplots(figsize=(9, 6))
        for algorithm, points in sorted(algorithms.items()):
            points.sort()
            axis.plot(
                [point[0] for point in points],
                [point[1] for point in points],
                marker="o",
                linewidth=1.8,
                label=algorithm,
            )
        axis.set_yscale("log")
        axis.set_xlabel(f"Recall@{top_k}")
        axis.set_ylabel("QPS (log scale)")
        axis.set_title(f"SIFT1M L2 QPS-Recall@{top_k}, threads={threads}")
        axis.grid(True, which="both", linestyle="--", alpha=0.35)
        axis.legend()
        figure.tight_layout()
        output = output_dir / f"qps_recall_at_{top_k}_threads_{threads}.png"
        figure.savefig(output, dpi=180)
        plt.close(figure)
        print(output.as_posix())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
