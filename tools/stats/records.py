from __future__ import annotations

import pandas as pd

NUMERIC = [
    "n", "d", "k", "seed", "run", "mse", "sse", "time_s",
    "iters", "min_size", "max_size", "feasible", "tau", "rho",
]


def load(path) -> pd.DataFrame:
    df = pd.read_csv(path, sep="\t", dtype=str)
    df = df[df["algorithm"] != "algorithm"]
    for col in NUMERIC:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
    df["feasible"] = df["feasible"].fillna(0).astype(int).astype(bool)
    return df.reset_index(drop=True)


def check(df: pd.DataFrame) -> list[str]:
    problems = []

    per_algo = df.groupby("algorithm")["dataset"].apply(set)
    universe = set().union(*per_algo) if len(per_algo) else set()
    for algo, seen in per_algo.items():
        missing = universe - seen
        if missing:
            problems.append(
                f"{algo}: no runs for {len(missing)} dataset(s): "
                + ", ".join(sorted(missing)[:6])
                + ("..." if len(missing) > 6 else "")
            )

    seeds = df.groupby("algorithm")["seed"].apply(lambda s: tuple(sorted(set(s))))
    if len(set(seeds)) > 1:
        problems.append(
            "algorithms did not use the same seed list: "
            + "; ".join(f"{a}={list(s)[:5]}" for a, s in seeds.items())
        )

    counts = df.groupby(["algorithm", "dataset"]).size()
    if counts.nunique() > 1:
        lo, hi = counts.min(), counts.max()
        problems.append(f"run counts differ across (algorithm, dataset): {lo}..{hi}")

    infeasible = df[~df["feasible"]]
    if len(infeasible):
        by = infeasible.groupby("algorithm").size().to_dict()
        problems.append(f"runs violating their own balance constraint: {by}")

    return problems


def per_dataset(df: pd.DataFrame) -> pd.DataFrame:
    g = df.groupby(["algorithm", "dataset"])
    out = g.agg(
        runs=("mse", "size"),
        mse_mean=("mse", "mean"),
        mse_sd=("mse", lambda s: s.std(ddof=1)),
        mse_min=("mse", "min"),
        time_mean=("time_s", "mean"),
        time_sd=("time_s", lambda s: s.std(ddof=1)),
        feasible=("feasible", "all"),
    ).reset_index()
    return out
