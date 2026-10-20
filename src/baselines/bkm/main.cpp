#include <iostream>

#include "bkm.hpp"
#include "io.hpp"
#include "metric.hpp"
#include "stat.hpp"

using namespace std;

int main(int argc, char **argv) {
  static const char *extraHelp =
    "  --labels DIR        write each run's assignment there, one cluster\n"
    "                      index per line, for bc-tau (R1-4b)\n";
  auto opt = protocol::parse(argc, argv, extraHelp, {"--labels"});
  if (opt.exitEarly)
    return 0;
  if (!opt.ok)
    return 2;

  const string algo_name = "BKM";
  const string config = "max_iters=" + to_string(max_iters);

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

      BKM algo(max_iters, b.k, dataset, seed);
      algo.run();

      const auto labels = algo.get_labels();
      const auto centroids = algo.get_centroids();

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
      r.timeSeconds = algo.get_elapsed_time();
      r.config = config;
            harness.record(r, labels);
    }
    harness.finishDataset(b.name);
  }
  return 0;
}
