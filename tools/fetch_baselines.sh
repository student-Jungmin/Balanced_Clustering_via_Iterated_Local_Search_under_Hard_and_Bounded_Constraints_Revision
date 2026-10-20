#!/usr/bin/env bash
# Usage: tools/fetch_baselines.sh [name ...]
set -euo pipefail
cd "$(dirname "$0")/.."
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

take() { cp "$1" "$2"; }

clone() {
  git clone -q "https://github.com/$2" "$WORK/$1"
  git -C "$WORK/$1" checkout -q "$3"
}

apply_patch() {
  patch -s -p1 -d "src/baselines/$1" < "patches/$1.patch"
}

bkmpp() {
  clone bkmpp uef-machine-learning/Balanced_k-Means_Revisited 8b64326b262d657ac76ef42ec45ef54972eb9fd0
  for f in cluster.cpp cluster.h coordinate.cpp coordinate.h gnuplot.h kMeans.cpp kMeans.h point.cpp point.h; do
    take "$WORK/bkmpp/SoftBKmeans/$f" "src/baselines/bkmpp/$f"
  done
}

lima() {
  clone lima leandrorochink/lima_vns_bmssc f61954e6972df872b450aacaa20c67c4379bbae1
  for f in DistanceMatrix.cpp DistanceMatrix.h LocalSearch.cpp LocalSearch.h Pair.cpp Pair.h \
           Point.cpp Point.h Random.cpp Random.h Solution.cpp Solution.h 'tempsC++.h' Vns.cpp Vns.h; do
    take "$WORK/lima/src/$f" "src/baselines/lima/$f"
  done
  take "$WORK/lima/LICENSE" src/baselines/lima/LICENSE
  apply_patch lima
}

rkm() {
  clone rkm zhu-he/regularized-k-means e45a26cd7914d9f86cbb02b59ffd90e708f1bf41
  for f in k_means.h network_simplex.h regularized_k_means.h; do take "$WORK/rkm/include/$f" "src/baselines/rkm/$f"; done
  for f in k_means.cc network_simplex.cc regularized_k_means.cc; do take "$WORK/rkm/src/$f" "src/baselines/rkm/$f"; done
}

sobmssc() {
  clone sobmssc rmartinsanta/BMSSC f76c20cc85dae7dc224e14605573f7185190d0f1
  mkdir -p src/baselines/sobmssc/upstream
  (cd "$WORK/sobmssc/src" && find . -type f) | while read -r f; do
    mkdir -p "src/baselines/sobmssc/upstream/$(dirname "$f")"
    take "$WORK/sobmssc/src/$f" "src/baselines/sobmssc/upstream/$f"
  done
  take "$WORK/sobmssc/LICENSE" src/baselines/sobmssc/upstream/LICENSE
  apply_patch sobmssc
}

mabmssc() {
  local zip="$WORK/MA_BMSSC.zip"
  curl -fsSL -o "$zip" "https://leria-info.univ-angers.fr/%7Ejinkao.hao/BMSSC/MA_BMSSC.zip"
  echo "77f5d8c1706481a85ad17d8bfafec8267de0474ab660b10253ff470a58a5cd75  $zip" | sha256sum -c --quiet \
    || { echo "MA_BMSSC.zip differs from the version used in the paper" >&2; exit 1; }
  unzip -q -d "$WORK/mabmssc" "$zip"
  take "$WORK/mabmssc/MA_BMSSC.cpp" src/baselines/mabmssc/ma_bmssc_upstream.cpp
  sed -i 's/99999999999999/1e300/' src/baselines/mabmssc/ma_bmssc_upstream.cpp
}

for name in ${@:-bkmpp lima rkm sobmssc mabmssc}; do
  echo "[fetch] $name"
  "$name"
done
echo "done; build with: make"
