#!/usr/bin/env bash
# Usage: RUNS=10 experiments/run-all.sh [stage ...]
set -uo pipefail
cd "$(dirname "$0")/.."

RUNS="${RUNS:-10}"
LOG="${LOG:-results/run-all${SLURM_JOB_ID:+-$SLURM_JOB_ID}.log}"
mkdir -p "$(dirname "$LOG")"

FAILED=()

stage() {
  local name="$1"; shift
  echo "=== $name : $(date -Is) ===" | tee -a "$LOG"
  if "$@" >>"$LOG" 2>&1; then
    echo "    ok   $name : $(date -Is)" | tee -a "$LOG"
  else
    echo "    FAIL $name : $(date -Is)" | tee -a "$LOG"
    FAILED+=("$name")
  fi
}

echo "run-all: RUNS=$RUNS  host=$(hostname)  started $(date -Is)" | tee -a "$LOG"
echo "seeds: $(seq -s, 42 $((42 + RUNS - 1)))" | tee -a "$LOG"

WANT="${*:-}"
want() { [ -z "$WANT" ] || printf '%s\n' $WANT | grep -qx "$1"; }

stage verify-data      make verify-data

want exp-exactness && stage exp-exactness    make exp-exactness    RUNS="$RUNS"
want exp-acceptance && stage exp-acceptance   make exp-acceptance   RUNS="$RUNS"
want exp-runtime && stage exp-runtime      make exp-runtime      RUNS="$RUNS"
want exp-pruning && stage exp-pruning      make exp-pruning      RUNS="$RUNS"
want exp-convergence && stage exp-convergence  make exp-convergence  RUNS="$RUNS"
want exp-optimization && stage exp-optimization make exp-optimization RUNS="$RUNS"
want exp-calibration && stage exp-calibration  make exp-calibration  RUNS="$RUNS"

want exp-bbc && stage exp-bbc          make exp-bbc          RUNS="$RUNS"

want exp-benchmark && stage exp-benchmark    make exp-benchmark    RUNS="$RUNS"

want exp-feasibility && stage exp-feasibility  make exp-feasibility  RUNS="$RUNS"
want exp-relaxation && stage exp-relaxation   make exp-relaxation   RUNS="$RUNS"
want exp-comparison && stage exp-comparison   make exp-comparison

echo | tee -a "$LOG"
if [ ${#FAILED[@]} -eq 0 ]; then
  echo "run-all: all stages ok, finished $(date -Is)" | tee -a "$LOG"
else
  echo "run-all: ${#FAILED[@]} stage(s) FAILED: ${FAILED[*]}" | tee -a "$LOG"
  echo "run-all: finished $(date -Is)" | tee -a "$LOG"
  exit 1
fi
