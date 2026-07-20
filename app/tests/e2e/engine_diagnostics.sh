#!/bin/bash

set -e
set -x

source app/tests/e2e/lib.sh

setup_e2e "$@"

output_file=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s option.Hash=-16 option.Threads=2 -rounds 5 -repeat -concurrency 2 \
    -openings file=app/tests/data/openings.pgn format=pgn order=random -log file=log.txt level=info 2>&1 | tee "$output_file"

if ! grep -q "Warning; random_move_1 doesn't have option Threads" "$output_file"; then
    echo "Failed to report missing option Threads."
    exit 1
fi

if ! grep -q "Warning; Invalid value for option Hash; -16" "$output_file"; then
    echo "Failed to report invalid value for option Hash."
    exit 1
fi

check_no_engine_runtime_errors "$output_file"

if [[ "$OSTYPE" != "linux-gnu" && "$OSTYPE" != "darwin"* ]]; then
    echo "Skipping non-uci responding engine test on non-linux/macOS systems."
    exit 0
fi

output_file=$(mktemp)
./fastchess -engine cmd=app/tests/mock/engine/missing_engine.sh name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s option.Hash=-16 option.Threads=2 -rounds 5 -repeat -concurrency 1 \
    -openings file=app/tests/data/openings.pgn format=pgn order=random -log file=log.txt level=warn 2>&1 | tee "$output_file"

if ! grep -q "Fatal; random_move_1 engine startup failure: \"Engine didn't respond to uciok after startup\"" "$output_file"; then
    echo "Failed to report invalid command."
    exit 1
fi

echo "Engine diagnostic tests passed."
