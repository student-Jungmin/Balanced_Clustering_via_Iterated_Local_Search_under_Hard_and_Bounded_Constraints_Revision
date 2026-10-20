#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
RUNS="${RUNS:-10}"
OUT=results/benchmark

WITH_MABMSSC="${WITH_MABMSSC:-1}"
MABMSSC_FACTORS="${MABMSSC_FACTORS:-1 100}"
for b in bc-ours rkm bkm tbc bkmpp lima mabmssc; do
  [ -x "./bin/$b" ] || { echo "build first: make" >&2; exit 1; }
done

SCALES="${SCALES:-standard large}"
DATASETS="${DATASETS:-}"
SHARD="${SHARD:-}"

RESUME="${RESUME:-0}"
resume_args=()
[ "$RESUME" = 1 ] && resume_args+=(--resume)

ds_args=()
if [ -n "$DATASETS" ]; then
  while IFS= read -r d; do [ -n "$d" ] && ds_args+=(--dataset "$d"); done <<< "$DATASETS"
fi

for scale in $SCALES; do
  dir="$OUT/$scale"
  raw="$dir/raw_runs${SHARD:+.$SHARD}.tsv"
  mkdir -p "$dir"
  [ "$RESUME" = 1 ] || rm -f "$raw"

  echo "[$scale] HBC"
  ./bin/bc-ours --runs "$RUNS" --scale "$scale" --hard-tau on "${ds_args[@]}" "${resume_args[@]}" --out "$dir/ours_summary.txt" --raw "$raw"

  echo "[$scale] rkm"
  ./bin/rkm --runs "$RUNS" --scale "$scale" "${ds_args[@]}" "${resume_args[@]}" --out "$dir" --raw "$raw"

  for mode in published repaired; do
    echo "[$scale] tbc ($mode)"
    ./bin/tbc --runs "$RUNS" --scale "$scale" "${ds_args[@]}" "${resume_args[@]}" --tbc-mode "$mode" \
        --out "$dir" --raw "$raw"
  done

  if [ "$WITH_MABMSSC" = 1 ] && [ "$scale" = standard ]; then
    sf=(); for f in $MABMSSC_FACTORS; do sf+=(--scale-factor "$f"); done
    echo "[$scale] mabmssc (x{$(echo $MABMSSC_FACTORS | tr ' ' ,)})"
    ./bin/mabmssc --runs "$RUNS" --scale "$scale" "${ds_args[@]}" "${resume_args[@]}" --out "$dir" --raw "$raw" \
        --budget "$dir/ours_summary.txt" "${sf[@]}"
  else
    echo "[$scale] mabmssc SKIPPED (WITH_MABMSSC=0)"
  fi

  echo "[$scale] bkmpp"
  ./bin/bkmpp --runs "$RUNS" --scale "$scale" "${ds_args[@]}" "${resume_args[@]}" --out "$dir" --raw "$raw"

  if [ "$scale" = standard ]; then
    echo "[$scale] bkm"
    ./bin/bkm --runs "$RUNS" --scale "$scale" "${ds_args[@]}" "${resume_args[@]}" --out "$dir" --raw "$raw"
    echo "[$scale] sobmssc"
    ./bin/sobmssc --runs "$RUNS" --scale "$scale" "${ds_args[@]}" "${resume_args[@]}" --out "$dir" --raw "$raw"
    echo "[$scale] lima (x1 and x100)"
    ./bin/lima --runs "$RUNS" --scale "$scale" "${ds_args[@]}" "${resume_args[@]}" --out "$dir" --raw "$raw" \
        --budget "$dir/ours_summary.txt"
  fi
  if [ -z "$SHARD" ]; then
    echo "[$scale] paired tests"
    python3 tools/stats/analyze.py "$raw" --ours HBC --out "$dir/stats"
  else
    echo "[$scale] shard $SHARD written; paired tests deferred to merge.sh"
  fi
done
echo "raw records -> $OUT/<scale>/raw_runs.tsv"
