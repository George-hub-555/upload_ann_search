#!/usr/bin/env bash
#
# Run this script on build machine A (aarch64). It builds Falcon leaf and perf
# with Bazel 6.5.0 and creates a tar.gz package for runtime machine B.
#
# Usage:
#   bash tools/test/perf/build_leaf_perf_package_arm64_bazel65.sh [output_directory]
#
# Environment overrides:
#   BAZEL=/absolute/path/to/bazel-6.5.0
#   BAZEL_OUTPUT_USER_ROOT=/directory/with/enough/free/space

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd -P)"
readonly EXPECTED_BAZEL_VERSION="6.5.0"
readonly BUNDLED_BAZEL="${REPO_ROOT}/devel/builder/bazel-6.5.0-linux-arm64"
readonly OUTPUT_ROOT_ARG="${1:-${REPO_ROOT}/dist}"
readonly BAZEL_OUTPUT_USER_ROOT="${BAZEL_OUTPUT_USER_ROOT:-${REPO_ROOT}/bazel-cache}"
readonly RUNTIME_SCRIPT="${SCRIPT_DIR}/run_leaf_perf_package_arm64.sh"

BAZEL_CMD=""

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

validate_build_machine()
{
    local host_arch
    host_arch="$(uname -m)"
    if [[ "${host_arch}" != "aarch64" && "${host_arch}" != "arm64" ]]; then
        fail "build machine must be aarch64/arm64; current architecture=${host_arch}"
    fi
    if [[ ! -f "${REPO_ROOT}/WORKSPACE" && ! -f "${REPO_ROOT}/WORKSPACE.bazel" ]]; then
        fail "Falcon WORKSPACE was not found under ${REPO_ROOT}"
    fi
    [[ -f "${RUNTIME_SCRIPT}" ]] || fail "runtime script does not exist: ${RUNTIME_SCRIPT}"
}

select_bazel()
{
    if [[ -n "${BAZEL:-}" ]]; then
        BAZEL_CMD="${BAZEL}"
    elif [[ -x "${BUNDLED_BAZEL}" ]]; then
        BAZEL_CMD="${BUNDLED_BAZEL}"
    else
        BAZEL_CMD="bazel"
    fi

    if ! command -v "${BAZEL_CMD}" >/dev/null 2>&1; then
        fail "Bazel executable not found: ${BAZEL_CMD}"
    fi

    local version_output
    if ! version_output="$("${BAZEL_CMD}" --version 2>&1)"; then
        fail "failed to execute ${BAZEL_CMD}: ${version_output}"
    fi
    if [[ "${version_output}" != *"${EXPECTED_BAZEL_VERSION}"* ]]; then
        fail "Bazel ${EXPECTED_BAZEL_VERSION} is required; detected: ${version_output}"
    fi
    echo "Using ${version_output}"
}

bazel_command()
{
    "${BAZEL_CMD}" --output_user_root="${BAZEL_OUTPUT_USER_ROOT}" "$@"
}

copy_binary_and_runfiles()
{
    local source_binary="$1"
    local output_name="$2"
    cp -p "${source_binary}" "${PACKAGE_DIR}/bin/${output_name}"
    chmod +x "${PACKAGE_DIR}/bin/${output_name}"

    if [[ -d "${source_binary}.runfiles" ]]; then
        cp -aL "${source_binary}.runfiles" "${PACKAGE_DIR}/bin/${output_name}.runfiles"
    fi
    if [[ -f "${source_binary}.runfiles_manifest" ]]; then
        cp -p "${source_binary}.runfiles_manifest" "${PACKAGE_DIR}/bin/${output_name}.runfiles_manifest"
    fi
}

write_binary_report()
{
    local binary="$1"
    local name="$2"
    {
        echo "binary=${name}"
        echo "build_host=$(uname -a)"
        if command -v file >/dev/null 2>&1; then
            file "${binary}"
        fi
        if command -v ldd >/dev/null 2>&1; then
            ldd "${binary}" || true
        fi
    } >"${PACKAGE_DIR}/reports/${name}.txt" 2>&1
}

validate_build_machine
select_bazel
mkdir -p "${BAZEL_OUTPUT_USER_ROOT}"

echo "[1/3] Building leaf and perf for native aarch64"
(
    cd "${REPO_ROOT}"
    bazel_command build \
        //falcon/serving/leaf/bootstrap:leaf \
        //tools/test/perf:perf
)

readonly BAZEL_BIN="$(
    cd "${REPO_ROOT}"
    bazel_command info bazel-bin
)"
readonly LEAF_BINARY="${BAZEL_BIN}/falcon/serving/leaf/bootstrap/leaf"
readonly PERF_BINARY="${BAZEL_BIN}/tools/test/perf/perf"
[[ -x "${LEAF_BINARY}" ]] || fail "leaf binary was not generated: ${LEAF_BINARY}"
[[ -x "${PERF_BINARY}" ]] || fail "perf binary was not generated: ${PERF_BINARY}"

readonly OUTPUT_ROOT="$(mkdir -p "${OUTPUT_ROOT_ARG}" && cd "${OUTPUT_ROOT_ARG}" && pwd -P)"
readonly PACKAGE_NAME="leaf_perf_arm64_bazel65"
readonly PACKAGE_DIR="${OUTPUT_ROOT}/${PACKAGE_NAME}"
readonly ARCHIVE_PATH="${OUTPUT_ROOT}/${PACKAGE_NAME}.tar.gz"

echo "[2/3] Creating runtime package ${PACKAGE_DIR}"
if [[ -e "${PACKAGE_DIR}" ]]; then
    if [[ "${PACKAGE_DIR}" != "${OUTPUT_ROOT}/leaf_perf_arm64_bazel65" ]]; then
        fail "refusing to replace unexpected package path: ${PACKAGE_DIR}"
    fi
    rm -rf -- "${PACKAGE_DIR}"
fi
if [[ -e "${ARCHIVE_PATH}" ]]; then
    rm -f -- "${ARCHIVE_PATH}"
fi
if [[ -e "${ARCHIVE_PATH}.sha256" ]]; then
    rm -f -- "${ARCHIVE_PATH}.sha256"
fi

mkdir -p "${PACKAGE_DIR}/bin" "${PACKAGE_DIR}/reports"
copy_binary_and_runfiles "${LEAF_BINARY}" "leaf"
copy_binary_and_runfiles "${PERF_BINARY}" "perf"
cp -p "${RUNTIME_SCRIPT}" "${PACKAGE_DIR}/run_leaf_perf_package_arm64.sh"
chmod +x "${PACKAGE_DIR}/run_leaf_perf_package_arm64.sh"
write_binary_report "${LEAF_BINARY}" "leaf"
write_binary_report "${PERF_BINARY}" "perf"

{
    echo "bazel_version=${EXPECTED_BAZEL_VERSION}"
    echo "build_arch=$(uname -m)"
    echo "build_time=$(date -Iseconds)"
    echo "leaf_target=//falcon/serving/leaf/bootstrap:leaf"
    echo "perf_target=//tools/test/perf:perf"
} >"${PACKAGE_DIR}/BUILD_INFO"

echo "[3/3] Creating archive"
tar -C "${OUTPUT_ROOT}" -czf "${ARCHIVE_PATH}" "${PACKAGE_NAME}"
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${ARCHIVE_PATH}" >"${ARCHIVE_PATH}.sha256"
fi

echo "Package ready:"
echo "  ${ARCHIVE_PATH}"
echo "Copy this archive to runtime machine B."
