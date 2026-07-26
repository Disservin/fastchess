#!/bin/bash

set -e
set -x

source app/tests/e2e/lib.sh

setup_e2e "$@"

output_file=$(mktemp)
./fastchess -engine cmd=./scripted_result args=--crash-on-go name=crasher -engine cmd=./scripted_result name=healthy \
    -each nodes=1 -rounds 2 -repeat -concurrency 1 -recover -log file=log.txt level=info 2>&1 | tee "$output_file"

if ! grep -q "disconnects}" "$output_file"; then
    echo "Crash was not reported as a disconnect."
    exit 1
fi

if ! grep -q "Crashed: 4" "$output_file"; then
    echo "Crash count was not reported."
    exit 1
fi

if ! grep -q "Finished game 4" "$output_file"; then
    echo "Recover mode did not continue through all crash games."
    exit 1
fi

output_file=$(mktemp)
./fastchess -engine cmd=./scripted_result args=--stall-after-go name=staller -engine cmd=./scripted_result name=healthy \
    -each nodes=1 -games 1 -rounds 1 -noswap -concurrency 1 -recover -ping-ms 50 -log file=log.txt level=info 2>&1 | tee "$output_file"

if ! grep -q "stalls}" "$output_file"; then
    echo "Stall was not reported."
    exit 1
fi

if ! grep -q "Finished game 1" "$output_file"; then
    echo "Stall game was not completed."
    exit 1
fi

output_file=$(mktemp)
./fastchess -engine cmd=./scripted_result args=--crash-on-go name=crasher -engine cmd=./scripted_result name=healthy \
    -each nodes=1 -rounds 2 -repeat -concurrency 1 -log file=log.txt level=info 2>&1 | tee "$output_file" || true

if ! grep -q "no recover option set" "$output_file"; then
    echo "Non-recover crash did not stop the tournament."
    exit 1
fi

if grep -q "Finished game 4" "$output_file"; then
    echo "Non-recover crash unexpectedly continued the tournament."
    exit 1
fi

echo "Engine recovery tests passed."
