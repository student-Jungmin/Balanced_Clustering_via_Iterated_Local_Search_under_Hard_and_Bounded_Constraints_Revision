#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

RUNS="${RUNS:-10}"
SCALES="${SCALES:-standard large}"
RESUME="${RESUME:-0}"
resume_args=()
[ "$RESUME" = 1 ] && resume_args+=(--resume)

OUT=results/acceptance
BIN="${BIN:-./bin/bc-ours}"
[ -x "$BIN" ] || { echo "build first: make ours" >&2; exit 1; }

for scale in $SCALES; do
  dir="$OUT/$scale"
  mkdir -p "$dir"

  for arm in randomwalk better better_stall3 better_stall10; do
    raw="$dir/raw_$arm.tsv"
    [ "$RESUME" = 1 ] || rm -f "$raw"
    case "$arm" in
      randomwalk)      args=(--accept randomwalk) ;;
      better)          args=(--accept better) ;;
      better_stall*)   args=(--accept better --stall "${arm#better_stall}") ;;
    esac
    echo "[$scale] $arm$([ "$arm" = randomwalk ] && echo ' (published)')"
    "$BIN" --runs "$RUNS" --scale "$scale" "${args[@]}" \
        --out "$dir/$arm.txt" --raw "$raw" "${resume_args[@]}"
  done
done

echo "raw records -> $OUT/<scale>/raw_<arm>.tsv"
