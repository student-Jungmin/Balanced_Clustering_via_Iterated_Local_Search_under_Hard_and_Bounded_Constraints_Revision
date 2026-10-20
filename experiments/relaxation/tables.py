#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, statistics, sys
from collections import defaultdict
from pathlib import Path

LABEL = {"MABMSSC": r"MA-BMSSC$\times$1$^\dagger$",
         "MABMSSCx100": r"MA-BMSSC$\times$100$^\dagger$",
         "HBC": "HBC", "BKMPP": "BKM++",
         "LIMAx1": r"LIMA$\times$1$^\dagger$",
         "LIMAx100": r"LIMA$\times$100$^\dagger$",
         "SOBMSSC": "SO-BMSSC"}
sys.path.insert(0, "tools")
from methods import sort as table1_order

ORDER = table1_order(["HBC", "RKM", "BKM", "TBC", "MABMSSC", "MABMSSCx100", "BKMPP",
                      "LIMAx1", "LIMAx100", "SOBMSSC"])


def read(path: Path):
    if not path.exists():
        raise SystemExit(f"missing {path} - run experiments/relaxation/run.sh first")
    acc = defaultdict(list)
    infeasible = defaultdict(int)
    total = defaultdict(int)
    for r in csv.DictReader(path.open(), delimiter="\t"):
        algorithm = r["algorithm"]
        if algorithm.startswith("TBC"):
            if algorithm != "TBC_repaired":
                continue
            algorithm = "TBC"
        total[(algorithm, r["rho"])] += 1
        if r["feasible"] != "1":
            raise SystemExit(f"{algorithm} {r['dataset']} rho={r['rho']}: "
                             "relaxation ended outside max - min <= tau")
        acc[(algorithm, r["rho"])].append(float(r["gain_pct"]))
    return acc, infeasible, total


def dataset_means(path: Path):
    after = defaultdict(lambda: defaultdict(list))
    start = defaultdict(lambda: defaultdict(list))
    for r in csv.DictReader(path.open(), delimiter="\t"):
        algorithm = r["algorithm"]
        if algorithm.startswith("TBC"):
            if algorithm != "TBC_repaired":
                continue
            algorithm = "TBC"
        if float(r["rho"]) == min(GRID):
            start[algorithm][r["dataset"]].append(float(r["mse_before"]))
        if r["feasible"] != "1":
            raise SystemExit(f"{algorithm} {r['dataset']} rho={r['rho']}: infeasible")
        after[(algorithm, r["rho"])][r["dataset"]].append(float(r["mse_after"]))
    mean = lambda d: {k: statistics.fmean(v) for k, v in d.items()}
    return ({k: mean(v) for k, v in after.items()},
            {k: mean(v) for k, v in start.items()})


def gap(ours: dict, other: dict):
    import numpy as np
    from stats.tests import logratio_test, tied
    shared = sorted(set(ours) & set(other))
    x = np.array([other[d] for d in shared]); y = np.array([ours[d] for d in shared])
    med = float(np.median(np.where(tied(x, y), 0.0, (x - y) / y))) * 100
    t = logratio_test(x, y)
    return med, t["p"], t


def p_text(p: float) -> str:
    return r"$<$0.001" if p < 0.001 else f"{p:.3f}"


GRID = (0.01, 0.05, 0.1, 0.25)


LOG: list = []


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--records", type=Path,
                    default=Path("results/relaxation/transfer.tsv"))
    ap.add_argument("--out", type=Path, default=Path("paper/generated"))
    a = ap.parse_args()
    acc, infeasible, total = read(a.records)
    per_ds = defaultdict(lambda: defaultdict(list))
    for r in csv.DictReader(a.records.open(), delimiter="\t"):
        alg = r["algorithm"]
        if alg.startswith("TBC"):
            if alg != "TBC_repaired":
                continue
            alg = "TBC"
        per_ds[(alg, r["rho"])][r["dataset"]].append(float(r["gain_pct"]))
    after, start = dataset_means(a.records)
    a.out.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, "tools")
    from stats.tests import holm

    rhos = sorted({rho for _, rho in acc if float(rho) in GRID}, key=float)
    present = [x for x in ORDER if any(k[0] == x for k in acc)]
    omitted = sorted({k[0] for k in acc} - set(present))
    if omitted:
        print("  not in the table:", ", ".join(omitted))

    final = {}
    for rho in rhos:
        others = [x for x in present if x != "HBC"]
        res = {x: gap(after[("HBC", rho)], after[(x, rho)]) for x in others}
        for x, q in zip(others, holm([res[x][1] for x in others])):
            final[(x, rho)] = (res[x][0], q)
            LOG.append((rho, x, res[x][2]["zeros"], res[x][2]["method"], len(others),
                        res[x][1], q, abs(res[x][2]["effect_r"]), res[x][0]))

    rows = []
    for algorithm in present:
        if algorithm == "HBC":
            cells = ["ref."]
        else:
            g = gap(start['HBC'], start[algorithm])[0]
            cells = ["$0.00$" if abs(g) < 0.005 else f"${g:+.2f}$"]
        for rho in rhos:
            g = per_ds.get((algorithm, rho))
            red = statistics.median(statistics.fmean(v) for v in g.values()) if g else None
            cells.append(f"${red:.2f}$" if red is not None else "--")
            if algorithm == "HBC":
                cells.append("ref.")
            else:
                med, q = final[(algorithm, rho)]
                txt = "0.00" if abs(med) < 0.005 else f"{med:+.2f}"
                cells.append(f"${txt}$ ({p_text(q)})")
        rows.append(f"{LABEL.get(algorithm, algorithm)} & " + " & ".join(cells) + r" \\")
    groups = " & ".join(rf"\multicolumn{{2}}{{c}}{{$\bm{{\rho={float(r):g}}}$}}" for r in rhos)
    cmid = " ".join(rf"\cmidrule(lr){{{3+2*i}-{4+2*i}}}" for i in range(len(rhos)))
    heads = " & ".join(r"\textbf{Red.} & \textbf{Final gap ($p$)}" for _ in rhos)
    name = "tab_relaxation_transfer.tex"
    (a.out / name).write_text(f"""% generated by experiments/relaxation/tables.py -- do not edit
\\begin{{table}}[htbp]
\\centering
\\caption{{Our relaxation started from each method's hard-balanced solution on 28 standard-scale datasets. \\emph{{Start gap}}, \\emph{{Final gap}}: median relative MSE difference (\\%) from HBC before and after the relaxation; \\emph{{Red.}}: median MSE reduction (\\%); $p$: paired Wilcoxon test (Section~\\ref{{sec:metric}}), Holm-corrected at each $\\rho$. $^\\dagger$Does not converge within the matched time budget.}}
\\label{{tab:relaxation_transfer}}
\\small
\\setlength{{\\tabcolsep}}{{3pt}}
\\resizebox{{\\textwidth}}{{!}}{{
\\begin{{tabular}}{{lc{'cc' * len(rhos)}}}
\\toprule
 & & {groups} \\\\
{cmid}
\\textbf{{Starting solution}} & \\textbf{{Start gap}} & {heads} \\\\
\\midrule
{chr(10).join(rows)}
\\bottomrule
\\end{{tabular}}
}}
\\end{{table}}
""")
    print(f"  {a.out / name}")
    with open(a.out / "tab_relaxation_transfer.log.tsv", "w") as f:
        f.write("rho\tstart\tzero_pairs\tp_method\tholm_family\tp_raw\tp_holm\tr\tfinal_gap\n")
        for row in LOG:
            f.write("\t".join(str(v) for v in row) + "\n")


if __name__ == "__main__":
    main()
