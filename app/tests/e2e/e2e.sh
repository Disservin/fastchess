#!/bin/bash

set -e
set -x

source app/tests/e2e/lib.sh

setup_e2e "$@"

export E2E_SKIP_BUILD=1

bash app/tests/e2e/opening_books.sh
bash app/tests/e2e/engine_diagnostics.sh
bash app/tests/e2e/rating_interval.sh
bash app/tests/e2e/pgn_book_plies.sh
bash app/tests/e2e/random_order.sh
bash app/tests/e2e/black_to_move.sh
