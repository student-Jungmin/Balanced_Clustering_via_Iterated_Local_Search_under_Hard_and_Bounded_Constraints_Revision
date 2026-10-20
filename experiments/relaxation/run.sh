#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
RUNS="${RUNS:-10}"
SCALE="${SCALE:-standard}"
RHOS="${RHOS:-0.01 0.03 0.05 0.10 0.25 0.50 1.00}"
RESUME="${RESUME:-0}"
resume_args=()
[ "$RESUME" = 1 ] && resume_args+=(--resume)
OUT=results/relaxation
LABELS="$OUT/labels"
BUDGET="${BUDGET:-results/benchmark/$SCALE/ours_summary.txt}"
[ -s "$BUDGET" ] || { echo "missing $BUDGET - run experiments/benchmark/run.sh first" >&2; exit 1; }
if [ "$RESUME" != 1 ] && [ -e "$OUT" ]; then
  [ "${FORCE:-0}" = 1 ] || { echo "$OUT exists: RESUME=1 to continue it, FORCE=1 to replace it" >&2; exit 1; }
  rm -rf "$OUT"
fi
mkdir -p "$LABELS"

echo "[labels] ours"
./bin/bc-ours --runs "$RUNS" --scale "$SCALE" --hard-tau on --out "$OUT/ours.txt" \
    --raw "$OUT/raw_runs.tsv" "${resume_args[@]}" --labels "$LABELS" >/dev/null
for b in rkm bkm tbc mabmssc bkmpp lima sobmssc; do
  echo "[labels] $b"
  extra=()
  case "$b" in
    lima) extra=(--budget "$BUDGET") ;;
    mabmssc) extra=(--budget "$BUDGET" --scale-factor 1 --scale-factor 100) ;;
  esac
  [ "$b" = tbc ] && extra=(--tbc-mode repaired)
  ./bin/$b --runs "$RUNS" --scale "$SCALE" --out "$OUT" --raw "$OUT/raw_runs.tsv" "${resume_args[@]}" \
      --labels "$LABELS" "${extra[@]}" >/dev/null
done

echo "[relax] applying tauConversion to every dumped solution"
parts="$OUT/transfer.d"; mkdir -p "$parts"
[ "$RESUME" = 1 ] || rm -f "$parts"/*.tsv "$parts"/.header
for f in "$LABELS"/*.txt; do
  base=$(basename "$f" .txt)
  algo=${base%%_*}; rest=${base#*_}
  case "$rest" in repaired_*) algo="${algo}_repaired"; rest=${rest#repaired_} ;; esac
  ds=${rest%_seed*}
  for rho in $RHOS; do
    part="$parts/${base}__rho${rho}.tsv"
    [ -s "$part" ] && continue
    ./bin/bc-tau --dataset "$ds" --labels "$f" --rho "$rho" > "$part.tmp"
    [ -s "$parts/.header" ] || \
      awk 'NR==1{print "algorithm\t"$0}' "$part.tmp" > "$parts/.header"
    awk -v a="$algo" 'NR==2{print a"\t"$0}' "$part.tmp" > "$part.new"
    mv "$part.new" "$part"; rm -f "$part.tmp"
  done
done
out="$OUT/transfer.tsv"
{ cat "$parts/.header"; find "$parts" -name '*.tsv' -print0 | sort -z | xargs -0 cat; } > "$out"
echo "transfer records -> $out"
