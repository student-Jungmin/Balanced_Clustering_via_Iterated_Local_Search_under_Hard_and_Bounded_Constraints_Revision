#include <algorithm>
#include <chrono>
#include <float.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "DistanceMatrix.h"
#include "Pair.h"
#include "Random.h"
#include "Vns.h"

#include "SummaryReader.hpp"
#include "io.hpp"
#include "metric.hpp"
#include "stat.hpp"

using namespace std;
using namespace std::chrono;

static void runLima(const protocol::Options &opt, double scale,
                    const map<string, double> &budgets) {
  const string algo_name = "LIMAx" + protocol::formatMultiplier(scale);
  protocol::report(opt, algo_name);
  BaselineHarness harness(opt, algo_name);
  if (!harness.ok())
    return;

  for (const auto &b : opt.datasets) {
    auto it = budgets.find(b.name);
    if (it == budgets.end()) {
      cerr << "[ERROR] Skipping " << b.name
           << ": no time budget in the summary. Run ours over this dataset "
              "first." << endl;
      continue;
    }
    const double max_time = it->second * scale;

    auto dataset = load_matrix(harness.datasetPath(b.name));
    if (dataset.empty()) {
      cerr << "[ERROR] Skipping " << b.name << ": load failed." << endl;
      continue;
    }
    const int n_data = static_cast<int>(dataset.size());
    const int dim = static_cast<int>(dataset[0].size());
    harness.requireShape(b, n_data, dim);

    const int kMin = 2;
    const int kMax = n_data / 2;
    const int kStep = kMax / 20;

    vector<Point> data(n_data);
    for (int p = 0; p < n_data; ++p)
      data[p] = {dataset[p]};

    DistanceMatrix distances(&data);
    vector<vector<Pair>> rankedEntities(n_data);
    for (int o = 0; o < n_data; ++o) {
      for (int m = 0; m < n_data; ++m)
        if (o != m)
          rankedEntities[o].push_back(Pair(m, distances.getDistance(o, m)));
      sort(rankedEntities[o].begin(), rankedEntities[o].end());
    }

    for (int i = 0; i < opt.runs; ++i) {
      const unsigned int seed = protocol::seedFor(opt, i);
      if (harness.alreadyDone(b.name, seed))
        continue;
      Random random(seed);
      Solution solution(b.k, n_data, &distances);
      stringstream ss;

      double elapsed_time = 0;
      Vns vns(&data, &distances, b.k, &random, &rankedEntities);
      const int iter = vns.execute(solution, max_time, kMin, kStep, kMax,
                                   ss.str(), &elapsed_time);

      vector<int> labels(solution.assignment, solution.assignment + n_data);
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
      r.timeSeconds = elapsed_time;
      r.iters = iter;
      {
        ostringstream cfg;
        cfg << "budget_x" << scale << "=" << max_time << "s,kMin=" << kMin
            << ",kMax=" << kMax << ",kStep=" << kStep;
        r.config = cfg.str();
      }
            harness.record(r, labels);
    }
    harness.finishDataset(b.name);
  }
}

int main(int argc, char **argv) {
  const char *extraHelp =
      "  --budget FILE       summary written by ours, supplying the per-dataset\n"
      "                      time budget (default results/benchmark/ours_summary.txt)\n"
      "  --scale-factor X    budget multiplier, fractional allowed; repeatable\n"
      "                      (default 1, 100)\n"
    "  --labels DIR        write each run's assignment there, one cluster\n"
    "                      index per line, for bc-tau (R1-4b)\n";
  auto opt = protocol::parse(argc, argv, extraHelp, {"--budget", "--scale-factor", "--labels"});
  if (opt.exitEarly)
    return 0;
  if (!opt.ok)
    return 2;

  auto budgetArgs = protocol::extraValues(opt, "--budget");
  const filesystem::path budgetPath =
      budgetArgs.empty() ? filesystem::path("results/benchmark/ours_summary.txt")
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

  auto scaleArgs = protocol::extraValues(opt, "--scale-factor");
  vector<double> scales;
  for (const auto &s : scaleArgs)
    scales.push_back(stod(s));
  if (scales.empty())
    scales = {1, 100};

  for (double s : scales)
    runLima(opt, s, budgets);
  return 0;
}
