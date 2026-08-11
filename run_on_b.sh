#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

case "$(uname -m)" in
  aarch64|arm64) ;;
  *)
    echo "ERROR: run_on_b.sh must run on Linux ARM64; detected $(uname -m)." >&2
    exit 2
    ;;
esac

command -v python3 >/dev/null || {
  echo "ERROR: python3 is required." >&2
  exit 2
}

python3 run_benchmarks.py --config config.json
