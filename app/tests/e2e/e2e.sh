#!/bin/bash 

set -x

# Compile the random_mover
g++ -O3 -std=c++17 app/tests/mock/engine/random_mover.cpp -o random_mover

# Compile fastchess
make -j build=debug $1

# EPD Book Test

OUTPUT_FILE=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 2 \
    -openings file=./app/tests/data/openings.epd format=epd order=random -log file=log.txt level=info 2>&1 | tee $OUTPUT_FILE

if grep -q "WARNING: ThreadSanitizer:" $OUTPUT_FILE; then
    echo "Data races detected."
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

# If the output contains "stalls" then fail
if grep -q "stalls" $OUTPUT_FILE; then
    echo "Stall detected."
    exit 1
fi

# If the output contains "loses on time" then fail
if grep -q "loses on time" $OUTPUT_FILE; then
    echo "Loses on time detected."
    exit 1
fi

# PGN Book Test

OUTPUT_FILE_2=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 2 \
    -openings file=app/tests/data/openings.pgn format=pgn order=random -log file=log.txt level=info  2>&1 | tee $OUTPUT_FILE_2

if grep -q "WARNING: ThreadSanitizer:" $OUTPUT_FILE_2; then
    echo "Data races detected."
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

# If the output contains "stalls" then fail
if grep -q "stalls" $OUTPUT_FILE_2; then
    echo "Stall detected."
    exit 1
fi

# If the output contains "loses on time" then fail
if grep -q "loses on time" $OUTPUT_FILE_2; then
    echo "Loses on time detected."
    exit 1
fi


# Invalid UciOptions Test

OUTPUT_FILE_3=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s option.Hash=-16 option.Threads=2 -rounds 5 -repeat -concurrency 2 \
    -openings file=app/tests/data/openings.pgn format=pgn order=random -log file=log.txt level=info  2>&1 | tee $OUTPUT_FILE_3

if ! grep -q "Warning; random_move_1 doesn't have option Threads" $OUTPUT_FILE_3; then
    echo "Failed to report missing option Threads."
    exit 1
fi

if ! grep -q "Warning; Invalid value for option Hash; -16" $OUTPUT_FILE_3; then
    echo "Failed to report invalid value for option Hash."
    exit 1
fi


if grep -q "WARNING: ThreadSanitizer:" $OUTPUT_FILE_3; then
    echo "Data races detected."
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

# If the output contains "stalls" then fail
if grep -q "stalls" $OUTPUT_FILE_3; then
    echo "Stall detected."
    exit 1
fi

# If the output contains "loses on time" then fail
if grep -q "loses on time" $OUTPUT_FILE_3; then
    echo "Loses on time detected."
    exit 1
fi


# Non UCI responding engine
# Only continue if linux or mac

if [[ "$OSTYPE" != "linux-gnu" && "$OSTYPE" != "darwin"* ]]; then
    echo "Skipping non-uci responding engine test on non-linux/macOS systems."
    exit 0
fi

OUTPUT_FILE_4=$(mktemp)
./fastchess -engine cmd=app/tests/mock/engine/missing_engine.sh name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s option.Hash=-16 option.Threads=2 -rounds 5 -repeat -concurrency 2 \
    -openings file=app/tests/data/openings.pgn format=pgn order=random -log file=log.txt level=warn  2>&1 | tee $OUTPUT_FILE_4


# check if the output contains the expected error message
if ! grep -q "Fatal; random_move_1 engine startup failure: \"Engine didn't respond to uciok after startup\"" $OUTPUT_FILE_4; then
    echo "Failed to report invalid command."
    exit 1
fi
