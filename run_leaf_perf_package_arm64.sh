#!/usr/bin/env bash
#
# Run a prebuilt Falcon leaf/perf package on an aarch64 machine.
# This script never calls Bazel and only connects to 127.0.0.1.
#
# Usage:
#   bash run_leaf_perf_package_arm64.sh all INDEX_OUTPUT REQUEST_JSONL
#   bash run_leaf_perf_package_arm64.sh start INDEX_OUTPUT
#   bash run_leaf_perf_package_arm64.sh perf REQUEST_JSONL
#   bash run_leaf_perf_package_arm64.sh status
#   bash run_leaf_perf_package_arm64.sh stop
#   bash run_leaf_perf_package_arm64.sh logs
#
# Environment overrides:
#   LEAF_PORT=6635
#   LEAF_START_TIMEOUT=600
#   PERF_THREADS=20
#   PERF_SECONDS=60
#   PERF_STUB_NUM=10
#   REQUEST_SECTION_FROM=relevance_learning2rank
#   REQUEST_SECTION_TO=relevance_softLtrAfmConditionv1

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly PACKAGE_ROOT="${SCRIPT_DIR}"
readonly LEAF_BINARY="${PACKAGE_ROOT}/bin/leaf"
readonly PERF_BINARY="${PACKAGE_ROOT}/bin/perf"
readonly ACTION="${1:-help}"
readonly LEAF_SERVER_IP="127.0.0.1"
readonly LEAF_PORT="${LEAF_PORT:-6635}"
readonly LEAF_START_TIMEOUT="${LEAF_START_TIMEOUT:-600}"
readonly LEAF_STOP_TIMEOUT="${LEAF_STOP_TIMEOUT:-75}"
readonly PERF_THREADS="${PERF_THREADS:-20}"
readonly PERF_SECONDS="${PERF_SECONDS:-60}"
readonly PERF_STUB_NUM="${PERF_STUB_NUM:-10}"
readonly REQUEST_SECTION_FROM="${REQUEST_SECTION_FROM:-relevance_learning2rank}"
readonly REQUEST_SECTION_TO="${REQUEST_SECTION_TO:-relevance_softLtrAfmConditionv1}"
readonly RUNTIME_DIR="${LEAF_PERF_RUNTIME_ROOT:-${PACKAGE_ROOT}/runtime_${LEAF_PORT}}"
readonly PID_FILE="${RUNTIME_DIR}/leaf.pid"
readonly INDEX_STATE_FILE="${RUNTIME_DIR}/index_path"
readonly LEAF_LOG="${RUNTIME_DIR}/leaf.log"
readonly PERF_LOG="${RUNTIME_DIR}/perf.log"

INDEX_PATH=""
PREPARED_REQUEST_PATH=""

print_usage()
{
    sed -n '5,20p' "$0"
}

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

validate_positive_integer()
{
    local name="$1"
    local value="$2"
    if [[ ! "${value}" =~ ^[0-9]+$ ]] || ((value < 1)); then
        fail "${name} must be a positive integer, got: ${value}"
    fi
}

check_binary()
{
    local binary="$1"
    [[ -x "${binary}" ]] || fail "executable does not exist: ${binary}"

    if command -v ldd >/dev/null 2>&1; then
        local ldd_output
        ldd_output="$(ldd "${binary}" 2>&1 || true)"
        if grep -q "not found" <<<"${ldd_output}"; then
            echo "${ldd_output}" >&2
            fail "runtime library is missing for ${binary}"
        fi
    fi
}

validate_runtime()
{
    local host_arch
    host_arch="$(uname -m)"
    if [[ "${host_arch}" != "aarch64" && "${host_arch}" != "arm64" ]]; then
        fail "this package only supports aarch64/arm64; current architecture=${host_arch}"
    fi

    validate_positive_integer "LEAF_PORT" "${LEAF_PORT}"
    if ((LEAF_PORT > 65535)); then
        fail "LEAF_PORT must not exceed 65535, got: ${LEAF_PORT}"
    fi
    validate_positive_integer "LEAF_START_TIMEOUT" "${LEAF_START_TIMEOUT}"
    validate_positive_integer "LEAF_STOP_TIMEOUT" "${LEAF_STOP_TIMEOUT}"
    validate_positive_integer "PERF_THREADS" "${PERF_THREADS}"
    validate_positive_integer "PERF_SECONDS" "${PERF_SECONDS}"
    validate_positive_integer "PERF_STUB_NUM" "${PERF_STUB_NUM}"

    check_binary "${LEAF_BINARY}"
    check_binary "${PERF_BINARY}"
    mkdir -p "${RUNTIME_DIR}"
}

