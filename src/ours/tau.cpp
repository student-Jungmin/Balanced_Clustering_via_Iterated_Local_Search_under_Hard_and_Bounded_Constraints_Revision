#include <cmath>
#include "Algorithm.hpp"
#include <fstream>
#include <iostream>
#include <map>

namespace {

vector<int> readLabels(const fs::path &path) {
  ifstream in(path);
  vector<int> labels;
  for (int label; in >> label;)
    labels.push_back(label);
  return labels;
}

double scoreLabels(const vector<int> &labels, const vector<double> &data,
                   int nData, int nDim, int nCluster) {
  vector<Coordinate> centroid(nCluster, Coordinate(nDim, 0.0));
  vector<int> size(nCluster, 0);
  for (int i = 0; i < nData; ++i) {
    const int c = labels[i];
    size[c]++;
    for (int d = 0; d < nDim; ++d)
      centroid[c][d] += data[static_cast<size_t>(i) * nDim + d];
  }
  for (int c = 0; c < nCluster; ++c)
    if (size[c] > 0)
      for (int d = 0; d < nDim; ++d)
        centroid[c][d] /= size[c];

  double sse = 0.0;
  for (int i = 0; i < nData; ++i)
    for (int d = 0; d < nDim; ++d) {
      const double diff =
          centroid[labels[i]][d] - data[static_cast<size_t>(i) * nDim + d];
      sse += diff * diff;
    }
  return sse;
}

void usage() {
  cerr << "usage: bc-tau --dataset NAME --labels FILE --rho R [--data DIR]\n"
          "  Applies the bounded-balance relaxation to the given assignment and\n"
          "  reports MSE and cluster-size spread before and after.\n";
}

}

int main(int argc, char **argv) {
  map<string, string> arg;
  for (int i = 1; i + 1 < argc; i += 2)
    arg[argv[i]] = argv[i + 1];
  if (!arg.count("--dataset") || !arg.count("--labels") || !arg.count("--rho")) {
    usage();
    return 2;
  }

  const fs::path dataDir = arg.count("--data") ? arg["--data"] : "data";
  const string name = arg["--dataset"];
  auto loaded = DatasetLoader::load(dataDir / (name + ".txt"));
  if (!loaded) {
    cerr << "[ERROR] could not load " << name << "\n";
    return 1;
  }
  auto labels = readLabels(arg["--labels"]);
  if (static_cast<int>(labels.size()) != loaded->nData) {
    cerr << "[ERROR] " << arg["--labels"] << " has " << labels.size()
         << " labels for " << loaded->nData << " points\n";
    return 1;
  }
  const int lowest = *min_element(labels.begin(), labels.end());
  if (lowest < 0) {
    cerr << "[ERROR] label file holds a negative cluster index (" << lowest
         << ")" << endl;
    return 1;
  }
  const int nCluster = 1 + *max_element(labels.begin(), labels.end());

  Config config;
  config.nCluster = nCluster;
  config.enableTau = true;
  config.tauRatio = stod(arg["--rho"]);

  Algorithm algorithm(config, loaded->flatData, loaded->nData, loaded->nDim);

  auto &best = algorithm.bestOutput;
  vector<int> filled(nCluster, 0);
  for (int i = 0; i < loaded->nData; ++i) {
    const int c = labels[i];
    if (filled[c] >= static_cast<int>(best.clusters[c].size()))
      best.clusters[c].resize(filled[c] + 1);
    best.clusters[c][filled[c]++] = {0.0, i};
  }
  for (int c = 0; c < nCluster; ++c)
    best.capacity[c] = filled[c];
  for (int c = 0; c < nCluster; ++c) {
    fill(best.centroids[c].begin(), best.centroids[c].end(), 0.0);
    for (int e = 0; e < best.capacity[c]; ++e) {
      const int i = best.clusters[c][e].second;
      for (int d = 0; d < loaded->nDim; ++d)
        best.centroids[c][d] += loaded->flatData[static_cast<size_t>(i) * loaded->nDim + d];
    }
    if (best.capacity[c] > 0)
      for (int d = 0; d < loaded->nDim; ++d)
        best.centroids[c][d] /= best.capacity[c];
  }

  const double before = scoreLabels(labels, loaded->flatData, loaded->nData,
                                    loaded->nDim, nCluster);
  const auto beforeRange =
      minmax_element(best.capacity.begin(), best.capacity.end());
  const int beforeSpread = *beforeRange.second - *beforeRange.first;

  if (*beforeRange.first < loaded->nData / nCluster ||
      *beforeRange.second > loaded->nData / nCluster +
                                (loaded->nData % nCluster ? 1 : 0)) {
    cerr << "[ERROR] input is not hard-balanced: sizes " << *beforeRange.first
         << ".." << *beforeRange.second << " for n/k = "
         << loaded->nData / nCluster << endl;
    return 1;
  }

  const int tau = record::tauEff(loaded->nData, nCluster, config.tauRatio);
  if (!record::relaxationIsNoop(loaded->nData, nCluster, tau))
    algorithm.tauConversion();

  vector<int> after(loaded->nData, 0);
  for (int c = 0; c < nCluster; ++c)
    for (int e = 0; e < best.capacity[c]; ++e)
      after[best.clusters[c][e].second] = c;
  const double afterSse = scoreLabels(after, loaded->flatData, loaded->nData,
                                      loaded->nDim, nCluster);
  const auto afterRange =
      minmax_element(best.capacity.begin(), best.capacity.end());
  const int afterSpread = *afterRange.second - *afterRange.first;
  const bool feasible = record::withinBalance(
      *afterRange.first, *afterRange.second, loaded->nData, nCluster, tau);

  cout << "dataset\tn\td\tk\trho\ttau\tmse_before\tmse_after\tgain_pct"
          "\tspread_before\tspread_after\tfeasible\n";
  cout << name << '\t' << loaded->nData << '\t' << loaded->nDim << '\t'
       << nCluster << '\t' << config.tauRatio << '\t' << tau << '\t'
       << setprecision(17) << before / loaded->nData << '\t'
       << afterSse / loaded->nData << '\t' << setprecision(6)
       << 100.0 * (1.0 - afterSse / before) << '\t' << beforeSpread << '\t'
       << afterSpread << '\t' << (feasible ? 1 : 0) << '\n';
  return 0;
}
