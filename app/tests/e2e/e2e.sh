#!/bin/bash 

set -x

# Since Linux Kernel 6.5 we are getting false positives from the ci,
# lower the ALSR entropy to disable ALSR, which works as a temporary workaround.
# https://github.com/google/sanitizers/issues/1716
# https://bugs.launchpad.net/ubuntu/+source/linux/+bug/2056762
sudo sysctl -w vm.mmap_rnd_bits=28

# Compile the random_mover
g++ -O3 -std=c++17 app/tests/mock/engine/random_mover.cpp -o random_mover

# Compile fast-chess
make -j build=debug $1

# EPD Book Test

OUTPUT_FILE=$(mktemp)
./fast-chess -engine cmd=random_mover name=random_move_1 -engine cmd=random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 2 \
    -openings file=./app/tests/data/openings.epd format=epd order=random -log file=log.txt level=info 2>&1 | tee $OUTPUT_FILE

if grep -q "WARNING: ThreadSanitizer:" $OUTPUT_FILE; then
    echo "Data races detected."
    exit 1
fi


# Check if "Saved results." is in the output, else fail
if ! grep -q "Saved results." $OUTPUT_FILE; then
    echo "Failed to save results."
    exit 1
fi

# If the output contains "illegal move" then fail
if grep -q "illegal move" $OUTPUT_FILE; then
    echo "Illegal move detected."
    exit 1
fi

# If the output contains "disconnects" then fail
if grep -q "disconnects" $OUTPUT_FILE; then
    echo "Disconnect detected."
    exit 1
fi

# If the output contains "loses on time" then fail
if grep -q "loses on time" $OUTPUT_FILE; then
    echo "Loses on time detected."
    exit 1
fi

# PGN Book Test

OUTPUT_FILE_2=$(mktemp)
./fast-chess -engine cmd=random_mover name=random_move_1 -engine cmd=random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 2 \
    -openings file=app/tests/data/openings.pgn format=pgn order=random -log file=log.txt level=info  2>&1 | tee $OUTPUT_FILE_2

if grep -q "WARNING: ThreadSanitizer:" $OUTPUT_FILE_2; then
    echo "Data races detected."
    exit 1
fi

# Check if "Saved results." is in the output, else fail
if ! grep -q "Saved results." $OUTPUT_FILE_2; then
    echo "Failed to save results."
    exit 1
fi

# If the output contains "illegal move" then fail
if grep -q "illegal move" $OUTPUT_FILE_2; then
    echo "Illegal move detected."
    exit 1
fi

# If the output contains "disconnects" then fail
if grep -q "disconnects" $OUTPUT_FILE_2; then
    echo "Disconnect detected."
    exit 1
fi

# If the output contains "loses on time" then fail
if grep -q "loses on time" $OUTPUT_FILE_2; then
    echo "Loses on time detected."
    exit 1
fi

# Fixed time-per-move test

OUTPUT_FILE_3=$(mktemp)
./fast-chess -engine cmd=random_mover name=random_move_1 -engine cmd=random_mover name=random_move_2 \
    -each st=1 -rounds 5 -repeat -concurrency 2 \
    -openings file=app/tests/data/openings.pgn format=pgn order=random -log file=log.txt level=info  2>&1 | tee $OUTPUT_FILE_3

if grep -q "WARNING: ThreadSanitizer:" $OUTPUT_FILE_3; then
    echo "Data races detected."
    exit 1
fi

# Check if "Saved results." is in the output, else fail
if ! grep -q "Saved results." $OUTPUT_FILE_3; then
    echo "Failed to save results."
    exit 1
fi

# If the output contains "illegal move" then fail
if grep -q "illegal move" $OUTPUT_FILE_3; then
    echo "Illegal move detected."
    exit 1
fi

# If the output contains "disconnects" then fail
if grep -q "disconnects" $OUTPUT_FILE_3; then
    echo "Disconnect detected."
    exit 1
fi

# If the output contains "loses on time" then fail
if grep -q "loses on time" $OUTPUT_FILE_3; then
    echo "Loses on time detected."
    exit 1
fi
