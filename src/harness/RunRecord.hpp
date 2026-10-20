#pragma once
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <fstream>
#include <iomanip>
#include <ios>
#include <string>
#include <utility>
#include <vector>

namespace record {

struct Run {
  std::string algorithm;
  std::string dataset;
  std::string scale;
  int n = 0;
  int d = 0;
  int k = 0;
  unsigned int seed = 0;
  int run = 0;
  double mse = 0.0;
  double sse = 0.0;
  double timeSeconds = 0.0;
  long long iters = -1;
  int minClusterSize = -1;
  int maxClusterSize = -1;
  bool feasible = true;
  double tau = 0.0;
  double rho = 0.0;
  std::string config;
};

inline const std::vector<std::string> &columns() {
  static const std::vector<std::string> c = {
      "algorithm", "dataset", "scale",   "n",        "d",
      "k",         "seed",    "run",     "mse",      "sse",
      "time_s",    "iters",   "min_size", "max_size", "feasible",
      "tau",       "rho",     "config"};
  return c;
}

class Writer {
public:
  Writer() = default;

  explicit Writer(const std::filesystem::path &path) { open(path); }

  void open(const std::filesystem::path &path) {
    if (path.empty())
      return;
    if (path.has_parent_path())
      std::filesystem::create_directories(path.parent_path());

    const bool fresh = !std::filesystem::exists(path) ||
                       std::filesystem::file_size(path) == 0;
    out_.open(path, std::ios::app);
    if (!out_.is_open())
      return;

    if (fresh) {
      for (std::size_t i = 0; i < columns().size(); ++i)
        out_ << columns()[i] << (i + 1 == columns().size() ? '\n' : '\t');
    }
    enabled_ = true;
  }

  bool enabled() const { return enabled_; }

  void write(const Run &r) {
    if (!enabled_)
      return;
    out_ << r.algorithm << '\t' << r.dataset << '\t' << r.scale << '\t' << r.n
         << '\t' << r.d << '\t' << r.k << '\t' << r.seed << '\t' << r.run
         << '\t';
    out_ << std::setprecision(17) << r.mse << '\t' << r.sse << '\t'
         << r.timeSeconds << '\t';
    out_ << std::defaultfloat << std::setprecision(6);
    out_ << r.iters << '\t' << r.minClusterSize << '\t' << r.maxClusterSize
         << '\t' << (r.feasible ? 1 : 0) << '\t' << r.tau << '\t' << r.rho
         << '\t' << r.config << '\n';
    out_.flush();
  }

private:
  std::ofstream out_;
  bool enabled_ = false;
};

inline std::set<std::string> existingKeys(const std::filesystem::path &path) {
  std::set<std::string> keys;
  std::ifstream in(path);
  if (!in)
    return keys;
  std::string line;
  std::getline(in, line);
  while (std::getline(in, line)) {
    std::size_t a = line.find('\t');
    if (a == std::string::npos)
      continue;
    std::size_t b = line.find('\t', a + 1);
    if (b == std::string::npos)
      continue;
    std::size_t p = b;
    for (int i = 0; i < 4 && p != std::string::npos; ++i)
      p = line.find('\t', p + 1);
    if (p == std::string::npos)
      continue;
    const std::size_t q = line.find('\t', p + 1);
    if (q == std::string::npos)
      continue;
    keys.insert(line.substr(0, b) + '\t' + line.substr(p + 1, q - p - 1));
  }
  return keys;
}

inline std::string key(const std::string &algorithm, const std::string &dataset,
                       unsigned int seed) {
  return algorithm + '\t' + dataset + '\t' + std::to_string(seed);
}

inline std::pair<int, int> sizeRange(const std::vector<int> &labels, int k) {
  if (k <= 0 || labels.empty())
    return {-1, -1};
  std::vector<int> counts(static_cast<std::size_t>(k), 0);
  for (int l : labels)
    if (l >= 0 && l < k)
      ++counts[static_cast<std::size_t>(l)];
  return {*std::min_element(counts.begin(), counts.end()),
          *std::max_element(counts.begin(), counts.end())};
}

inline int tauEff(int n, int k, double rho) {
  const int base = rho > 0.0
                       ? static_cast<int>(std::floor(rho * n / k + 1e-9))
                       : 0;
  return std::max(base, n % k != 0 ? 1 : 0);
}

inline bool relaxationIsNoop(int n, int k, int tau) {
  return n % k == 0 && tau <= 1;
}

inline bool withinBalance(int lo, int hi, int n, int k, double tau) {
  if (tau < 1.0) {
    const int base = n / k;
    return lo >= base && hi <= base + (n % k ? 1 : 0);
  }
  return hi - lo <= static_cast<int>(tau);
}

inline void setBalance(Run &r, const std::vector<int> &labels) {
  auto [lo, hi] = sizeRange(labels, r.k);
  r.minClusterSize = lo;
  r.maxClusterSize = hi;
  if (lo < 0 || r.k <= 0) {
    r.feasible = false;
    return;
  }
  r.feasible = withinBalance(lo, hi, r.n, r.k, r.tau);
}

}
