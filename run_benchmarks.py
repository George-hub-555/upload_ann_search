#!/usr/bin/env python3
"""Run available ARM ANN executables and merge them into one CSV.

This script uses only the Python standard library.  Every configured path is
relative to test_all; no build-machine or test-machine absolute path is stored.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


FIELDS = [
    "algorithm",
    "implementation",
    "top_k",
    "threads",
    "search_parameter",
    "search_value",
    "recall_at_k",
    "qps",
    "repeat_count",
    "warmup_queries",
    "base_count",
    "query_count",
    "dimension",
    "index_parameters",
    "status",
    "note",
]

EXPECTED_ALGORITHMS = {
    "faiss-hnsw": "Faiss",
    "faiss-ivfpq": "Faiss",
    "hnswlib-hnsw": "hnswlib",
    "ngt": "NGT",
    "kgn": "KGN",
    "qsgngt": "QSG-NGT",
    "parlay-vamana": "ParlayANN",
    "parlay-hnsw": "ParlayANN",
    "parlay-hcnng": "ParlayANN",
    "parlay-pynndescent": "ParlayANN",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default="config.json")
    return parser.parse_args()


def list_arg(values: list[int]) -> str:
    return ",".join(str(value) for value in values)


def validate_relative(path_text: str, label: str) -> Path:
    path = Path(path_text)
    if path.is_absolute() or ".." in path.parts:
        raise ValueError(f"{label} must be a relative path inside test_all: {path_text}")
    return path


def validate_dataset(dataset: Path) -> None:
    required = [
        "sift_base.fvecs",
        "sift_query.fvecs",
        "sift_learn.fvecs",
        "sift_groundtruth.ivecs",
    ]
    missing = [name for name in required if not (dataset / name).is_file()]
    if missing:
        raise FileNotFoundError("missing dataset files: " + ", ".join(missing))


def run_command(command: list[str], log_path: Path) -> tuple[bool, str]:
    env = os.environ.copy()
    env.setdefault("OMP_PROC_BIND", "true")
    env.setdefault("OMP_PLACES", "cores")
    with log_path.open("w", encoding="utf-8") as log:
        log.write("command=" + " ".join(command) + "\n")
        log.flush()
        try:
            completed = subprocess.run(
                command,
                stdout=log,
                stderr=subprocess.STDOUT,
                env=env,
                check=False,
            )
        except OSError as error:
            return False, str(error)
    if completed.returncode != 0:
        return False, f"exit_code={completed.returncode}; see {log_path.as_posix()}"
    return True, ""


def load_result_rows(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        raise FileNotFoundError(f"benchmark did not create {path.as_posix()}")
    with path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        if reader.fieldnames != FIELDS:
            raise ValueError(
                f"unexpected CSV schema in {path.as_posix()}: {reader.fieldnames}"
            )
        rows = list(reader)
        if not rows:
            raise ValueError(f"benchmark produced no result rows in {path.as_posix()}")
        return rows


def run_adapter(
    command: list[str], log_path: Path, output_path: Path
) -> tuple[bool, str, list[dict[str, str]]]:
    """运行一个适配器，并尽量保留本次进程已经写出的有效曲线点。"""
    # 防止进程在创建新 CSV 前失败时，误把上一次运行的 raw CSV 当成本次结果。
    if output_path.is_file():
        output_path.unlink()

    ok, note = run_command(command, log_path)
    result_rows: list[dict[str, str]] = []
    if output_path.is_file():
        try:
            result_rows = load_result_rows(output_path)
        except (OSError, ValueError) as error:
            detail = str(error)
            note = f"{note}; {detail}" if note else detail
            ok = False
    elif ok:
        note = f"benchmark did not create {output_path.as_posix()}"
        ok = False

    return ok, note, result_rows


def status_row(algorithm: str, implementation: str, status: str, note: str) -> dict[str, str]:
    row = {field: "" for field in FIELDS}
    row.update(
        {
            "algorithm": algorithm,
            "implementation": implementation,
            "status": status,
            "note": note,
        }
    )
    return row


def main() -> int:
    args = parse_args()
    machine = platform.machine().lower()
    if machine not in {"aarch64", "arm64"}:
        print(
            f"ERROR: benchmarks are intentionally disabled on non-ARM hosts; detected {machine}",
            file=sys.stderr,
        )
        return 2

    config_path = validate_relative(args.config, "config")
    config = json.loads(config_path.read_text(encoding="utf-8"))
    dataset = validate_relative(config["dataset"], "dataset")
    results = validate_relative(config["results"], "results")
    binary_dir = validate_relative(config["binary_dir"], "binary_dir")
    validate_dataset(dataset)

    raw_dir = results / "raw"
    log_dir = results / "logs"
    raw_dir.mkdir(parents=True, exist_ok=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    rows: list[dict[str, str]] = []
    completed_algorithms: set[str] = set()

    # 每个适配器独立运行：某一个失败时记录 failed，后面的算法仍继续执行。
    faiss_output = raw_dir / "faiss.csv"
    faiss_binary = binary_dir / "ann_faiss_benchmark"

    if faiss_binary.is_file():
        faiss = config["faiss"]
        command = [
            faiss_binary.as_posix(),
            "--dataset",
            dataset.as_posix(),
            "--output",
            faiss_output.as_posix(),
            "--top-k",
            list_arg(config["top_k"]),
            "--threads",
            list_arg(config["threads"]),
            "--repeats",
            str(config["repeats"]),
            "--warmup-queries",
            str(config["warmup_queries"]),
            "--query-count",
            str(config["query_count"]),
            "--hnsw-m",
            str(faiss["hnsw_m"]),
            "--hnsw-ef-construction",
            str(faiss["hnsw_ef_construction"]),
            "--ef-search",
            list_arg(faiss["ef_search"]),
            "--ivfpq-nlist",
            str(faiss["ivfpq_nlist"]),
            "--ivfpq-m",
            str(faiss["ivfpq_m"]),
            "--ivfpq-nbits",
            str(faiss["ivfpq_nbits"]),
            "--nprobe",
            list_arg(faiss["nprobe"]),
        ]
        ok, note, faiss_rows = run_adapter(
            command, log_dir / "faiss.log", faiss_output
        )
        rows.extend(faiss_rows)
        completed_algorithms.update(row["algorithm"] for row in faiss_rows)
        if not ok:
            for algorithm in ("faiss-hnsw", "faiss-ivfpq"):
                rows.append(status_row(algorithm, "Faiss", "failed", note))
    else:
        for algorithm in ("faiss-hnsw", "faiss-ivfpq"):
            rows.append(
                status_row(
                    algorithm,
                    "Faiss",
                    "skipped",
                    "bin/ann_faiss_benchmark is missing; build it on ARM machine A",
                )
            )

    # 所有适配器输出同一字段顺序，因此这里只需校验表头后直接合并。
    hnswlib_output = raw_dir / "hnswlib.csv"
    hnswlib_binary = binary_dir / "ann_hnswlib_benchmark"
    if hnswlib_binary.is_file():
        hnsw = config["hnswlib"]
        command = [
            hnswlib_binary.as_posix(),
            "--dataset",
            dataset.as_posix(),
            "--output",
            hnswlib_output.as_posix(),
            "--top-k",
            list_arg(config["top_k"]),
            "--threads",
            list_arg(config["threads"]),
            "--repeats",
            str(config["repeats"]),
            "--warmup-queries",
            str(config["warmup_queries"]),
            "--query-count",
            str(config["query_count"]),
            "--m",
            str(hnsw["m"]),
            "--ef-construction",
            str(hnsw["ef_construction"]),
            "--ef-search",
            list_arg(hnsw["ef_search"]),
        ]
        ok, note, hnswlib_rows = run_adapter(
            command, log_dir / "hnswlib.log", hnswlib_output
        )
        rows.extend(hnswlib_rows)
        completed_algorithms.update(row["algorithm"] for row in hnswlib_rows)
        if not ok:
            rows.append(status_row("hnswlib-hnsw", "hnswlib", "failed", note))
    else:
        rows.append(
            status_row(
                "hnswlib-hnsw",
                "hnswlib",
                "skipped",
                "bin/ann_hnswlib_benchmark is missing; build it on ARM machine A",
            )
        )

    ngt_output = raw_dir / "ngt.csv"
    ngt_binary = binary_dir / "ann_ngt_benchmark"
    if ngt_binary.is_file():
        ngt = config["ngt"]
        command = [
            ngt_binary.as_posix(),
            "--dataset",
            dataset.as_posix(),
            "--output",
            ngt_output.as_posix(),
            "--top-k",
            list_arg(config["top_k"]),
            "--threads",
            list_arg(config["threads"]),
            "--repeats",
            str(config["repeats"]),
            "--warmup-queries",
            str(config["warmup_queries"]),
            "--query-count",
            str(config["query_count"]),
            "--edge-size-for-creation",
            str(ngt["edge_size_for_creation"]),
            "--edge-size-for-search",
            str(ngt["edge_size_for_search"]),
            "--epsilon",
            ",".join(str(value) for value in ngt["epsilon"]),
        ]
        ok, note, ngt_rows = run_adapter(
            command, log_dir / "ngt.log", ngt_output
        )
        rows.extend(ngt_rows)
        completed_algorithms.update(row["algorithm"] for row in ngt_rows)
        if not ok:
            rows.append(status_row("ngt", "NGT", "failed", note))
    else:
        rows.append(
            status_row(
                "ngt",
                "NGT",
                "skipped",
                "bin/ann_ngt_benchmark is missing; build it on ARM machine A",
            )
        )

    skip_notes = {
        "kgn": "repository contains only a linux_x86_64 wheel and no ARM-buildable source",
        "qsgngt": "repository contains only x86_64 binaries/libraries and no ARM-buildable source",
        "parlay-vamana": "supplied ParlayANN distance code includes x86 intrinsics; ARM build disabled",
        "parlay-hnsw": "supplied ParlayANN HNSW benchmark/search entry is incomplete; ARM build disabled",
        "parlay-hcnng": "supplied ParlayANN distance code includes x86 intrinsics; ARM build disabled",
        "parlay-pynndescent": "supplied ParlayANN distance code includes x86 intrinsics; ARM build disabled",
    }
    existing_status = {row["algorithm"] for row in rows}
    for algorithm, implementation in EXPECTED_ALGORITHMS.items():
        if algorithm not in existing_status and algorithm not in completed_algorithms:
            rows.append(
                status_row(
                    algorithm,
                    implementation,
                    "skipped",
                    skip_notes.get(algorithm, "benchmark produced no result rows"),
                )
            )

    output_path = results / "all_results.csv"
    with output_path.open("w", newline="", encoding="utf-8") as destination:
        writer = csv.DictWriter(destination, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)

    metadata = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "machine": machine,
        "platform": platform.platform(),
        "config": config_path.as_posix(),
        "results": output_path.as_posix(),
    }
    (results / "run_metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    print(f"Unified CSV: {output_path.as_posix()}")
    successful = sum(1 for row in rows if row["status"] == "ok")
    print(f"Successful curve points: {successful}")

    if shutil.which("python3"):
        plot_command = ["python3", "plot_qps_recall.py", "--input", output_path.as_posix()]
        plot_ok, plot_note = run_command(plot_command, log_dir / "plot.log")
        if not plot_ok:
            print(f"Plot skipped: {plot_note}")

    return 0 if successful > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
