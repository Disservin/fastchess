#!/bin/bash

set -x

SNAPSHOT_DIR=app/tests/snapshots/cli

if [ "${E2E_SKIP_BUILD:-0}" != "1" ]; then
    g++ -O3 -std=c++17 app/tests/mock/engine/scripted_result.cpp -o scripted_result
    make -j build=debug $1
fi

assert_snapshot() {
    local actual_file=$1
    local expected_file=$2
    local normalized_actual
    local normalized_expected

    normalized_actual=$(mktemp)
    normalized_expected=$(mktemp)
    tr -d '\r' < "$actual_file" > "$normalized_actual"
    tr -d '\r' < "$expected_file" > "$normalized_expected"

    if ! diff -u "$normalized_expected" "$normalized_actual"; then
        echo "Snapshot mismatch: $expected_file"
        exit 1
    fi
}

normalize_version() {
    perl -pe 's/[0-9]{8}-[0-9a-f]+/<build>/g'
}

normalize_help_intro() {
    perl -CS -pe 's/\r$//g; s/\e\[[0-9;]*m//g; s/^    //; s/\x{00b7}/-/g; s/[ \t]+$//g' | awk 'NR <= 34 { print }'
}

normalize_actual_game() {
    perl -pe 's/\r$//g; s/[ \t]+$//g' | awk '/^Started game / || /^Warning; Illegal PV move/ || /^Info; / || /^Position; / || /^Moves;$/ || /^Warning; Illegal move/ || /^Finished game / || /^Results of / || /^Games: / || /^Finished match$/ { print }'
}

VERSION_OUTPUT=$(mktemp)
./fastchess -version | normalize_version > "$VERSION_OUTPUT"
assert_snapshot "$VERSION_OUTPUT" "$SNAPSHOT_DIR/version.txt"

HELP_OUTPUT=$(mktemp)
./fastchess -help | normalize_help_intro > "$HELP_OUTPUT"
assert_snapshot "$HELP_OUTPUT" "$SNAPSHOT_DIR/help_intro.txt"

UNKNOWN_OPTION_OUTPUT=$(mktemp)
./fastchess -engne > "$UNKNOWN_OPTION_OUTPUT" 2>&1 || true
assert_snapshot "$UNKNOWN_OPTION_OUTPUT" "$SNAPSHOT_DIR/unknown_option.txt"

ACTUAL_GAME_OUTPUT=$(mktemp)
./fastchess -engine cmd=./scripted_result name=black_winner -engine cmd=./scripted_result args=--illegal name=white_loser \
    -each nodes=1 -rounds 1 -games 1 -concurrency 1 -ratinginterval 1 -report penta=false \
    -openings file=./app/tests/data/black-to-move.epd format=epd order=sequential -log file=log.txt level=info 2>&1 \
    | normalize_actual_game > "$ACTUAL_GAME_OUTPUT"
assert_snapshot "$ACTUAL_GAME_OUTPUT" "$SNAPSHOT_DIR/actual_game_black_to_move.txt"
