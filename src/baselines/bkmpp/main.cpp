#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#include "io.hpp"
#include "kMeans.h"
#include "metric.hpp"
#include "stat.hpp"

using namespace std;
using namespace std::chrono;

int main(int argc, char **argv) {
  static const char *extraHelp =
    "  --labels DIR        write each run's assignment there, one cluster\n"
    "                      index per line, for bc-tau (R1-4b)\n";
  auto opt = protocol::parse(argc, argv, extraHelp, {"--labels"});
  if (opt.exitEarly)
    return 0;
  if (!opt.ok)
    return 2;

  const string algo_name = "BKMPP";
  const double terminationCriterionValue = 1.0;
  const bool stopWhenBalanced = false;
  const double partlyRemainingFraction = 0.15;
  const double increasingPenaltyFactor = 1.01;
  const bool useFunctionIter = true;
  const int switchPostp = 10;
  const auto terminationCriterion =
      KMeans::TerminationCriterion::MaxDiffClusterSizes;
  const string config = "max_iters=" + to_string(bkmpp_max_iters) +
                        ",term=MaxDiffClusterSizes,frac=0.15,penalty=1.01";

  protocol::report(opt, algo_name);
  BaselineHarness harness(opt, algo_name);
  if (!harness.ok())
    return 1;

  for (const auto &b : opt.datasets) {
    auto dataset = load_matrix(harness.datasetPath(b.name));
    if (dataset.empty()) {
      cerr << "[ERROR] Skipping " << b.name << ": load failed." << endl;
      continue;
    }
    const int n_data = static_cast<int>(dataset.size());
    const int dim = static_cast<int>(dataset[0].size());
    harness.requireShape(b, n_data, dim);

    for (int i = 0; i < opt.runs; ++i) {
      const unsigned int seed = protocol::seedFor(opt, i);
      if (harness.alreadyDone(b.name, seed))
        continue;
      srand(seed);

      auto start = system_clock::now();
      KMeans bkmpp;
      bkmpp.initialize(dataset, b.k);
      bkmpp.run(terminationCriterion, terminationCriterionValue,
                stopWhenBalanced, partlyRemainingFraction,
                increasingPenaltyFactor, useFunctionIter, switchPostp,
                bkmpp_max_iters);
      auto end = system_clock::now();

      int *raw = bkmpp.getLabels();
      vector<int> labels(raw, raw + n_data);
      delete[] raw;

      vector<vector<double>> centroids(b.k, vector<double>(dim, 0.0));
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
      r.config = config;
            harness.record(r, labels);
    }
    harness.finishDataset(b.name);
  }
  return 0;
}
