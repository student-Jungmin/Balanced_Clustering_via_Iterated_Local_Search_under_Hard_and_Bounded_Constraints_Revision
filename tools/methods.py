from __future__ import annotations
import re

ORDER = ["RKM", "BKM", "BKMPP", "LIMA", "MABMSSC", "SOBMSSC", "TBC",
         "HBC", "BBC"]

_TRAILING_RHO = re.compile(r"^(.*?)(\d+)$")


def _split(name: str) -> tuple[str, int, int, bool]:
    repaired = name.endswith("_repaired")
    stem = name[: -len("_repaired")] if repaired else name
    budget = 0
    head, sep, tail = stem.rpartition("x")
    if sep and head and tail.isdigit():
        stem, budget = head, int(tail)
    rho = 0
    m = _TRAILING_RHO.match(stem)
    if m and m.group(1) in ORDER:
        stem, rho = m.group(1), int(m.group(2))
    return stem, rho, budget, repaired


def rank(name: str):
    stem, rho, budget, repaired = _split(name)
    try:
        return (ORDER.index(stem), rho, budget, repaired, name)
    except ValueError:
        return (len(ORDER), 0, 0, False, name)


def sort(names):
    return sorted(names, key=rank)
