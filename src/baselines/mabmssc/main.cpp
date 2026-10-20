#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

#include "SummaryReader.hpp"
#include "io.hpp"
#include "ma_bmssc.hpp"
#include "metric.hpp"
#include "stat.hpp"

using namespace std;
using namespace std::chrono;

static void runMaBmssc(const protocol::Options &opt, double scale,
                       const map<string, double> &budgets) {
  const string algo_name =
      scale == 1.0 ? "MABMSSC"
                   : "MABMSSCx" + protocol::formatMultiplier(scale);
  protocol::report(opt, algo_name);
  BaselineHarness harness(opt, algo_name);
  if (!harness.ok())
    return;

  for (const auto &b : opt.datasets) {
    auto budget = budgets.find(b.name);
    if (budget == budgets.end()) {
      cerr << "[ERROR] Skipping " << b.name
           << ": no time budget in the summary. Run ours over this dataset "
              "first." << endl;
      continue;
    }
    const double maxTime = budget->second * scale;

    if (maxTime < 0.01) {
      cerr << "[WARN ] " << b.name << ": budget is " << maxTime
           << "s. MA-BMSSC needs orders of magnitude more to converge; raise "
              "--scale-factor." << endl;
    }

    const string path = harness.datasetPath(b.name);
    auto dataset = load_matrix(path);
    if (dataset.empty()) {
      cerr << "[ERROR] Skipping " << b.name << ": load failed." << endl;
      continue;
    }
    const int n_data = static_cast<int>(dataset.size());
    const int dim = static_cast<int>(dataset[0].size());
    harness.requireShape(b, n_data, dim);

    N = n_data;
    D = dim;
    initialing(path);
    K = b.k;
    allocateMemory();
    buildNeighbors();

    for (int i = 0; i < opt.runs; ++i) {
      const unsigned int seed = protocol::seedFor(opt, i);
      if (harness.alreadyDone(b.name, seed))
        continue;
      srand(seed);

      auto start = system_clock::now();
      StartTime = clock();
      memetic(maxTime);
      auto end = system_clock::now();

      vector<int> labels(BestS, BestS + n_data);
      if (std::any_of(labels.begin(), labels.end(),
                      [&](int c) { return c < 0 || c >= b.k; })) {
        cerr << "[ERROR] " << b.name << " seed " << seed
             << ": MA-BMSSC returned no valid partition; skipping this run."
             << endl;
        continue;
      }
      vector<vector<double>> centroids(b.k, vector<double>(dim, 0.0));
      vector<int> count(b.k, 0);
      for (int p = 0; p < n_data; ++p) {
        ++count[labels[p]];
        for (int j = 0; j < dim; ++j)
          centroids[labels[p]][j] += dataset[p][j];
      }
      for (int c = 0; c < b.k; ++c)
        if (count[c])
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
      {
        ostringstream cfg;
        cfg << "budget_x" << scale << "=" << maxTime << "s,pop=5,rts_iter="
            << RTS_ITER;
        r.config = cfg.str();
      }
            harness.record(r, labels);
    }

    harness.finishDataset(b.name);
    freeMemory1();
    freeMemory();
  }
}

int main(int argc, char **argv) {
  const char *extraHelp =
      "  --budget FILE       summary written by ours, supplying the per-dataset\n"
      "                      time budget (default results/benchmark/ours_summary.txt)\n"
      "  --scale-factor X    budget multiplier, fractional allowed; repeatable\n"
      "                      (default 1)\n"
      "  --rts-iter N        responsive threshold search iterations (default 100)\n"
    "  --labels DIR        write each run's assignment there, one cluster\n"
    "                      index per line, for bc-tau (R1-4b)\n";
  auto opt = protocol::parse(argc, argv, extraHelp, {"--budget", "--scale-factor", "--rts-iter", "--labels"});
  if (opt.exitEarly)
    return 0;
  if (!opt.ok)
    return 2;

  auto budgetArgs = protocol::extraValues(opt, "--budget");
  const filesystem::path budgetPath =
      budgetArgs.empty()
          ? filesystem::path("results/benchmark/ours_summary.txt")
          : filesystem::path(budgetArgs.back());
  auto budgets = summary::timeMeans(budgetPath);
  if (budgets.empty()) {
    cerr << "[ERROR] No time budgets read from " << budgetPath << ".\n"
         << "[HINT ] Run `make run RUNS=10` first, or pass --budget FILE."
         << endl;
    return 1;
  }
  cout << "[INFO] budgets   : " << budgets.size() << " datasets from "
       << budgetPath << endl;

  auto rtsArgs = protocol::extraValues(opt, "--rts-iter");
  RTS_ITER = rtsArgs.empty() ? 100 : stoi(rtsArgs.back());
  Runs = opt.runs;

  vector<double> scales;
  for (const auto &s : protocol::extraValues(opt, "--scale-factor"))
    scales.push_back(stod(s));
  if (scales.empty())
    scales = {1};

  for (double s : scales)
    runMaBmssc(opt, s, budgets);
  return 0;
}
