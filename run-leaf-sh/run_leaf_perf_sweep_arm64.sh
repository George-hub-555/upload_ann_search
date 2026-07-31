#!/usr/bin/env bash
#
# Run five local perf sweeps against an already-running Leaf.
# This script never starts, stops, or kills a Leaf process.
#
# Usage:
#   bash run_leaf_perf_sweep_arm64.sh REQUEST_JSONL
#
# Environment overrides:
#   LEAF_PORT=6335
#   PERF_SECONDS=60
#   PERF_STUB_NUM=10
#   INTERVAL_SECONDS=20
#   LEAF_PERF_RUNTIME_ROOT=/path/to/runtime_directory
#   PERF_SWEEP_RESULT_DIR=/path/to/result_directory
#

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly BASE_RUNNER="${SCRIPT_DIR}/run_leaf_perf_package_arm64.sh"
readonly REQUEST_PATH="${1:-}"
readonly LEAF_PORT_VALUE="${LEAF_PORT:-6335}"
readonly PERF_SECONDS_VALUE="${PERF_SECONDS:-60}"
readonly PERF_STUB_NUM_VALUE="${PERF_STUB_NUM:-10}"
readonly INTERVAL_SECONDS_VALUE="${INTERVAL_SECONDS:-20}"
readonly RUNTIME_DIR="${LEAF_PERF_RUNTIME_ROOT:-${SCRIPT_DIR}/runtime_${LEAF_PORT_VALUE}}"
readonly RESULT_DIR="${PERF_SWEEP_RESULT_DIR:-${RUNTIME_DIR}/sweep_results}"
readonly TOTAL_ROUNDS=5

readonly -a ASCENDING_THREADS=(1 2 4 6 8 10 12 16 20)
readonly -a DESCENDING_THREADS=(20 16 12 10 8 6 4 2 1)

declare -a RUN_PLAN=()

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

print_usage()
{
    sed -n '5,14p' "$0"
}

validate_positive_integer()
{
    local name="$1"
    local value="$2"
    [[ "${value}" =~ ^[0-9]+$ ]] || fail "${name} must be a positive integer, got: ${value}"
    ((value > 0)) || fail "${name} must be greater than zero, got: ${value}"
}

validate_non_negative_integer()
{
    local name="$1"
    local value="$2"
    [[ "${value}" =~ ^[0-9]+$ ]] || fail "${name} must be a non-negative integer, got: ${value}"
}

append_round_to_plan()
{
    local round="$1"
    shift

    local thread
    for thread in "$@"; do
        RUN_PLAN+=("${round}:${thread}")
    done
}

build_run_plan()
{
    local round
    for ((round = 1; round <= TOTAL_ROUNDS; ++round)); do
        if ((round % 2 == 1)); then
            append_round_to_plan "${round}" "${ASCENDING_THREADS[@]}"
        else
            append_round_to_plan "${round}" "${DESCENDING_THREADS[@]}"
        fi
    done
}

result_log_path()
{
    local sequence="$1"
    local round="$2"
    local thread="$3"
    printf '%s/%03d_round%d_thread%d.log\n' "${RESULT_DIR}" "${sequence}" "${round}" "${thread}"
}