resolve_index_path()
{
    local index_path_arg="$1"
    [[ -d "${index_path_arg}" ]] || fail "index directory does not exist: ${index_path_arg}"
    INDEX_PATH="$(cd "${index_path_arg}" && pwd -P)"

    local meta_files=("${INDEX_PATH}"/shard*.meta)
    local docid_files=("${INDEX_PATH}"/shard*.docids)
    local section_files=("${INDEX_PATH}"/shard*.section.*)
    [[ -e "${meta_files[0]}" ]] || fail "no shard*.meta file found in ${INDEX_PATH}"
    [[ -e "${docid_files[0]}" ]] || fail "no shard*.docids file found in ${INDEX_PATH}"
    [[ -e "${section_files[0]}" ]] || fail "no shard*.section.* file found in ${INDEX_PATH}"
}

load_index_state()
{
    if [[ -f "${INDEX_STATE_FILE}" ]]; then
        local saved_index
        saved_index="$(<"${INDEX_STATE_FILE}")"
        if [[ -d "${saved_index}" ]]; then
            INDEX_PATH="${saved_index}"
        fi
    fi
}

read_running_pid()
{
    [[ -f "${PID_FILE}" ]] || return 1

    local pid
    pid="$(<"${PID_FILE}")"
    [[ "${pid}" =~ ^[0-9]+$ ]] || return 1
    kill -0 "${pid}" 2>/dev/null || return 1

    local expected_executable
    local running_executable
    expected_executable="$(readlink -f "${LEAF_BINARY}")"
    running_executable="$(readlink -f "/proc/${pid}/exe" 2>/dev/null || true)"
    [[ "${running_executable}" == "${expected_executable}" ]] || return 1

    printf '%s\n' "${pid}"
}

wait_for_leaf()
{
    local leaf_pid="$1"
    local elapsed
    for ((elapsed = 0; elapsed < LEAF_START_TIMEOUT; ++elapsed)); do
        if ! kill -0 "${leaf_pid}" 2>/dev/null; then
            echo "Leaf exited while loading the index. Last log lines:" >&2
            tail -n 80 "${LEAF_LOG}" >&2 || true
            rm -f "${PID_FILE}"
            return 1
        fi

        if grep -q "add new shard:test_corpus/" "${LEAF_LOG}" &&
            grep -q "grpc server begin loop" "${LEAF_LOG}"; then
            return 0
        fi
        sleep 1
    done

    echo "Leaf did not finish loading within ${LEAF_START_TIMEOUT}s. Last log lines:" >&2
    tail -n 80 "${LEAF_LOG}" >&2 || true
    return 1
}

start_leaf()
{
    local index_path_arg="$1"
    if [[ "$(id -u)" -eq 0 ]]; then
        fail "Falcon leaf refuses to run as root; use a non-root account"
    fi

    resolve_index_path "${index_path_arg}"
    local running_pid
    if running_pid="$(read_running_pid)"; then
        fail "leaf is already running: pid=${running_pid}, log=${LEAF_LOG}"
    fi
    rm -f "${PID_FILE}"
    printf '%s\n' "${INDEX_PATH}" >"${INDEX_STATE_FILE}"

    echo "[1/2] Starting local leaf and loading ${INDEX_PATH}"
    nohup "${LEAF_BINARY}" \
        --test_shard_path="${INDEX_PATH}" \
        --server_ip="${LEAF_SERVER_IP}" \
        --grpc_server_port="${LEAF_PORT}" \
        --etcd_address= \
        >"${LEAF_LOG}" 2>&1 &

    local leaf_pid=$!
    printf '%s\n' "${leaf_pid}" >"${PID_FILE}"

    if ! wait_for_leaf "${leaf_pid}"; then
        kill "${leaf_pid}" 2>/dev/null || true
        rm -f "${PID_FILE}"
        fail "leaf startup failed"
    fi

    echo "Leaf is ready: pid=${leaf_pid}, address=${LEAF_SERVER_IP}:${LEAF_PORT}"
    echo "Local shard key: corpus=test_corpus, cycle=<empty>, shard=0, replica=0"
    echo "Leaf log: ${LEAF_LOG}"
}

