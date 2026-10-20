#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
RUNS="${RUNS:-10}"
OUT=results/optimization
[ -x ./bin/bc-ours ] || { echo "build first: make ours" >&2; exit 1; }
for scale in standard large; do
  for ranking in full prefix; do
    side=$([ "$ranking" = full ] && echo before || echo after)
    dir="$OUT/$side/$scale"; mkdir -p "$dir"
    echo "[$scale] ranking=$ranking"
    ./bin/bc-ours --runs "$RUNS" --scale "$scale" --ranking "$ranking" \
        --out "$dir/summary.txt" --raw "$dir/raw_runs.tsv" >/dev/null
  done
done
echo "raw records -> $OUT/{before,after}/<scale>/"
