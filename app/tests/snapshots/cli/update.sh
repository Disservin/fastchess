#!/bin/bash

set -e

UPDATE_SNAPSHOTS=1 bash app/tests/snapshots/cli/snapshot.sh "$@"
