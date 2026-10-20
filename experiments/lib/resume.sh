stage_complete() {
  local raw="$1" runs="$2" scale="$3"
  [ "${RESUME:-0}" = 1 ] || return 1
  [ -f "$raw" ] || return 1
  local want have
  want=$(( runs * $(./bin/bc-ours --list | awk -F'\t' -v s="$scale" '$3==s' | wc -l) ))
  have=$(( $(wc -l < "$raw") - 1 ))
  [ "$have" -ge "$want" ]
}
