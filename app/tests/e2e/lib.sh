#!/bin/bash

set -e
set -x

setup_e2e() {
    if [ "${E2E_SKIP_BUILD:-0}" = "1" ]; then
        return
    fi

    g++ -O3 -std=c++17 app/tests/mock/engine/random_mover.cpp -o random_mover
    g++ -O3 -std=c++17 app/tests/mock/engine/scripted_result.cpp -o scripted_result
    make -j build=debug "$@"
}

check_no_engine_runtime_errors() {
    local output_file=$1

    if grep -q "WARNING: ThreadSanitizer:" "$output_file"; then
        echo "Data races detected."
        exit 1
    fi

    if grep -q "illegal move" "$output_file"; then
        echo "Illegal move detected."
        exit 1
    fi

    if grep -q "disconnects" "$output_file"; then
        echo "Disconnect detected."
        exit 1
    fi

    if grep -q "stalls" "$output_file"; then
        echo "Stall detected."
        exit 1
    fi

    if grep -q "loses on time" "$output_file"; then
        echo "Loses on time detected."
        exit 1
    fi
}
