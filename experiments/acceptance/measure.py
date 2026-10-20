#!/usr/bin/env python3
from __future__ import annotations
import csv, re, statistics, sys
from collections import defaultdict
from pathlib import Path

import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "stats"))
import tests  # noqa: E402

BASE = "randomwalk"

ARMS = {
    "randomwalk":     ("Random Walk (published)", "#404040", "o", False),
    "better":         ("Better, as implemented",  "#a83232", "s", True),
    "better_stall3":  ("Better, stall 3",         "#1f4e79", "^", True),
    "better_stall10": ("Better, stall 10",        "#4f6228", "v", False),
}
VARIANTS = [k for k in ARMS if k != BASE]

SCALES = [("standard", "standard scale"), ("large", "large scale")]

_W = re.compile(r"term=W\((\d+)")


def arm_of(config: str) -> str | None:
    fields = dict(f.split("=", 1) for f in config.split(",") if "=" in f)
    accept = fields.get("accept")
    if accept == "RandomWalk":
        return BASE
    if accept != "Better":
        return None
    window = _W.search(config)
    return f"better_stall{window.group(1)}" if window else "better"


def read(path: Path):
    if not path.exists():
        return {}
    per = defaultdict(lambda: defaultdict(list))
    for r in csv.DictReader(path.open(), delimiter="\t"):
        arm = arm_of(r["config"])
        if arm is None:
            continue
        per[r["dataset"]][arm].append(
            (float(r["mse"]), float(r["time_s"]), int(r["seed"])))
    return per


def ratios(per, variant):
    out = {}
    for dataset, arms in sorted(per.items(), key=lambda kv: kv[0].lower()):
        if not {BASE, variant} <= arms.keys():
            continue
        base = {s: (m, t) for m, t, s in arms[BASE]}
        var = {s: (m, t) for m, t, s in arms[variant]}
        seeds = sorted(base.keys() & var.keys())
        if not seeds:
            continue
        bm = statistics.fmean(base[s][0] for s in seeds)
        vm = statistics.fmean(var[s][0] for s in seeds)
        bt = statistics.fmean(base[s][1] for s in seeds)
        vt = statistics.fmean(var[s][1] for s in seeds)
        if bm <= 0 or bt <= 0:
            continue
        out[dataset] = dict(mse=vm / bm, time=vt / bt, seeds=len(seeds),
                            base_mse=bm, var_mse=vm, base_t=bt, var_t=vt)
    return out


def wilcoxon(per):
    rows = []
    for dataset, arms in per.items():
        for arm, runs in arms.items():
            for mse, seconds, seed in runs:
                rows.append(dict(algorithm=arm, dataset=dataset, seed=seed,
                                 mse=mse, time_s=seconds))
    if not rows:
        return {}
    df = pd.DataFrame(rows)
    out = defaultdict(dict)
    for metric in ("mse", "time_s"):
        result = tests.compare(df, ours=BASE, metric=metric)
        for _, row in result.iterrows():
            out[row["baseline"]][metric] = row.to_dict()
    return out


def load(results: Path):
    by_scale, merged = {}, defaultdict(lambda: defaultdict(list))
    for scale, _ in SCALES:
        per = defaultdict(lambda: defaultdict(list))
        found = False
        for f in sorted((results / scale).glob("raw_*.tsv")):
            for dataset, arms in read(f).items():
                found = True
                for arm, runs in arms.items():
                    per[dataset][arm].extend(runs)
        if not found:
            continue
        by_scale[scale] = {v: ratios(per, v) for v in VARIANTS}
        for dataset, arms in per.items():
            for arm, runs in arms.items():
                merged[dataset][arm].extend(runs)
    if not by_scale:
        raise SystemExit("no records - run experiments/acceptance/run.sh first")

    counts = {}
    for v in VARIANTS:
        every = {d: r for s in by_scale.values() for d, r in s.get(v, {}).items()}
        counts[v] = dict(datasets=len(every),
                         mse_better=sum(1 for x in every.values() if x["mse"] < 1),
                         time_better=sum(1 for x in every.values() if x["time"] < 1))
    return by_scale, merged, wilcoxon(merged), counts


def fmt_p(value) -> str:
    if value is None:
        return "n/a"
    if value >= 1e-3:
        return f"{value:.3g}"
    mantissa, exponent = f"{value:.2e}".split("e")
    return rf"{mantissa} \times 10^{{{int(exponent)}}}"


def stat_line(stats, variant, metric, what) -> str:
    s = (stats.get(variant) or {}).get(metric)
    if not s or not s.get("pairs"):
        return f"The paired test on {what} for {ARMS[variant][0]} is not available."
    med = s.get("median_rel_diff") or 0.0
    p = s.get("p_holm", s.get("p"))
    lower = "Random Walk" if med < 0 else ARMS[variant][0]
    verdict = ("a difference the test does not reject"
               if p is not None and p >= 0.05 else "and the test rejects equality")
    return (f"Against {ARMS[variant][0]}, the median paired difference on "
            f"{what} over {int(s['pairs'])} pairs is ${med * 100:+.2f}\\%$ "
            f"($r = {s.get('effect_r', float('nan')):.2f}$, "
            f"$p = {fmt_p(p)}$), so {lower} is the lower of the two, {verdict}.")


def write_summary(by_scale, stats, path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["scale", "arm", "dataset", "seeds",
                    "mse_randomwalk", "mse_arm", "mse_ratio",
                    "time_randomwalk", "time_arm", "time_ratio"])
        for scale, _ in SCALES:
            for variant in VARIANTS:
                for dataset, v in (by_scale.get(scale, {}).get(variant) or {}).items():
                    w.writerow([scale, variant, dataset, v["seeds"],
                                f"{v['base_mse']:.10g}", f"{v['var_mse']:.10g}",
                                f"{v['mse']:.6f}", f"{v['base_t']:.10g}",
                                f"{v['var_t']:.10g}", f"{v['time']:.6f}"])
        f.write("\n# paired Wilcoxon signed-rank, Random Walk (ours) against each "
                "arm, over the datasets of both scales, each the mean of its ten seeds\n")
        for variant, metrics in stats.items():
            for metric, s in metrics.items():
                f.write(f"# {variant}\t{metric}\t"
                        + "\t".join(f"{k}={v}" for k, v in s.items()) + "\n")
    return path
