#!/bin/bash

set -e
set -x

source app/tests/e2e/lib.sh

setup_e2e "$@"

rm -f plies_test.pgn

output_file=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 3 -repeat -concurrency 2 \
    -openings file=./app/tests/data/openings.pgn format=pgn order=sequential plies=4 \
    -pgnout file=plies_test.pgn -log file=log.txt level=info 2>&1 | tee "$output_file"
check_no_engine_runtime_errors "$output_file"

if ! grep -q "Finished game 6" "$output_file"; then
    echo "Not all 6 games were played."
    exit 1
fi

if [ ! -f "plies_test.pgn" ]; then
    echo "plies_test.pgn file was not created."
    exit 1
fi

game_count=$(grep -c "^\[Event " plies_test.pgn)
if [ "$game_count" -ne 6 ]; then
    echo "Expected 6 games in plies_test.pgn, but found $game_count."
    exit 1
fi

book_count=$(grep -o "{book}" plies_test.pgn | wc -l)
echo "Found $book_count {book} annotations in PGN file"
if [ "$book_count" -ne 24 ]; then
    echo "Expected exactly 24 {book} annotations (6 games * 4 plies), but found $book_count."
    exit 1
fi

if ! grep -q "{book} [0-9]\+\." plies_test.pgn; then
    echo "PGN does not show engine moves after book moves."
    exit 1
fi

echo "PGN book plies option works correctly."