prepare_request_file()
{
    local request_path_arg="$1"
    [[ -f "${request_path_arg}" ]] || fail "request file does not exist: ${request_path_arg}"
    [[ -s "${request_path_arg}" ]] || fail "request file is empty: ${request_path_arg}"

    local request_path
    request_path="$(cd "$(dirname "${request_path_arg}")" && pwd -P)/$(basename "${request_path_arg}")"
    PREPARED_REQUEST_PATH="${request_path}"

    if [[ -z "${REQUEST_SECTION_FROM}" || "${REQUEST_SECTION_FROM}" == "${REQUEST_SECTION_TO}" ]]; then
        return 0
    fi
    if [[ ! "${REQUEST_SECTION_FROM}" =~ ^[[:alnum:]_-]+$ ||
        ! "${REQUEST_SECTION_TO}" =~ ^[[:alnum:]_-]+$ ]]; then
        fail "REQUEST_SECTION_FROM/TO may contain only letters, digits, '_' and '-'"
    fi
    if ! grep -q -m 1 "${REQUEST_SECTION_FROM}" "${request_path}"; then
        return 0
    fi

    load_index_state
    if [[ -n "${INDEX_PATH}" ]]; then
        local target_sections=("${INDEX_PATH}"/shard*.section."${REQUEST_SECTION_TO}")
        [[ -e "${target_sections[0]}" ]] ||
            fail "target vector section was not found in the loaded index: ${REQUEST_SECTION_TO}"
    fi

    PREPARED_REQUEST_PATH="${RUNTIME_DIR}/requests_${REQUEST_SECTION_TO}.jsonl"
    if [[ ! -s "${PREPARED_REQUEST_PATH}" || "${request_path}" -nt "${PREPARED_REQUEST_PATH}" ]]; then
        echo "Preparing a local request copy:"
        echo "  ${REQUEST_SECTION_FROM} -> ${REQUEST_SECTION_TO}"
        echo "WARNING: this changes only the section identifier; it does not prove that both names use the same embedding model." >&2
        sed "s/${REQUEST_SECTION_FROM}/${REQUEST_SECTION_TO}/g" \
            "${request_path}" >"${PREPARED_REQUEST_PATH}.tmp"
        mv -f "${PREPARED_REQUEST_PATH}.tmp" "${PREPARED_REQUEST_PATH}"
    fi
}

run_perf()
{
    local request_path_arg="$1"
    local running_pid
    if ! running_pid="$(read_running_pid)"; then
        fail "local leaf is not running; run the start or all action first"
    fi

    prepare_request_file "${request_path_arg}"
    echo "[2/2] Running perf.cpp: threads=${PERF_THREADS}, seconds=${PERF_SECONDS}"
    echo "Perf input: ${PREPARED_REQUEST_PATH}"
    echo "Perf log: ${PERF_LOG}"

    set +e
    "${PERF_BINARY}" \
        --tgt=leaf \
        --dst="${LEAF_SERVER_IP}:${LEAF_PORT}" \
        --data="${PREPARED_REQUEST_PATH}" \
        --thread_num="${PERF_THREADS}" \
        --seconds="${PERF_SECONDS}" \
        --stub_num="${PERF_STUB_NUM}" \
        --corpus=test_corpus \
        --cycle= \
        --shard=0 \
        --replica=0 \
        2>&1 | tee "${PERF_LOG}"
    local perf_status=${PIPESTATUS[0]}
    set -e

    if ((perf_status != 0)); then
        fail "perf exited with status ${perf_status}; see ${PERF_LOG}"
    fi
}

show_status()
{
    local running_pid
    if running_pid="$(read_running_pid)"; then
        echo "leaf is running: pid=${running_pid}, address=${LEAF_SERVER_IP}:${LEAF_PORT}"
        echo "leaf log: ${LEAF_LOG}"
        return 0
    fi
    echo "leaf is not running for port ${LEAF_PORT}"
    return 1
}

stop_leaf()
{
    local running_pid
    if ! running_pid="$(read_running_pid)"; then
        rm -f "${PID_FILE}"
        echo "leaf is not running for port ${LEAF_PORT}"
        return 0
    fi

    kill "${running_pid}"
    local elapsed
    for ((elapsed = 0; elapsed < LEAF_STOP_TIMEOUT; ++elapsed)); do
        if ! kill -0 "${running_pid}" 2>/dev/null; then
            rm -f "${PID_FILE}"
            echo "leaf stopped: pid=${running_pid}"
            return 0
        fi
        sleep 1
    done
    fail "leaf did not stop within ${LEAF_STOP_TIMEOUT}s; pid=${running_pid}"
}

show_logs()
{
    [[ -f "${LEAF_LOG}" ]] || fail "leaf log does not exist: ${LEAF_LOG}"
    tail -n 100 -f "${LEAF_LOG}"
}

case "${ACTION}" in
    all)
        (($# >= 3)) || fail "all requires INDEX_OUTPUT and REQUEST_JSONL"
        validate_runtime
        start_leaf "$2"
        run_perf "$3"
        ;;
    start)
        (($# >= 2)) || fail "start requires INDEX_OUTPUT"
        validate_runtime
        start_leaf "$2"
        ;;
    perf)
        (($# >= 2)) || fail "perf requires REQUEST_JSONL"
        validate_runtime
        run_perf "$2"
        ;;
    status)
        validate_runtime
        show_status
        ;;
    stop)
        validate_runtime
        stop_leaf
        ;;
    logs)
        validate_runtime
        show_logs
        ;;
    help | -h | --help)
        print_usage
        ;;
    *)
        echo "ERROR: unsupported action: ${ACTION}" >&2
        print_usage >&2
        exit 2
        ;;
esac
