#!/usr/bin/env python3
from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import re
import struct
import sys
import urllib.request
import zipfile
from dataclasses import dataclass
from decimal import ROUND_HALF_UP, Decimal
from pathlib import Path
from typing import Callable

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
MANIFEST = HERE / "manifest.tsv"


@dataclass(frozen=True)
class Source:
    url: str
    sha256: str
    member: str | None = None

    @property
    def cache_name(self) -> str:
        return re.sub(r"^https?://", "", self.url).replace("/", "_")


SIPU = "https://cs.uef.fi/sipu/datasets/"
SANTA = "https://cs.uef.fi/sipu/santa/SantaTourData.zip"
UCI = "https://archive.ics.uci.edu/ml/machine-learning-databases/"
BKMPP = ("https://raw.githubusercontent.com/uef-machine-learning/"
         "Balanced_k-Means_Revisited/8b64326b262d657ac76ef42ec45ef54972eb9fd0/"
         "SoftBKmeans/data/")
BMSSC = ("https://raw.githubusercontent.com/rmartinsanta/BMSSC/"
         "f76c20cc85dae7dc224e14605573f7185190d0f1/resources/instances/sota/")
TBC = ("https://raw.githubusercontent.com/MKMaS-GUET/tau-balanced-clustering/"
       "efa7114601bed026132e274c4fb2a5ddf21d47e8/datasets/Unbalanced/")
MNIST = "https://ossci-datasets.s3.amazonaws.com/mnist/"
KAGGLE_FASHION = "https://www.kaggle.com/api/v1/datasets/download/zalando-research/fashionmnist"

WORMS = SIPU + "worms.zip"
WORMS_SHA = "58be0146e5a5e6138e4a6b54810d90736f36f233e5570b1da2288c0f3ad99cb6"
SHUTTLE_TRN = Source(UCI + "statlog/shuttle/shuttle.trn.Z",
                     "ffd2cbf221d4982ccc77c674621c23291447f3fd9484c00d8078858965e0e7ed")
SHUTTLE_TST = Source(UCI + "statlog/shuttle/shuttle.tst",
                     "f776934a628d9b94c482cb058a76ddaa63823e0f74aec065813c9dff3b89d661")
VEHICLE = [Source(UCI + f"statlog/vehicle/xa{c}.dat", h) for c, h in zip(
    "abcdefghi",
    ["091ec73ae54031aea8432d00caaef101a445eae13196039db9079b0eac13331d",
     "3fbc55ac03592425521df18d59ba2ed7ddca626b22f84f5fc3253ebda6b82145",
     "43536f3aadb2a285fa2ba6e13dacc7013c2298fbd728f011e88669bb44856b8d",
     "371b0de9ba3564d8220d8ad17c5f2f886bafebd2c486de1b0273ceb0c24d3035",
     "503f9c87feb46dfaab6e3c4275e121640a39c0b30b29f120e679ce8e3fbc7be2",
     "460e14ec4746a1464d7a03671e09310695c348d6831452cb53172734a6a70531",
     "c279f285c353beaa661168e68feeac726cd446f076f717662635267d89006601",
     "467735394d1a058e15da81d51fd8e1147d425e7f53392ac6b8e730a34a02049e",
     "feba2f3aad6db050bc7acbd224cd4e0d32846865ab5cfc53cc59baea2e1ff484"])]


def lines(raw: bytes, skip: int = 0) -> list[str]:
    return [l for l in raw.decode("ascii").splitlines()[skip:] if l.strip()]


def split(line: str, sep: str | None) -> list[str]:
    return line.split(sep) if sep else line.split()


def columns(raws: list[bytes], sep: str | None, out: str,
            keep: Callable[[list[str]], list[str]] = lambda t: t,
            skip: int = 0, fmt: Callable[[str], str] = lambda t: t) -> bytes:
    rows = []
    for raw in raws:
        for l in lines(raw, skip):
            rows.append(out.join(fmt(t) for t in keep(split(l.strip(), sep))))
    return ("\n".join(rows) + "\n").encode("ascii")


def numpy_e18(t: str) -> str:
    return "%.18e" % float(t)


