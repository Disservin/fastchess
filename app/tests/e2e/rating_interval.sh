#!/bin/bash

set -e
set -x

source app/tests/e2e/lib.sh

setup_e2e "$@"

output_file=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 2 -ratinginterval 2 -report penta=false \
    -openings file=./app/tests/data/openings.epd format=epd order=random -log file=log.txt level=info 2>&1 | tee "$output_file"
check_no_engine_runtime_errors "$output_file"

if ! grep -q "Finished game 10" "$output_file"; then
    echo "Not all 10 games were played."
    exit 1
fi

rating_report_count=$(grep -c "Results of" "$output_file")
echo "Found $rating_report_count rating reports"
if [ "$rating_report_count" -ne 5 ]; then
    echo "Expected exactly 5 rating interval reports (at games 2, 4, 6, 8, 10), but found $rating_report_count."
    exit 1
fi

if ! grep -q "Elo:" "$output_file"; then
    echo "Rating interval output does not contain Elo information."
    exit 1
fi

if ! grep -q "Games:" "$output_file"; then
    echo "Rating interval output does not contain game statistics."
    exit 1
fi

output_file=$(mktemp)
./fastchess -engine cmd=./random_mover name=random_move_1 -engine cmd=./random_mover name=random_move_2 \
    -each tc=2+0.02s -rounds 5 -repeat -concurrency 1 -ratinginterval 2 -report penta=true \
    -openings file=./app/tests/data/openings.epd format=epd order=random -log file=log.txt level=info 2>&1 | tee "$output_file"
check_no_engine_runtime_errors "$output_file"

if ! grep -q "Finished game 10" "$output_file"; then
    echo "Not all 10 games were played."
    exit 1
fi

rating_report_count_penta=$(grep -c "Results of" "$output_file")
echo "Found $rating_report_count_penta rating reports with penta=true"
if [ "$rating_report_count_penta" -ne 3 ]; then
    echo "Expected exactly 3 rating interval reports with penta=true, but found $rating_report_count_penta."
    exit 1
fi

if ! grep -q "Elo:" "$output_file"; then
    echo "Rating interval output does not contain Elo information."
    exit 1
fi

if ! grep -q "Games:" "$output_file"; then
    echo "Rating interval output does not contain game statistics."
    exit 1
fi

if ! grep -q "Ptnml" "$output_file"; then
    echo "Rating interval output with penta=true does not contain Ptnml statistics."
    exit 1
fi

echo "Rating interval tests passed."
