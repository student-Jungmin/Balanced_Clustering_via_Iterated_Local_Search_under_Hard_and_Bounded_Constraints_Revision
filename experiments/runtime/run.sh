#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
RUNS="${RUNS:-10}"
BIN=./bin/bc-ours-analysis
[ -x "$BIN" ] || { echo "build first: make ours" >&2; exit 1; }
for scale in standard large; do
  echo "[runtime/$scale] runs=$RUNS (unoptimized variant)"
  $BIN --runs "$RUNS" --scale "$scale" --geo off --local-search adaptive \
      --out "results/runtime/$scale" >/dev/null
done
echo "raw records -> results/runtime/<scale>/phases.tsv"
