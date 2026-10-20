#include <chrono>
#include <iostream>

#include "io.hpp"
#include "metric.hpp"
#include "regularized_k_means.h"
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

  const KMeans::InitMethod init_method = KMeans::kForgy;
  const bool warm_start = true;
  const int n_jobs = 1;
  const string algo_name = "RKM";
  const string config = "init=forgy,warm_start=1,n_jobs=1";

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
      RegularizedKMeans rkm(dataset, b.k, init_method, warm_start, n_jobs, seed);

      auto start = system_clock::now();
      rkm.SolveHard();
      auto end = system_clock::now();

      const auto labels = rkm.assignments();
      const auto centroids = rkm.cluster_centers();

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