def int_or_repr(t: str) -> str:
    v = float(t)
    return str(int(v)) if v.is_integer() else repr(v)


def excel_general(t: str) -> str:
    d = Decimal(t)
    if "E" in t.upper():
        e = d.adjusted()
        m = d.scaleb(-e).quantize(Decimal("0.01"), rounding=ROUND_HALF_UP)
        if abs(m) >= 10:
            m, e = (m / 10).quantize(Decimal("0.01"), ROUND_HALF_UP), e + 1
        return f"{m}E{e:+03d}"
    s = str(d.quantize(Decimal("1e-9"), rounding=ROUND_HALF_UP))
    return s.rstrip("0").rstrip(".") if "." in s else s


def unlzw(data: bytes) -> bytes:
    if data[:2] != b"\x1f\x9d":
        raise ValueError("not a .Z file")
    maxbits, block = data[2] & 0x1F, bool(data[2] & 0x80)
    out = bytearray()
    pos, total, nbits = 24, len(data) * 8, 9
    table = [bytes([i]) for i in range(256)] + ([b""] if block else [])
    prev = None
    group = pos

    def align():
        nonlocal pos
        used = (pos - group) % (nbits * 8)
        if used:
            pos += nbits * 8 - used

    while True:
        if len(table) >= (1 << nbits) and nbits < maxbits:
            align()
            nbits += 1
            group = pos
        if pos + nbits > total:
            break
        chunk = int.from_bytes(data[pos >> 3:(pos >> 3) + 4], "little")
        code = (chunk >> (pos & 7)) & ((1 << nbits) - 1)
        pos += nbits
        if block and code == 256:
            align()
            table, nbits, group, prev = table[:257], 9, pos, None
            continue
        if code < len(table):
            entry = table[code]
            if prev is not None:
                table.append(prev + entry[:1])
        elif code == len(table) and prev is not None:
            entry = prev + prev[:1]
            table.append(entry)
        else:
            raise ValueError("corrupt .Z stream")
        out += entry
        prev = entry
    return bytes(out)


def idx_images(raw: bytes) -> list[bytes]:
    data = gzip.decompress(raw)
    magic, n, r, c = struct.unpack(">4i", data[:16])
    assert magic == 2051, "not an IDX image file"
    return [data[16 + i * r * c:16 + (i + 1) * r * c] for i in range(n)]


def pixels(raws: list[bytes]) -> bytes:
    text = [str(i) for i in range(256)]
    out = []
    for raw in raws:
        for img in idx_images(raw):
            out.append(" ".join(text[p] for p in img))
    return ("\n".join(out) + "\n").encode("ascii")


@dataclass
class Recipe:
    sources: list[Source]
    make: Callable[[list[bytes]], bytes] | None
    manual: str = ""


def verbatim(raws):
    return raws[0]


def drop(*idx):
    def keep(t):
        n = len(t)
        gone = {i % n for i in idx}
        return [v for i, v in enumerate(t) if i not in gone]
    return keep


def bkmpp(name, sha):
    return Recipe([Source(BKMPP + name, sha)], verbatim)


def bmssc(name, sha):
    return Recipe([Source(BMSSC + name, sha)],
                  lambda r: columns(r, ",", "\t"))


