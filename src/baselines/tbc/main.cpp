/*
  Modified version of TBC (Lin et al., 2022) from
  https://github.com/MKMaS-GUET/tau-balanced-clustering, commit efa7114
  (2021-10-29), distributed under the GNU General Public License v3.0
  (see LICENSE in this directory). This modified file is distributed under
  the same licence.

  Modified by Jeongmin Lee and Hyeon-Suk Na, 2026-09 to 2026-10. Changes:
  a "repaired" mode that enforces the declared balance condition
  max - min <= tau (a size cap tightened to the upper end of a per-iteration
  band [L, L + tau], and after every assignment step a repair that raises
  each undersized cluster to L by moving in the points that least increase
  the SSE); the per-run seed is passed to every run; the MSE is scored from
  the centroids of the returned labels; command-line handling for the
  benchmark harness. The distributed mode is otherwise unchanged.
*/
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "io.hpp"
#include "metric.hpp"
#include "stat.hpp"
#include "tau_balanced_clustering.hpp"

using namespace std;
using namespace std::chrono;

static void runTbc(const protocol::Options &opt, const string &algo_name,
                   double ratio, TBC::Balance balance) {
  protocol::report(opt, algo_name);
  BaselineHarness harness(opt, algo_name);
  if (!harness.ok())
    return;

  for (const auto &b : opt.datasets) {
    auto dataset = load_matrix(harness.datasetPath(b.name));
    if (dataset.empty()) {
      cerr << "[ERROR] Skipping " << b.name << ": load failed." << endl;
      continue;
    }
    const int n_data = static_cast<int>(dataset.size());
    const int dim = static_cast<int>(dataset[0].size());
    harness.requireShape(b, n_data, dim);
    const int tau = record::tauEff(n_data, b.k, ratio);

    for (int i = 0; i < opt.runs; ++i) {
      const unsigned int seed = protocol::seedFor(opt, i);
      if (harness.alreadyDone(b.name, seed))
        continue;
      TBC tbc(dataset, b.k, tau, seed, balance, /*band=*/false, /*pairwise=*/true);

      auto start = system_clock::now();
      tbc.start();
      auto end = system_clock::now();

      const auto labels = tbc.getAssignments();
      vector<vector<double>> centroids(b.k, vector<double>(dim, 0.0));
      if (balance == TBC::Balance::Repaired) {
        for (int c = 0; c < b.k; ++c)
          copy(tbc.cluster_centers_[c].value.begin(),
               tbc.cluster_centers_[c].value.end(), centroids[c].begin());
      } else {
        vector<int> count(b.k, 0);
        for (int p = 0; p < n_data; ++p) {
          ++count[labels[p]];
          for (int j = 0; j < dim; ++j)
            centroids[labels[p]][j] += dataset[p][j];
        }
        for (int c = 0; c < b.k; ++c)
          if (count[c] > 0)
            for (int j = 0; j < dim; ++j)
              centroids[c][j] /= count[c];
      }

      record::Run r;
      r.dataset = b.name;
      r.scale = bench::scaleName(b.scale);
      r.n = n_data;
      r.d = dim;
      r.k = b.k;
      r.seed = seed;
      r.run = i;
      r.mse = compute_mse(labels, centroids, dataset);
      r.sse = compute_sse(labels, centroids, dataset);
      r.timeSeconds = duration<double>(end - start).count();
      r.tau = tau;
      r.rho = ratio;
      r.config = "tau=" + to_string(tau) + ",rho=" + to_string(ratio) +
                 ",mode=" + (balance == TBC::Balance::Published ? "published"
                                                               : "repaired") +
                 (balance == TBC::Balance::Repaired && tau >= 2 ? ",band=pairwise" : "");
            harness.record(r, labels);
    }
    harness.finishDataset(b.name);
  }
}

static const char *kDefaultRhos[] = {"0", "0.01", "0.03", "0.05"};

static string nameFor(double ratio) {
  if (ratio < 1e-6)
    return "TBC";
  const int pct = static_cast<int>(ratio * 100.0 + 0.5);
  const double back = pct / 100.0;
  if (pct >= 1 && std::abs(back - ratio) < 1e-9)
    return "TBC" + to_string(pct);
  ostringstream ss;
  ss << "TBC_rho" << ratio;
  return ss.str();
}

int main(int argc, char **argv) {
  const char *extraHelp =
      "  --rho R             tau = floor(R*N/K); "
      "repeatable (default 0, 0.01, 0.03, 0.05)\n"
      "  --tbc-mode M        published (default) | repaired; repaired enforces\n"
      "                      max-min <= tau, which the published code does not\n"
    "  --labels DIR        write each run's assignment there, one cluster\n"
    "                      index per line, for bc-tau (R1-4b)\n";
  auto opt = protocol::parse(argc, argv, extraHelp, {"--rho", "--tbc-mode", "--labels"});
  if (opt.exitEarly)
    return 0;
  if (!opt.ok)
    return 2;

  auto rhos = protocol::extraValues(opt, "--rho");
  if (rhos.empty())
    rhos.assign(begin(kDefaultRhos), end(kDefaultRhos));

  auto modeArgs = protocol::extraValues(opt, "--tbc-mode");
  const string mode = modeArgs.empty() ? "published" : modeArgs.back();
  if (mode != "published" && mode != "repaired") {
    cerr << "[ERROR] --tbc-mode must be published or repaired" << endl;
    return 2;
  }
  const auto balance = mode == "repaired" ? TBC::Balance::Repaired
                                          : TBC::Balance::Published;
  const string suffix = mode == "repaired" ? "_repaired" : "";

  for (const auto &s : rhos) {
    const double ratio = stod(s);
    runTbc(opt, nameFor(ratio) + suffix, ratio, balance);
  }
  return 0;
}
