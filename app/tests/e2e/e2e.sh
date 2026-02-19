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
    -each tc=2+0.02s option.Hash=-16 option.Threads=2 -rounds 5 -repeat -concurrency 1 \
    -openings file=app/tests/data/openings.pgn format=pgn order=random -log file=log.txt level=warn  2>&1 | tee $OUTPUT_FILE_4


# check if the output contains the expected error message
if ! grep -q "Fatal; random_move_1 engine startup failure: \"Engine didn't respond to uciok after startup\"" $OUTPUT_FILE_4; then
    echo "Failed to report invalid command."
    exit 1
fi

# Short EPD Book Test - More rounds than openings
# short.epd has only 5 openings, but we play 10 rounds (20 games with -repeat)
# This tests that the opening book cycles/repeats correctly
rm games.pgn

OUTPUT_FILE_5=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 10 -repeat -concurrency 2 \
    -openings file=./app/tests/data/short.epd format=epd order=random -pgnout file=games.pgn -log file=log.txt level=info 2>&1 | tee $OUTPUT_FILE_5

if grep -q "WARNING: ThreadSanitizer:" $OUTPUT_FILE_5; then
    echo "Data races detected."
    exit 1
fi

# If the output contains "illegal move" then fail
if grep -q "illegal move" $OUTPUT_FILE_5; then
    echo "Illegal move detected."
    exit 1
fi

# If the output contains "disconnects" then fail
if grep -q "disconnects" $OUTPUT_FILE_5; then
    echo "Disconnect detected."
    exit 1
fi

# If the output contains "stalls" then fail
if grep -q "stalls" $OUTPUT_FILE_5; then
    echo "Stall detected."
    exit 1
fi

# If the output contains "loses on time" then fail
if grep -q "loses on time" $OUTPUT_FILE_5; then
    echo "Loses on time detected."
    exit 1
fi

# Verify that all 20 games were played (10 rounds * 2 games per round with -repeat)
if ! grep -q "Finished game 20" $OUTPUT_FILE_5; then
    echo "Not all 20 games were played."
    exit 1
fi

# Verify that the games.pgn file was created and contains games
if [ ! -f "games.pgn" ]; then
    echo "games.pgn file was not created."
    exit 1
fi

# Count the number of games in the PGN file (each game starts with "[Event ")
game_count=$(grep -c "^\[Event " games.pgn)
if [ "$game_count" -ne 20 ]; then
    echo "Expected 20 games in games.pgn, but found $game_count."
    exit 1
fi

# Extract FENs from games.pgn and verify they match the short.epd openings
# The FEN should appear in the "[Setup \"1\"]" and "[FEN \"...\"]" tags
# Since we have 5 openings and 20 games, each opening should be used 4 times (20/5=4)

# Count occurrences of each opening FEN in the PGN file
# Opening 1: rnbq1rk1/p3bppp/2pp1n2/1p6/3NPB2/2N5/PPPQ1PPP/2KR1B1R w - b6 0 9
opening1_count=$(grep -c "rnbq1rk1/p3bppp/2pp1n2/1p6/3NPB2/2N5/PPPQ1PPP/2KR1B1R w - b6 0 9" games.pgn)
# Opening 2: r1bq1rk1/2ppbppp/p4n2/np2p3/4P3/1BNP1N2/PPP2PPP/R1BQ1RK1 w - - 1 9
opening2_count=$(grep -c "r1bq1rk1/2ppbppp/p4n2/np2p3/4P3/1BNP1N2/PPP2PPP/R1BQ1RK1 w - - 1 9" games.pgn)
# Opening 3: rnbq1rk1/pp3pbp/3p2p1/2pPp2n/2P1P3/2NB1N2/PP3PPP/R1BQ1RK1 w - - 2 9
opening3_count=$(grep -c "rnbq1rk1/pp3pbp/3p2p1/2pPp2n/2P1P3/2NB1N2/PP3PPP/R1BQ1RK1 w - - 2 9" games.pgn)
# Opening 4: rnb1kb1r/pp1pqp2/7p/2ppp3/8/2N2N2/PPP2PPP/R2QKB1R w KQkq - 0 9
opening4_count=$(grep -c "rnb1kb1r/pp1pqp2/7p/2ppp3/8/2N2N2/PPP2PPP/R2QKB1R w KQkq - 0 9" games.pgn)
# Opening 5: r2qkb1r/pp1n1ppp/2pp1n2/8/3P2b1/2N2N2/PPP1BPPP/R1BQ1RK1 w kq - 2 9
opening5_count=$(grep -c "r2qkb1r/pp1n1ppp/2pp1n2/8/3P2b1/2N2N2/PPP1BPPP/R1BQ1RK1 w kq - 2 9" games.pgn)

echo "Opening usage counts:"
echo "  Opening 1: $opening1_count"
echo "  Opening 2: $opening2_count"
echo "  Opening 3: $opening3_count"
echo "  Opening 4: $opening4_count"
echo "  Opening 5: $opening5_count"

# With random order, each opening should appear exactly 4 times (20 games / 5 openings = 4 each)
expected_count=4
if [ "$opening1_count" -ne "$expected_count" ] || [ "$opening2_count" -ne "$expected_count" ] || \
   [ "$opening3_count" -ne "$expected_count" ] || [ "$opening4_count" -ne "$expected_count" ] || \
   [ "$opening5_count" -ne "$expected_count" ]; then
    echo "Opening distribution is not as expected. Each opening should appear exactly $expected_count times."
    exit 1