RECIPES: dict[str, Recipe] = {
    "A1": bkmpp("A1.txt", "52e6a3fd83ed3f9dd2d0ea2a56ee2dcdfa5956ac36302a79bf13e7f620c5123a"),
    "A2": bkmpp("A2.txt", "316dda3dacfb168f5e1c064afe93f9a4a573b704a84a8ab9fad3cd4607dc7829"),
    "A3": bkmpp("A3.txt", "ac07048d137e055112059614a282a588376c81465d64abb97996249a98316ec9"),
    "S1": bkmpp("S1.txt", "50e8f105346f135aaa38c49903311773cd7d5f90d612d94a1b86e0ea7880b7a7"),
    "S2": bkmpp("S2.txt", "36344fb4bfa6c434c0dfad55bc7b73d0871198e056ac7d705b4f7a15c7e13bc3"),
    "S3": bkmpp("S3.txt", "037df28a52bb89acadb4fcf5d7bbee14909ed29e66d5f4341c28e4594033dc07"),
    "S4": bkmpp("S4.txt", "0699a23356aeb21ae56cbf6ff94bab9e898c533e335b041c09aa260e30cad9f6"),
    "dim32": bkmpp("dim32.txt", "2c5481228d978f4cff48df73aa5ddd3ed11bb4f8a3bb2f5b6499756f46d8080f"),
    "unbalance": bkmpp("unbalance.txt", "7579605f70954c6a419aa3fc63523f080b99b7734d7554e3482f6421bba57b37"),
    "Iris": bkmpp("iris.txt", "cad0da59d0e20e3e13d05a54b93ecfad3e9feb15add88db55cba56d6a632a486"),
    "Multiple Features": bkmpp("features.txt", "57dafdd647d5510cab7d9e210796aede463f6117d2d9d91e5690c4ad02a48783"),
    "Thyroid": bkmpp("thyroid.txt", "44e0bcb4215413b2bb443f99f82d1805c7b370621f878fe8be3955764ff7acaa"),
    "User Knowledge": bkmpp("user.txt", "f589703e094659e36389e1a10200bc31cbc7df49c72a1b4f19f405ac2c632702"),
    "Vowel Recognition": bkmpp("vowel.txt", "61022159adfdbf0d7c413f7bf8611bdbd1a0f7e08b6e9b29fba0b5cbecbdabfa"),
    "Wine": bkmpp("wine.txt", "9b5ffe147e14861b0cb7aa210d3fa71f2977bcf9750e5c20b71087f505d64808"),

    "Birch1": Recipe([Source(SIPU + "birch1.txt", "95230d302b2ffbe15de77f732af7002b037887c30c19b1539999dedfe3587400")], verbatim),
    "Birch2": Recipe([Source(SIPU + "birch2.txt", "4d8babe0a92e4bf3011bde7c60656cf96cef177e25d1e12e7961e6f9c77f568e")], verbatim),
    "Worms2": Recipe([Source(WORMS, WORMS_SHA, "worms/worms_2d.txt")], verbatim),
    "Worms64": Recipe([Source(WORMS, WORMS_SHA, "worms/worms_64d.txt")], verbatim),
    "Santa": Recipe([Source(SANTA, "689ec5d4e28288657f347e1adf50bee0971937521ccf4ab3babce704afb04555", "dataset.txt")],
                    lambda r: columns(r, " ", "\t")),

    "Body": bmssc("body.csv", "87593a2fa3f2d4ea9d4c7f4b106e5c4dbbd942f26558dfdd7b21d457bea28cec"),
    "Breast Cancer": bmssc("breast_cancer.csv", "6d7d2e9ce16886032d68b4937f0c15943fc7a2d9d0d4edff69efe1a3f2c520da"),
    "Ionosphere": bmssc("ionosphere.csv", "968306bbfc94a7d9c423a237cd7761f6fd88859cad90d197c5fafaaf36a09b27"),
    "Water": bmssc("water.csv", "57651c2a87e5d2f2150af15c48497a6d184db1ba2f1f176be8be90e45ff85a87"),

    "Cryotherapy": Recipe([Source(TBC + "Cryotherapy/Cryotherapy.csv", "1c5e8df1c66d731e1cfbb53bdd2fc5b775d58a26e08ce028943ee594cd9dd7e4")],
                          lambda r: columns(r, ",", " ", fmt=numpy_e18)),
    "G2": Recipe([Source(TBC + "G/G.csv", "fb3b0d631cafe013cd5df19d98884fc038075ac0275d8f66966b6a46e8c40d54")],
                 lambda r: columns(r, ",", " ", fmt=numpy_e18)),
    "User Model": Recipe([Source(TBC + "UserModel/UserModel.csv", "6bedbaa5e73046df83e41f0da708de1ab369a26154fe39b337a5b9e12daccf0e")],
                         lambda r: columns(r, ",", " ", fmt=int_or_repr)),

    "Glass": Recipe([Source(UCI + "glass/glass.data", "dd67373f4baf2807345df02cbfef2093d342e61ad0d82a4fb79af43ef8ce449d")],
                    lambda r: columns(r, ",", " ", drop(0, -1))),
    "Image Segmentation": Recipe(
        [Source(UCI + "image/segmentation.data", "ed5b5aee8081bbe875357149f73f50d7085e22cc0897f5b52eb7b52fea969a5b"),
         Source(UCI + "image/segmentation.test", "2e9e966479d54c6aaec309059376dd9c89c1b46bf3a23aceeefb36d20d93a189")],
        lambda r: columns(r, ",", " ", drop(0), skip=5)),
    "Libras": Recipe([Source(UCI + "libras/movement_libras.data", "97ebdaa6a9b28ab4a2cdd84b14f19a95a7456a46137c362b65a0669eca3c3c4d")],
                     lambda r: columns(r, ",", " ", drop(-1))),
    "Synthetic Control": Recipe([Source(UCI + "synthetic_control-mld/synthetic_control.data", "bb716cde4ab0fb4bd512dd90ccc61bf77fc795266b87b786ed273d6ea3fb5a45")],
                                lambda r: columns(r, None, " ")),
    "Vehicle": Recipe(VEHICLE, lambda r: columns(r, None, " ", drop(-1))),
    "Yeast": Recipe([Source(UCI + "yeast/yeast.data", "7cf61776fc04f527f93bf57a327b863893a1225d82df02d457e8950173218258")],
                    lambda r: columns(r, None, " ", drop(0, -1))),
    "Letter Recognition": Recipe(
        [Source(UCI + "letter-recognition/letter-recognition.data", "2b89f3602cf768d3c8355267d2f13f2417809e101fc2b5ceee10db19a60de6e2")],
        lambda r: columns(r, ",", " ", drop(0), fmt=lambda t: str(int(t) * 1000))),
    "Sensorless Drive": Recipe(
        [Source(UCI + "00325/Sensorless_drive_diagnosis.txt", "366b8c5c564a5a4d07600db9180d2871f51489104b554f6320781514059f35a9")],
        lambda r: columns(r, None, " ", drop(-1))),
    "Shuttle": Recipe([SHUTTLE_TRN, SHUTTLE_TST],
                      lambda r: columns([unlzw(r[0]), r[1]], None, " ",
                                        lambda t: t[2:9])),
    "ConfLongDemo": Recipe(
        [Source(UCI + "00196/ConfLongDemo_JSI.txt", "1fbc965b8d7faf499ea854428b2138132d94e6a26404bc8e4b0c62144f513e8a")],
        lambda r: columns(r, ",", "\t", lambda t: t[4:7], fmt=excel_general)),

    "MNIST": Recipe(
        [Source(MNIST + "t10k-images-idx3-ubyte.gz", "8d422c7b0a1c1c79245a5bcf07fe86e33eeafee792b84584aec276f5a2dbc4e6"),
         Source(MNIST + "train-images-idx3-ubyte.gz", "440fcabf73cc546fa21475e81ea370265605f56be210a4024d2ca8f203523609")],
        pixels),
    "Fashion-MNIST": Recipe(
        [Source(KAGGLE_FASHION, "67fd73d163f0b3f4a8ce341a3b16bd528805b71442436dabb97c1a6f1d4015a6", "fashion-mnist_test.csv"),
         Source(KAGGLE_FASHION, "67fd73d163f0b3f4a8ce341a3b16bd528805b71442436dabb97c1a6f1d4015a6", "fashion-mnist_train.csv")],
        lambda r: columns(r, ",", " ", drop(0), skip=1)),
}


def sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def shape(path: Path) -> tuple[int, int]:
    n = d = 0
    with open(path, "rb") as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            if n == 0:
                d = len(t)
            n += 1
    return n, d


def load_manifest() -> dict[str, dict]:
    rows = [l for l in MANIFEST.read_text().splitlines()
            if l.strip() and not l.startswith("#")]
    header = rows[0].split("\t")
    return {r.split("\t")[0]: dict(zip(header, r.split("\t")))
            for r in rows[1:]}


def selected(names: list[str]) -> list[str]:
    manifest = load_manifest()
    for n in names:
        if n not in manifest:
            sys.exit(f"[ERROR] unknown dataset {n!r}; see manifest.tsv")
    return names or list(manifest)


def download(src: Source, cache: Path) -> Path:
    dest = cache / src.cache_name
    if dest.exists():
        return dest
    cache.mkdir(parents=True, exist_ok=True)
    print(f"[fetch ] {src.url}")
    req = urllib.request.Request(src.url, headers={"User-Agent": "prepare.py"})
    with urllib.request.urlopen(req, timeout=600) as r:
        data = r.read()
    got = sha256_bytes(data)
    if got != src.sha256:
        print(f"[WARN  ] {src.url}: sha256 {got} differs from the recorded "
              f"{src.sha256}; `build` will check the result")
    tmp = dest.with_suffix(dest.suffix + ".part")
    tmp.write_bytes(data)
    tmp.replace(dest)
    return dest


