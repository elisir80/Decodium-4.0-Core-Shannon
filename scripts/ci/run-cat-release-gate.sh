#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Required release gate for the CAT contract.  It intentionally uses the
# release build directory and its pinned Hamlib rather than an unrelated CI
# image, so a package cannot be built after this gate has been skipped.
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 <cmake-build-dir> [parallel-jobs]" >&2
  exit 64
fi

build_dir="$1"
jobs="${2:-2}"
if [[ ! -f "${build_dir}/CTestTestfile.cmake" ]]; then
  echo "error: ${build_dir} was not configured with BUILD_TESTING=ON" >&2
  exit 1
fi

test_targets=(
  test_hamlib_release_contract
  test_cat_configuration_snapshot
  test_transceiver_base_split
  test_cw_cat_text
  test_serial_port_registry
  test_ptt_transition_policy
)
test_pattern='^(test_hamlib_release_contract|test_cat_configuration_snapshot|test_transceiver_base_split|test_cw_cat_text|test_serial_port_registry|test_ptt_transition_policy)$'

echo "Building mandatory CAT release tests with ${jobs} parallel jobs"
cmake --build "${build_dir}" --target "${test_targets[@]}" --parallel "${jobs}"

echo "Running mandatory CAT release tests"
ctest --test-dir "${build_dir}" -R "${test_pattern}" --output-on-failure
