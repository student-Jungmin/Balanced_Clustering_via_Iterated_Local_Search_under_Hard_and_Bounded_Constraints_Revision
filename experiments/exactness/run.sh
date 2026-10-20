#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
RUNS="${RUNS:-10}"
RESUME="${RESUME:-0}"
resume_args=()
if [ "$RESUME" = 1 ]; then
  echo "RESUME=1 is unsafe here: the arms share a raw file and --resume" >&2
  echo "cannot tell them apart. Rerun the stage from scratch." >&2
  exit 2
fi
OUT=results/exactness
[ -x ./bin/bc-ours ] || { echo "build first: make ours" >&2; exit 1; }

for scale in standard large; do
  dir="$OUT/$scale"; raw="$dir/raw_runs.tsv"
  mkdir -p "$dir"
  rm -f "$raw" "$dir/tau_raw.tsv"

  echo "[$scale] published"
  ./bin/bc-ours --runs "$RUNS" --scale "$scale" --hard-tau on --out "$dir/published.txt" --raw "$raw" "${resume_args[@]}"
  for v in centroids passcentroids pairs; do
    echo "[$scale] exact $v"
    ./bin/bc-ours --runs "$RUNS" --scale "$scale" --hard-tau on --exact "$v" \
        --out "$dir/exact_$v.txt" --raw "$raw" "${resume_args[@]}"
  done

done
echo "raw records -> $OUT/<scale>/"
