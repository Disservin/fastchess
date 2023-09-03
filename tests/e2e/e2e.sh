#!/bin/bash 

# Compile the random_mover
set -x

g++ -O3 -std=c++17 tests/e2e/random_mover.cpp -o random_mover

make -j

OUTPUT_FILE=$(mktemp)
./fast-chess -engine cmd=random_mover name=random_move_1 -engine cmd=random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 4 \
    -openings file=tests/e2e/openings.txt format=epd order=random | tee $OUTPUT_FILE

# Check if "Saved results." is in the output, else fail
if ! grep -q "Saved results." $OUTPUT_FILE; then
    echo "Failed to save results."
    exit 1
fi

OUTPUT_FILE_2=$(mktemp)
./fast-chess -engine cmd=random_mover name=random_move_1 -engine cmd=random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 4 \
    -openings file=tests/e2e/openings.txt format=epd order=random | tee $OUTPUT_FILE_2

# Check if "Saved results." is in the output, else fail
if ! grep -q "Saved results." $OUTPUT_FILE_2; then
    echo "Failed to save results."
    exit 1
fi
