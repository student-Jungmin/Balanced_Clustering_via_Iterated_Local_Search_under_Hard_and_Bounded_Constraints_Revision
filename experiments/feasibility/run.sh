#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
RUNS="${RUNS:-10}"
RESUME="${RESUME:-0}"
resume_args=()
[ "$RESUME" = 1 ] && resume_args+=(--resume)
OUT=results/feasibility
mkdir -p "$OUT"
for scale in standard large; do
  raw="$OUT/$scale/raw_runs.tsv"; mkdir -p "$OUT/$scale"
  [ "$RESUME" = 1 ] || rm -f "$raw"
  echo "[$scale] TBC as published"
  ./bin/tbc --runs "$RUNS" --scale "$scale" --tbc-mode published \
      --rho 0 --rho 0.01 --rho 0.03 --rho 0.05 --rho 0.10 --rho 0.25 --rho 0.50 --rho 1.00 --out "$OUT/$scale" --raw "$raw" "${resume_args[@]}"
done
echo "raw records -> $OUT/<scale>/raw_runs.tsv"