def read_source(src: Source, cache: Path) -> bytes:
    path = cache / src.cache_name
    if not path.exists():
        raise FileNotFoundError(f"{path} missing; run `prepare.py fetch`")
    data = path.read_bytes()
    if src.member:
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            data = z.read(src.member)
    return data


def cmd_fetch(args) -> int:
    bad = 0
    for name in selected(args.names):
        if RECIPES[name].make is None:
            print(f"[MANUAL] {name}: {RECIPES[name].manual}")
            continue
        for src in RECIPES[name].sources:
            try:
                download(src, args.cache)
            except OSError as e:
                print(f"[FAIL  ] {name}: {src.url}: {e}")
                bad += 1
    return 1 if bad else 0


def cmd_build(args) -> int:
    manifest = load_manifest()
    args.data.mkdir(parents=True, exist_ok=True)
    bad = 0
    for name in selected(args.names):
        recipe, want = RECIPES[name], manifest[name]["sha256"]
        dest = args.data / f"{name}.txt"
        if recipe.make is None:
            if dest.exists() and sha256_file(dest) == want:
                print(f"[ ok   ] {name}: manually placed file verified")
            else:
                print(f"[MANUAL] {name}: {recipe.manual}")
                bad += 1
            continue
        try:
            out = recipe.make([read_source(s, args.cache)
                               for s in recipe.sources])
        except FileNotFoundError as e:
            print(f"[FAIL  ] {name}: {e}")
            bad += 1
            continue
        got = sha256_bytes(out)
        if got != want:
            print(f"[FAIL  ] {name}: built sha256 {got} differs from "
                  f"manifest.tsv; not written")
            bad += 1
            continue
        tmp = dest.with_name(dest.name + ".part")
        tmp.write_bytes(out)
        tmp.replace(dest)
        print(f"[built ] {name}")
    return 1 if bad else 0


def cmd_verify(args) -> int:
    manifest = load_manifest()
    names = selected(args.names)
    bad = 0
    for name in names:
        e = manifest[name]
        path = args.data / f"{name}.txt"
        if not path.exists():
            print(f"[MISS  ] {name}: {path} not found")
            bad += 1
            continue
        n, d = shape(path)
        problems = []
        if int(e["n"]) != n:
            problems.append(f"n {n} != {e['n']}")
        if int(e["d"]) != d:
            problems.append(f"d {d} != {e['d']}")
        if sha256_file(path) != e["sha256"]:
            problems.append("sha256 differs")
        if problems:
            print(f"[FAIL  ] {name}: {'; '.join(problems)}")
            bad += 1
        elif args.verbose:
            print(f"[ ok   ] {name}: n={n} d={d}")
    print(f"{len(names) - bad}/{len(names)} datasets verified")
    return 1 if bad else 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name, func, text in (
            ("fetch", cmd_fetch, "download the originals into the cache"),
            ("build", cmd_build, "write data/<NAME>.txt from the cache"),
            ("verify", cmd_verify, "check n, d and sha256 against manifest.tsv")):
        p = sub.add_parser(name, help=text)
        p.add_argument("names", nargs="*", metavar="NAME")
        p.add_argument("--cache", type=Path, default=HERE / "sources",
                       help="downloaded originals (default: %(default)s)")
        p.add_argument("--data", type=Path, default=ROOT / "data",
                       help="dataset files (default: %(default)s)")
        p.set_defaults(func=func)
        if name == "verify":
            p.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
