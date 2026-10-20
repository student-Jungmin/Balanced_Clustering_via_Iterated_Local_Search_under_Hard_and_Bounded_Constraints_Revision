#pragma once
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace bench {

enum class Scale { Standard, Large };

struct Benchmark {
  std::string name;
  int k;
  Scale scale;
  int n;
  int d;
  bool explicit_only = false;
};

inline const std::vector<Benchmark> &all() {
  static const std::vector<Benchmark> registry = {
      {"A1", 20, Scale::Standard, 3000, 2},
      {"A2", 35, Scale::Standard, 5250, 2},
      {"A3", 50, Scale::Standard, 7500, 2},
      {"S1", 15, Scale::Standard, 5000, 2},
      {"S2", 15, Scale::Standard, 5000, 2},
      {"S3", 15, Scale::Standard, 5000, 2},
      {"S4", 15, Scale::Standard, 5000, 2},
      {"dim32", 16, Scale::Standard, 1024, 32},
      {"unbalance", 8, Scale::Standard, 6500, 2},
      {"Glass", 7, Scale::Standard, 214, 9},
      {"Image Segmentation", 7, Scale::Standard, 2310, 19},
      {"Iris", 3, Scale::Standard, 150, 4},
      {"Libras", 15, Scale::Standard, 360, 90},
      {"Multiple Features", 10, Scale::Standard, 2000, 240},
      {"Synthetic Control", 6, Scale::Standard, 600, 60},
      {"Thyroid", 3, Scale::Standard, 215, 5},
      {"User Knowledge", 4, Scale::Standard, 403, 5},
      {"User Model", 4, Scale::Standard, 258, 5},
      {"Vehicle", 6, Scale::Standard, 846, 18},
      {"Vowel Recognition", 6, Scale::Standard, 871, 3},
      {"Wine", 3, Scale::Standard, 178, 13},
      {"Yeast", 10, Scale::Standard, 1484, 8},
      {"Body", 2, Scale::Standard, 507, 5},
      {"Breast Cancer", 2, Scale::Standard, 569, 30},
      {"Ionosphere", 2, Scale::Standard, 351, 34},
      {"Water", 13, Scale::Standard, 527, 38},
      {"Cryotherapy", 2, Scale::Standard, 90, 6},
      {"G2", 2, Scale::Standard, 2048, 16},
      {"Birch1", 100, Scale::Large, 100000, 2},
      {"Birch2", 100, Scale::Large, 100000, 2},
      {"Worms2", 35, Scale::Large, 105600, 2},
      {"Worms64", 256, Scale::Large, 105000, 64},
      {"Letter Recognition", 26, Scale::Large, 20000, 16},
      {"Sensorless Drive", 11, Scale::Large, 58509, 48},
      {"Shuttle", 7, Scale::Large, 58000, 7},
      {"ConfLongDemo", 11, Scale::Large, 164860, 3},
      {"MNIST", 10, Scale::Large, 70000, 784},
      {"Fashion-MNIST", 10, Scale::Large, 70000, 784},
      {"Santa", 20, Scale::Large, 1437195, 2},
  };
  return registry;
}

struct Retired {
  std::string name;
  std::string reason;
};

inline const std::vector<Retired> &retired() {
  static const std::vector<Retired> r = {
      {"Water Treatment Plant",
       "ran for RKM/TBC and for ours' r20 sweep but never for BKM/BKMPP/LIMA; "
       "no complete row exists"},
      {"Birch3", "ran for RKM/TBC only; no complete row exists"},
      {"Libra", "BKM/BKMPP/LIMA wrote their results under this name while "
                "RKM/TBC and ours used `Libras`; the two files are "
                "byte-identical, so it was a naming split, not two datasets"},
      {"MNIST(test)", "BKM/BKMPP/LIMA only; superseded by the full `MNIST`"},
      {"KDDCUP04Bio", "present in data/ but never entered any reported table"},
  };
  return r;
}

inline std::vector<Benchmark> byScale(Scale s) {
  std::vector<Benchmark> out;
  for (const auto &b : all())
    if (b.scale == s && !b.explicit_only)
      out.push_back(b);
  return out;
}

inline std::optional<Benchmark> byName(const std::string &name) {
  for (const auto &b : all())
    if (b.name == name)
      return b;
  return std::nullopt;
}

inline std::string verify(const Benchmark &b, int n, int d) {
  if (b.n == n && b.d == d)
    return "";
  std::string msg = b.name + ": expected " + std::to_string(b.n) + "x" +
                    std::to_string(b.d) + ", loaded " + std::to_string(n) +
                    "x" + std::to_string(d);
  if (d == 1 && b.d > 1)
    msg += " (one dimension usually means the file is comma-separated; "
           "run `python3 data/prepare/prepare.py verify`)";
  return msg;
}

inline const char *scaleName(Scale s) {
  return s == Scale::Standard ? "standard" : "large";
}

}
