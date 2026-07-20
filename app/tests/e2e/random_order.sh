#!/bin/bash

set -e
set -x

source app/tests/e2e/lib.sh

setup_e2e "$@"

rm -f random_run1.pgn random_run2.pgn

output_file=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 3 -repeat -concurrency 1 \
    -openings file=./app/tests/data/short.epd format=epd order=random -pgnout file=random_run1.pgn -log file=log.txt level=info 2>&1 | tee "$output_file"
check_no_engine_runtime_errors "$output_file"

output_file=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 3 -repeat -concurrency 1 \
    -openings file=./app/tests/data/short.epd format=epd order=random -pgnout file=random_run2.pgn -log file=log.txt level=info 2>&1 | tee "$output_file"
check_no_engine_runtime_errors "$output_file"

fens_run1=$(grep -A1 "^\[SetUp" random_run1.pgn | grep "^\[FEN" | head -6)
fens_run2=$(grep -A1 "^\[SetUp" random_run2.pgn | grep "^\[FEN" | head -6)

echo "First 6 FENs from run 1:"
echo "$fens_run1"
echo ""
echo "First 6 FENs from run 2:"
echo "$fens_run2"

if [ "$fens_run1" = "$fens_run2" ]; then
    echo "Opening order is the same in both runs. Randomization may not be working."
    exit 1
fi

echo "Random order test passed - openings were shuffled differently in each run."