fi

echo "All openings were played the expected number of times."

# Rating Interval Test with penta=false
# Test that ratinginterval output works correctly - should print rating report every N games
# With -ratinginterval 2 and -report penta=false, we should see rating output after every 2 games: 2, 4, 6, 8, 10

OUTPUT_FILE_6=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 2 -ratinginterval 2 -report penta=false \
    -openings file=./app/tests/data/openings.epd format=epd order=random -log file=log.txt level=info 2>&1 | tee $OUTPUT_FILE_6

if grep -q "WARNING: ThreadSanitizer:" $OUTPUT_FILE_6; then
    echo "Data races detected."
    exit 1
fi

# If the output contains "illegal move" then fail
if grep -q "illegal move" $OUTPUT_FILE_6; then
    echo "Illegal move detected."
    exit 1
fi

# If the output contains "disconnects" then fail
if grep -q "disconnects" $OUTPUT_FILE_6; then
    echo "Disconnect detected."
    exit 1
fi

# If the output contains "stalls" then fail
if grep -q "stalls" $OUTPUT_FILE_6; then
    echo "Stall detected."
    exit 1
fi

# If the output contains "loses on time" then fail
if grep -q "loses on time" $OUTPUT_FILE_6; then
    echo "Loses on time detected."
    exit 1
fi

# Verify that all 10 games were played (5 rounds * 2 games per round with -repeat)
if ! grep -q "Finished game 10" $OUTPUT_FILE_6; then
    echo "Not all 10 games were played."
    exit 1
fi

# Check that rating reports were printed
# With -ratinginterval 2 and -report penta=false, rating reports are printed every 2 games
# Count occurrences of "Results of" which indicates rating interval output
rating_report_count=$(grep -c "Results of" $OUTPUT_FILE_6)
echo "Found $rating_report_count rating reports"
# We expect exactly 5 rating reports (after games 2, 4, 6, 8, 10)
if [ "$rating_report_count" -ne 5 ]; then
    echo "Expected exactly 5 rating interval reports (at games 2, 4, 6, 8, 10), but found $rating_report_count."
    exit 1
fi

# Check that Elo information is being printed in the rating reports
if ! grep -q "Elo:" $OUTPUT_FILE_6; then
    echo "Rating interval output does not contain Elo information."
    exit 1
fi

# Check that the rating reports contain game statistics
if ! grep -q "Games:" $OUTPUT_FILE_6; then
    echo "Rating interval output does not contain game statistics."
    exit 1
fi

echo "Rating interval output with penta=false works correctly."

# Rating Interval Test with penta=true (default)
# Test that ratinginterval output works correctly with pentanomial reporting
# With -ratinginterval 2 and -report penta=true, reports are printed when pairs complete

OUTPUT_FILE_7=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 2 -ratinginterval 2 -report penta=true \
    -openings file=./app/tests/data/openings.epd format=epd order=random -log file=log.txt level=info 2>&1 | tee $OUTPUT_FILE_7

if grep -q "WARNING: ThreadSanitizer:" $OUTPUT_FILE_7; then
    echo "Data races detected."
    exit 1
fi

# If the output contains "illegal move" then fail
if grep -q "illegal move" $OUTPUT_FILE_7; then
    echo "Illegal move detected."
    exit 1
fi

# If the output contains "disconnects" then fail
if grep -q "disconnects" $OUTPUT_FILE_7; then
    echo "Disconnect detected."
    exit 1
fi

# If the output contains "stalls" then fail
if grep -q "stalls" $OUTPUT_FILE_7; then
    echo "Stall detected."
    exit 1
fi

# If the output contains "loses on time" then fail
if grep -q "loses on time" $OUTPUT_FILE_7; then
    echo "Loses on time detected."
    exit 1
fi

# Verify that all 10 games were played (5 rounds * 2 games per round with -repeat)
if ! grep -q "Finished game 10" $OUTPUT_FILE_7; then
    echo "Not all 10 games were played."
    exit 1
fi

# Check that rating reports were printed
# With -ratinginterval 2 and -report penta=true, rating reports are printed when pairs complete
# Count occurrences of "Results of" which indicates rating interval output
rating_report_count_penta=$(grep -c "Results of" $OUTPUT_FILE_7)
echo "Found $rating_report_count_penta rating reports with penta=true"
# We expect exactly 3 rating reports (after pairs complete)
if [ "$rating_report_count_penta" -ne 3 ]; then
    echo "Expected exactly 3 rating interval reports with penta=true, but found $rating_report_count_penta."
    exit 1
fi

# Check that Elo information is being printed in the rating reports
if ! grep -q "Elo:" $OUTPUT_FILE_7; then
    echo "Rating interval output does not contain Elo information."
    exit 1
fi

# Check that the rating reports contain game statistics
if ! grep -q "Games:" $OUTPUT_FILE_7; then
    echo "Rating interval output does not contain game statistics."
    exit 1
fi

# Check that pentanomial statistics are present
if ! grep -q "Ptnml" $OUTPUT_FILE_7; then
    echo "Rating interval output with penta=true does not contain Ptnml statistics."
    exit 1
fi

echo "Rating interval output with penta=true works correctly."
