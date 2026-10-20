#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
RUNS="${RUNS:-10}"
OUT="${OUT:-results/bbc}"
RHOS="${RHOS:-0 0.01 0.03 0.05 0.10 0.25 0.50 1.00}"

RESUME="${RESUME:-0}"
resume_args=()
[ "$RESUME" = 1 ] && resume_args+=(--resume)

for scale in ${SCALES:-standard large}; do
  dir="$OUT/$scale"; raw="$dir/raw_runs.tsv"
  mkdir -p "$dir/shards"
  [ "$RESUME" = 1 ] || rm -f "$raw" "$dir"/shards/*.tsv

  for rho in $RHOS; do
    echo "[$scale] BBC rho=$rho"
    ./bin/bc-ours --runs "$RUNS" --scale "$scale" --rho "$rho" "${resume_args[@]}" \
        --out "$dir/ours_rho${rho}.txt" --raw "$dir/shards/ours_rho${rho}.tsv"
  done
  for rho in $RHOS; do
    echo "[$scale] TBC (repaired) rho=$rho"
    ./bin/tbc --runs "$RUNS" --scale "$scale" --tbc-mode repaired "${resume_args[@]}" \
        --rho "$rho" --out "$dir" --raw "$dir/shards/tbc_rho${rho}.tsv"
  done

  first=1; : > "$raw"
  for f in "$dir"/shards/*.tsv; do
    if [ $first = 1 ]; then cat "$f" >> "$raw"; first=0
    else tail -n +2 "$f" >> "$raw"; fi
  done
done
echo "raw records -> $OUT/<scale>/raw_runs.tsv"
