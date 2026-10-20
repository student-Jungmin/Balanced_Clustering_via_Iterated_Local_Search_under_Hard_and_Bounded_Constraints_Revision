#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
. "$(dirname "$0")/../lib/resume.sh"

RUNS="${RUNS:-10}"
OUT=results/calibration
[ -x ./bin/bc-ours ] || { echo "build first: make" >&2; exit 1; }

echo "[baseline] RKM, standard scale"
./bin/rkm --runs "$RUNS" --scale standard --out "$OUT/rkm" >/dev/null

for omega in 2 3 4 5 6 7 8 9; do
  for delta in 0.001 0.005 0.01 0.05 0.1; do
    if stage_complete "$OUT/sweep/w${omega}_d${delta}/raw_runs.tsv" "$RUNS" standard; then
      echo "[sweep] omega=$omega delta=$delta already complete, skipping"
      continue
    fi
    echo "[sweep] omega=$omega delta=$delta"
    ./bin/bc-ours --runs "$RUNS" --scale standard \
        --cv-window "$omega" --cv-threshold "$delta" \
        --out "$OUT/sweep/w${omega}_d${delta}/summary.txt" >/dev/null
  done
done
