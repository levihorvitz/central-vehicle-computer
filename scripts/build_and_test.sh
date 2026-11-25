#!/usr/bin/env bash
# Builds the project and runs the tests in the three CI configurations, with warnings as errors.
# Usage: ./scripts/build_and_test.sh
set -euo pipefail

cd "$(dirname "$0")/.."

run_configuration() {
    local name="$1" dir="$2" build_type="$3" sanitizer="$4"
    local prefix=()
    if [[ "$sanitizer" == "thread" ]]; then
        # TSan crashes on WSL with address randomization; the build also runs the tests (gtest_discover_tests).
        prefix=(setarch "$(uname -m)" -R)
    fi

    echo "=== ${name} ==="
    if [[ ! -f "${dir}/CMakeCache.txt" ]]; then
        cmake -S . -B "$dir" -DCMAKE_BUILD_TYPE="$build_type" -DCVC_WERROR=ON -DCVC_SANITIZER="$sanitizer"
    fi
    "${prefix[@]}" cmake --build "$dir" -j
    "${prefix[@]}" ctest --test-dir "$dir" --output-on-failure | grep -v " Passed "
}

run_configuration "release" build Release ""
run_configuration "asan + ubsan" build-asan Debug "address,undefined"
run_configuration "tsan" build-tsan Debug "thread"

echo "=== All three configurations passed ==="
