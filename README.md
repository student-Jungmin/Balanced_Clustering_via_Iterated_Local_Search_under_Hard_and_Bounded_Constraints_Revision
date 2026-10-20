# Balanced Clustering via Iterated Local Search under Hard and Bounded Constraints

Code and per-run results of the revised manuscript.

## Code

| Path | Content | Licence |
|---|---|---|
| `src/ours/`, `src/harness/`, `src/baselines/{bkm,shared}/`, wrappers in `src/baselines/*/` | HBC and BBC, the run harness, our reimplementation of BKM | MIT (`LICENSE`) |
| `src/baselines/tbc/` | TBC, modified to enforce max − min ≤ τ | GPL-3.0 (`src/baselines/tbc/LICENSE`) |

Fetched from their authors by `tools/fetch_baselines.sh`, not redistributed:

| Method | Source, version | Licence | Our change |
|---|---|---|---|
| BKM++ | github.com/uef-machine-learning/Balanced_k-Means_Revisited @ `8b64326` | none stated | none |
| LIMA | github.com/leandrorochink/lima_vns_bmssc @ `f61954e` | MIT | `patches/lima.patch`: the time budget is checked on a nanosecond clock (the original ticks every 10 ms) |
| MA-BMSSC | leria-info.univ-angers.fr/~jinkao.hao/MABMSSC.html, `MA_BMSSC.zip` | none stated | the sentinel `MAXNUM` 1e14 → 1e300 |
| RKM | github.com/zhu-he/regularized-k-means @ `e45a26c` | none stated | none |
| SO-BMSSC | github.com/rmartinsanta/BMSSC @ `f76c20c` | MIT | `patches/sobmssc.patch`: the seed is set per run |

## Toolchain

Linux on x86-64, with g++ 11, GNU make, OpenJDK 17 (SO-BMSSC only),
Python 3.10 with numpy 1.23.5, scipy 1.10.0, pandas 1.5.3 and matplotlib 3.7.0,
and git, curl, unzip, patch and sed.

With conda:

```bash
conda env create -f environment.yml
conda activate balanced-clustering
```

Or, on Ubuntu 22.04:

```bash
sudo apt install g++ make openjdk-17-jdk-headless git curl unzip patch python3-pip
pip install -r requirements.txt
```

`make` uses the `g++` and `javac` on the `PATH`; `make CXX=...` or
`make JAVA_HOME=...` selects others.

## Build

```bash
tools/fetch_baselines.sh   # BKM++, LIMA, MA-BMSSC, RKM, SO-BMSSC from their authors
make                       # all binaries into bin/
```

## Data

```bash
python3 tools/prepare/prepare.py fetch    # download the originals
python3 tools/prepare/prepare.py build    # write data/<Name>.txt
python3 tools/prepare/prepare.py verify   # sha256 of every dataset file
```

The source, preprocessing and sha256 of each dataset are listed in
`tools/prepare/manifest.tsv`.

## Run

Every binary takes `--dataset NAME` (repeatable) or `--scale standard|large`,
`--runs 10 --seed-base 42` (the default: seeds 42–51), `--out DIR` and
`--raw FILE`; `--list` prints the datasets.

```bash
./bin/bc-ours --dataset Iris --hard-tau on --out /tmp/hbc   # HBC on Iris, seeds 42-51
```

| Method | Command |
|---|---|
| HBC | `bin/bc-ours --scale S --hard-tau on` |
| BBC | `bin/bc-ours --scale S --rho R` |
| BKM | `bin/bkm --scale standard` |
| BKM++ | `bin/bkmpp --scale S` |
| LIMA×k, MA-BMSSC×k | `bin/lima --scale standard --budget results/benchmark/standard/ours_summary.txt --scale-factor k` (likewise `bin/mabmssc`) |
| RKM | `bin/rkm --scale S` |
| SO-BMSSC | `bin/sobmssc --scale standard` |
| TBC | `bin/tbc --scale S --rho R --tbc-mode repaired` (the TBC of the paper; `--tbc-mode published` runs the distributed code) |
| Relaxation of another method's solution | `bin/<method> --labels DIR …`, then `bin/bc-tau --dataset NAME --labels FILE --rho R` |

## Modified TBC

The distributed code of TBC does not enforce the condition it declares,
max − min ≤ τ; `experiments/feasibility` measures the violations reported in
the paper. `--tbc-mode repaired`, the TBC of the paper, enforces it:

- In each assignment step, a size window [L, L + τ] is fixed from the
  nearest-centroid cluster sizes of the previous step: L minimizes their total
  excess and deficit outside the window. In the first step, the window is
  centered on N/K; for τ ≤ 1, it is fixed by N and K.
- No cluster exceeds L + τ: a closer point still enters a full cluster, which
  then evicts its farthest point, as in the distributed code.
- After the assignment, each cluster below L is filled by moving in, from
  clusters above L, the points that least increase the SSE.

This enforcement lengthens the runtime of TBC by about 1.3 times in geometric
mean. `--tbc-mode published` runs the distributed behavior unchanged.

## Experiments

Each directory of `experiments/` holds `run.sh`, which reruns the experiment
into `results/`, and `tables.py` / `figure.py`, which write its tables and
figures from `results/` into `paper/generated/`; `make exp-<name>` does both,
and `experiments/run-all.sh` runs them all.

| Directory | Tables and figures |
|---|---|
| `benchmark` | Table 5, Tables A.1–A.2 |
| `comparison` | Figures 2–3 (from the `benchmark` records) |
| `bbc` | Table 6, Tables A.3–A.6 |
| `relaxation` | Table 7 |
| `runtime` | Table 8, Table B.2 |
| `pruning` | Tables 9–10, Tables B.3 and B.6 |
| `convergence` | Figure 4, Table B.7 (from the `pruning` records) |
| `acceptance` | Table B.1 |
| `optimization` | Table B.4 |
| `exactness` | Table B.5 |
| `feasibility` | Balance violations of the distributed TBC (Section 4.2) |
| `calibration` | Figure 5, Table 2 |

`calibration/figure.py` draws the grid of the submitted version
(`results/calibration/published/grid.tsv`), whose runs were not kept;
`calibration/run.sh` reruns the sweep with the current code and
`figure.py --grid ''` draws it.

## License

`src/baselines/tbc/` is a modified version of TBC and is distributed under
GPL-3.0 (`src/baselines/tbc/LICENSE`); everything else in this repository is
under the MIT License (`LICENSE`). `patches/` modifies the code of LIMA and
SO-BMSSC, both MIT-licensed; `tools/fetch_baselines.sh` fetches their original
LICENSE files along with the code.
