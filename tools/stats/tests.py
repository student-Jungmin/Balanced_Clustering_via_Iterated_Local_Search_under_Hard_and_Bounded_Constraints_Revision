from __future__ import annotations

import warnings

import numpy as np
import pandas as pd
from scipy import stats


def _paired(df: pd.DataFrame, ours: str, other: str, metric: str,
            unit: str = "dataset"):
    a = df[df["algorithm"] == ours].set_index(["dataset", "seed"])[metric]
    b = df[df["algorithm"] == other].set_index(["dataset", "seed"])[metric]
    for name, series in ((ours, a), (other, b)):
        dup = series.index[series.index.duplicated()]
        assert dup.empty, f"{name}: repeated (dataset, seed) {list(dup)[:3]}"
    joined = pd.concat([a.rename("ours"), b.rename("other")], axis=1).dropna()
    if unit == "dataset":
        joined = joined.groupby(level="dataset").mean()
    elif unit != "seed":
        raise ValueError(f"unit must be 'dataset' or 'seed', not {unit!r}")
    return joined


def rank_biserial(x: np.ndarray, y: np.ndarray) -> float:
    d = x - y
    d = d[d != 0]
    if d.size == 0:
        return 0.0
    ranks = stats.rankdata(np.abs(d))
    pos = ranks[d > 0].sum()
    neg = ranks[d < 0].sum()
    total = pos + neg
    return float((pos - neg) / total) if total else 0.0


def holm(pvalues: list[float]) -> list[float]:
    m = len(pvalues)
    order = np.argsort(pvalues)
    adjusted = np.empty(m, dtype=float)
    running = 0.0
    for rank, idx in enumerate(order):
        value = (m - rank) * pvalues[idx]
        running = max(running, value)
        adjusted[idx] = min(1.0, running)
    return adjusted.tolist()


TIE_RTOL = 1e-9


def tied(x, y) -> np.ndarray:
    x = np.asarray(x, float); y = np.asarray(y, float)
    return np.abs(x - y) <= TIE_RTOL * np.maximum(np.abs(x), np.abs(y))


def logratio_test(x: np.ndarray, y: np.ndarray) -> dict:
    d = np.log(np.asarray(x, float) / np.asarray(y, float))
    d[tied(x, y)] = 0.0
    nz = d[d != 0]
    n = int(nz.size)
    if n == 0:
        return {"p": 1.0, "statistic": np.nan, "effect_r": 0.0, "zeros": int(d.size),
                "nonzero": 0, "method": "none"}
    method = "exact" if n <= 50 else "approx"
    res = stats.wilcoxon(nz, method=method)
    ranks = stats.rankdata(np.abs(nz))
    pos, neg = ranks[nz > 0].sum(), ranks[nz < 0].sum()
    return {"p": float(res.pvalue), "statistic": float(res.statistic),
            "effect_r": float((pos - neg) / (pos + neg)),
            "zeros": int(d.size - n), "nonzero": n, "method": method}


def compare(df: pd.DataFrame, ours: str = "HBC", metric: str = "mse",
            alternative: str = "two-sided", unit: str = "dataset") -> pd.DataFrame:
    others = [a for a in sorted(df["algorithm"].unique()) if a != ours]
    rows = []
    for other in others:
        joined = _paired(df, ours, other, metric, unit)
        if joined.empty:
            rows.append({"baseline": other, "pairs": 0, "note": "no paired runs"})
            continue
        x = joined["ours"].to_numpy(float)
        y = joined["other"].to_numpy(float)
        t = logratio_test(x, y)
        tie = tied(x, y)
        rel = np.where(tie, 0.0, (x - y) / y)
        rows.append({
            "baseline": other,
            "pairs": int(len(joined)),
            "datasets": int(joined.index.get_level_values(0).nunique()),
            "ours_better": int(((x < y) & ~tie).sum()),
            "baseline_better": int(((x > y) & ~tie).sum()),
            "ties": int(tie.sum()),
            "untied_pairs": t["nonzero"],
            "zero_pairs": t["zeros"],
            "p_method": t["method"],
            "median_rel_diff": float(np.nanmedian(rel)),
            "effect_r": t["effect_r"],
            "statistic": t["statistic"],
            "p": t["p"],
        })
    out = pd.DataFrame(rows)
    if "p" in out.columns and out["p"].notna().any():
        bad = out[out["p"].isna() & (out["pairs"] > 0)]
        assert bad.empty, f"non-finite p for {list(bad['baseline'])}"
        out["p_holm"] = holm(out["p"].fillna(1.0).tolist())
        out["significant_0.05"] = out["p_holm"] < 0.05
    return out


def win_counts(df: pd.DataFrame, ours: str = "HBC", metric: str = "mse") -> pd.DataFrame:
    means = df.groupby(["algorithm", "dataset"])[metric].mean().unstack(0)
    if ours not in means.columns:
        return pd.DataFrame()
    rows = []
    for other in means.columns:
        if other == ours:
            continue
        both = means[[ours, other]].dropna()
        tie = tied(both[ours], both[other])
        rows.append({
            "baseline": other,
            "datasets": len(both),
            "ours_wins": int(((both[ours] < both[other]) & ~tie).sum()),
            "baseline_wins": int(((both[ours] > both[other]) & ~tie).sum()),
        })
    return pd.DataFrame(rows)
