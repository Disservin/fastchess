#!/bin/bash

set -e
set -x

source app/tests/e2e/lib.sh

setup_e2e "$@"

output_file=$(mktemp)
./fastchess -engine cmd=./scripted_result name=black_winner -engine cmd=./scripted_result args=--illegal name=white_loser \
    -each nodes=1 -rounds 1 -games 1 -concurrency 1 -ratinginterval 1 -report penta=false \
    -openings file=./app/tests/data/black-to-move.epd format=epd order=sequential -log file=log.txt level=info 2>&1 | tee "$output_file"

if grep -q "WARNING: ThreadSanitizer:" "$output_file"; then
    echo "Data races detected."
    exit 1
fi

if ! grep -q "Finished game 1 (black_winner vs white_loser): 1-0 {Black makes an illegal move}" "$output_file"; then
    echo "Black-to-move game did not finish with the expected black loss."
    exit 1
fi

if ! grep -q "Results of black_winner vs white_loser" "$output_file"; then
    echo "Black-to-move result report was not printed."
    exit 1
fi

if ! grep -q "Games: 1, Wins: 1, Losses: 0, Draws: 0" "$output_file"; then
    echo "Black-to-move win was not credited to the assignment-first engine."
    exit 1
fi

echo "Black-to-move scoring test passed."
