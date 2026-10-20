#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT=results/benchmark
SCALES="${SCALES:-standard large}"

for scale in $SCALES; do
  dir="$OUT/$scale"
  shards=("$dir"/raw_runs.*.tsv)
  if [ ! -e "${shards[0]}" ]; then
    echo "[$scale] no shards to merge; leaving raw_runs.tsv as it is"
    continue
  fi

  merged="$dir/raw_runs.tsv"
  echo "[$scale] merging ${#shards[@]} shard(s)"
  head -1 "${shards[0]}" > "$merged"
  for f in "${shards[@]}"; do
    tail -n +2 "$f" >> "$merged"
  done

  dup=$(awk -F'\t' 'NR>1{print $1"\t"$2"\t"$7}' "$merged" | sort | uniq -d | head -5)
  if [ -n "$dup" ]; then
    echo "[$scale] ERROR: duplicate (algorithm, dataset, seed) rows -- shards overlap:" >&2
    echo "$dup" >&2
    exit 1
  fi

  want=$(./bin/bc-ours --list | awk -F'\t' -v s="$scale" '$3==s{print $1}' | sort)
  have=$(awk -F'\t' 'NR>1{print $2}' "$merged" | sort -u)
  missing=$(comm -23 <(echo "$want") <(echo "$have") | tr '\n' ' ')
  if [ -n "${missing// /}" ]; then
    echo "[$scale] ERROR: no rows for: $missing" >&2
    exit 1
  fi

  echo "[$scale] $(( $(wc -l < "$merged") - 1 )) rows over $(echo "$have" | wc -l) datasets"
  echo "[$scale] paired tests"
  python3 tools/stats/analyze.py "$merged" --ours HBC --out "$dir/stats"
done

python3 experiments/benchmark/tables.py
python3 experiments/benchmark/significance.py
echo "merged -> $OUT/<scale>/raw_runs.tsv"
