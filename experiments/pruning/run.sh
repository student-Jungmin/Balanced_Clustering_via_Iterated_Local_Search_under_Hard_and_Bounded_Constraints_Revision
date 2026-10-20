#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
. "$(dirname "$0")/../lib/resume.sh"

RUNS="${RUNS:-10}"
BIN=./bin/bc-ours-analysis
[ -x "$BIN" ] || { echo "build first: make ours" >&2; exit 1; }

run_variant() {
  for scale in standard large; do
    out="results/pruning/$1/$scale"
    if stage_complete "$out/raw_runs.tsv" "$RUNS" "$scale"; then
      echo "[$1/$scale] already complete, skipping"
      continue
    fi
    echo "[$1/$scale] runs=$RUNS"
    $BIN --runs "$RUNS" --scale "$scale" --geo "$2" --local-search "$3" --out "$out" >/dev/null
  done
}

run_variant HBC_NA off adaptive
run_variant HBC_NO off once
run_variant HBC_WG off sync
run_variant HBC_WI on  adaptive
run_variant HBC    on  sync
echo "raw records -> results/pruning/<variant>/<scale>/pairs.tsv"