validate_inputs()
{
    (($# == 1)) || {
        print_usage >&2
        fail "exactly one REQUEST_JSONL argument is required"
    }

    [[ -f "${BASE_RUNNER}" ]] || fail "base runner does not exist: ${BASE_RUNNER}"
    [[ -f "${REQUEST_PATH}" ]] || fail "request file does not exist: ${REQUEST_PATH}"
    [[ -s "${REQUEST_PATH}" ]] || fail "request file is empty: ${REQUEST_PATH}"

    validate_positive_integer "LEAF_PORT" "${LEAF_PORT_VALUE}"
    ((LEAF_PORT_VALUE <= 65535)) || fail "LEAF_PORT must not exceed 65535, got: ${LEAF_PORT_VALUE}"
    validate_positive_integer "PERF_SECONDS" "${PERF_SECONDS_VALUE}"
    validate_positive_integer "PERF_STUB_NUM" "${PERF_STUB_NUM_VALUE}"
    validate_non_negative_integer "INTERVAL_SECONDS" "${INTERVAL_SECONDS_VALUE}"
}

validate_leaf_is_running()
{
    echo "Checking the existing Leaf on 127.0.0.1:${LEAF_PORT_VALUE}"
    if ! LEAF_PORT="${LEAF_PORT_VALUE}" \
        PERF_THREADS=1 \
        PERF_SECONDS="${PERF_SECONDS_VALUE}" \
        PERF_STUB_NUM="${PERF_STUB_NUM_VALUE}" \
        bash "${BASE_RUNNER}" status; then
        fail "the package Leaf is not running on port ${LEAF_PORT_VALUE}; start it before running this sweep"
    fi
}

validate_result_paths()
{
    local index
    local sequence
    local round
    local thread
    local entry
    local log_path

    for ((index = 0; index < ${#RUN_PLAN[@]}; ++index)); do
        sequence=$((index + 1))
        entry="${RUN_PLAN[index]}"
        IFS=: read -r round thread <<<"${entry}"
        log_path="$(result_log_path "${sequence}" "${round}" "${thread}")"
        [[ ! -e "${log_path}" ]] || fail "result log already exists: ${log_path}"
    done
}

run_one_test()
{
    local sequence="$1"
    local total_runs="$2"
    local round="$3"
    local thread="$4"
    local log_path
    log_path="$(result_log_path "${sequence}" "${round}" "${thread}")"

    {
        printf 'run=%03d/%03d\n' "${sequence}" "${total_runs}"
        printf 'round=%d\n' "${round}"
        printf 'thread=%d\n' "${thread}"
        printf 'seconds=%d\n' "${PERF_SECONDS_VALUE}"
        printf 'stub_num=%d\n' "${PERF_STUB_NUM_VALUE}"
        printf 'leaf_port=%d\n' "${LEAF_PORT_VALUE}"
        printf 'request=%s\n' "${REQUEST_PATH}"
        echo
    } | tee "${log_path}"

    set +e
    LEAF_PORT="${LEAF_PORT_VALUE}" \
        PERF_THREADS="${thread}" \
        PERF_SECONDS="${PERF_SECONDS_VALUE}" \
        PERF_STUB_NUM="${PERF_STUB_NUM_VALUE}" \
        bash "${BASE_RUNNER}" perf "${REQUEST_PATH}" 2>&1 | tee -a "${log_path}"
    local perf_status=${PIPESTATUS[0]}
    set -e

    ((perf_status == 0)) ||
        fail "perf failed for run ${sequence}, round ${round}, thread ${thread}; see ${log_path}"
}

main()
{
    validate_inputs "$@"
    build_run_plan

    local total_runs="${#RUN_PLAN[@]}"
    ((total_runs == 45)) || fail "internal run plan error: expected 45 tests, got ${total_runs}"

    validate_leaf_is_running
    validate_result_paths
    mkdir -p "${RESULT_DIR}"

    echo "Starting ${total_runs} perf tests."
    echo "Results: ${RESULT_DIR}"
    echo "The existing Leaf will not be started, stopped, or killed by this script."

    local index
    local sequence
    local round
    local thread
    local entry

    for ((index = 0; index < total_runs; ++index)); do
        sequence=$((index + 1))
        entry="${RUN_PLAN[index]}"
        IFS=: read -r round thread <<<"${entry}"

        echo
        echo "===== Run $(printf '%03d' "${sequence}")/${total_runs}: round=${round}, thread=${thread} ====="
        run_one_test "${sequence}" "${total_runs}" "${round}" "${thread}"

        if ((sequence < total_runs && INTERVAL_SECONDS_VALUE > 0)); then
            echo "Waiting ${INTERVAL_SECONDS_VALUE}s before the next test..."
            sleep "${INTERVAL_SECONDS_VALUE}"
        fi
    done

    echo
    echo "Completed ${total_runs} perf tests."
    echo "Results: ${RESULT_DIR}"
}

main "$@"
