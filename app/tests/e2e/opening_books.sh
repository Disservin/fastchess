#!/bin/bash

set -e
set -x

source app/tests/e2e/lib.sh

setup_e2e "$@"

output_file=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 2 \
    -openings file=./app/tests/data/openings.epd format=epd order=random -log file=log.txt level=info 2>&1 | tee "$output_file"
check_no_engine_runtime_errors "$output_file"

output_file=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 2 \
    -openings file=app/tests/data/openings.pgn format=pgn order=random -log file=log.txt level=info 2>&1 | tee "$output_file"
check_no_engine_runtime_errors "$output_file"

rm -f games.pgn

output_file=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 10 -repeat -concurrency 2 \
    -openings file=./app/tests/data/short.epd format=epd order=random -pgnout file=games.pgn -log file=log.txt level=info 2>&1 | tee "$output_file"
check_no_engine_runtime_errors "$output_file"

if ! grep -q "Finished game 20" "$output_file"; then
    echo "Not all 20 games were played."
    exit 1
fi

if [ ! -f "games.pgn" ]; then
    echo "games.pgn file was not created."
    exit 1
fi

game_count=$(grep -c "^\[Event " games.pgn)
if [ "$game_count" -ne 20 ]; then
    echo "Expected 20 games in games.pgn, but found $game_count."
    exit 1
fi

opening1_count=$(grep -c "rnbq1rk1/p3bppp/2pp1n2/1p6/3NPB2/2N5/PPPQ1PPP/2KR1B1R w - - 0 9" games.pgn)
opening2_count=$(grep -c "r1bq1rk1/2ppbppp/p4n2/np2p3/4P3/1BNP1N2/PPP2PPP/R1BQ1RK1 w - - 1 9" games.pgn)
opening3_count=$(grep -c "rnbq1rk1/pp3pbp/3p2p1/2pPp2n/2P1P3/2NB1N2/PP3PPP/R1BQ1RK1 w - - 2 9" games.pgn)
opening4_count=$(grep -c "rnb1kb1r/pp1pqp2/7p/2ppp3/8/2N2N2/PPP2PPP/R2QKB1R w KQkq - 0 9" games.pgn)
opening5_count=$(grep -c "r2qkb1r/pp1n1ppp/2pp1n2/8/3P2b1/2N2N2/PPP1BPPP/R1BQ1RK1 w kq - 2 9" games.pgn)

echo "Opening usage counts:"
echo "  Opening 1: $opening1_count"
echo "  Opening 2: $opening2_count"
echo "  Opening 3: $opening3_count"
echo "  Opening 4: $opening4_count"
echo "  Opening 5: $opening5_count"

expected_count=4
if [ "$opening1_count" -ne "$expected_count" ] || [ "$opening2_count" -ne "$expected_count" ] || \
   [ "$opening3_count" -ne "$expected_count" ] || [ "$opening4_count" -ne "$expected_count" ] || \
   [ "$opening5_count" -ne "$expected_count" ]; then
    echo "Opening distribution is not as expected. Each opening should appear exactly $expected_count times."
    exit 1
fi

echo "Opening book tests passed."
