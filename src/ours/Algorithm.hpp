#pragma once
#include "Benchmarks.hpp"
#include "Config.hpp"
#include "Instrumentation.hpp"
#include "DatasetLoader.hpp"
#include "Output.hpp"
#include "RunRecord.hpp"
#include "Stats.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <queue>
#include <random>
#include <ranges>
#include <set>
#include <tuple>
#include <utility>

using namespace std;
using namespace std::chrono;
using Matrix = vector<double>;

struct CapacityConstraint {

  int baseCapacity;
  int maxCapacity;
  int nFullClusters;

  CapacityConstraint(int nData, int nCluster)
      : baseCapacity(nData / nCluster), maxCapacity(baseCapacity + 1),
        nFullClusters(nData % nCluster) {}
};

class Algorithm {
public:

  mt19937 gen;
  Config config;

  int nDim;
  int nData;
  int nCluster;
  const double *dataset;
  CapacityConstraint capacityConstraint;
  Matrix distance;
  Output curOutput;
  vector<double> clusterRadius;
  vector<int> nextClusterToTry;
  vector<vector<int>> preferredClusters;
  vector<int> prefValid;
  vector<pair<double, int>> prefScratch;
  vector<double> centroidsT;
  vector<pair<double, int>> firstProfits;
  vector<pair<double, int>> secondProfits;
  vector<pair<int, int>> overlappedClusterPairs;

  vector<double> sseHistory;
  Output prevOutput;
  Output bestOutput;
#ifdef ANALYSIS
  probe::Probe probe;
#endif

  static constexpr int kPrefPrefix = 8;
  static constexpr int kProfitPrefix = 8;

  void expandPreferences(int dataIdx) {
    const double *row = &distance[dataIdx * nCluster];
    for (int c = 0; c < nCluster; ++c)
      prefScratch[c] = {row[c], c};
    sort(prefScratch.begin(), prefScratch.end());
    for (int c = 0; c < nCluster; ++c)
      preferredClusters[dataIdx][c] = prefScratch[c].second;
    prefValid[dataIdx] = nCluster;
  }

  inline int preferenceAt(int dataIdx, int rank) {
    if (rank >= prefValid[dataIdx])
      expandPreferences(dataIdx);
    return preferredClusters[dataIdx][rank];
  }

  void rankPreferences() {
    const int prefix = min(nCluster, kPrefPrefix);
    for (int dataIdx = 0; dataIdx < nData; ++dataIdx) {
      const double *row = &distance[dataIdx * nCluster];
      for (int c = 0; c < nCluster; ++c)
        prefScratch[c] = {row[c], c};

      partial_sort(prefScratch.begin(), prefScratch.begin() + prefix,
                   prefScratch.end());

      for (int j = 0; j < prefix; ++j)
        preferredClusters[dataIdx][j] = prefScratch[j].second;
      prefValid[dataIdx] = prefix;
    }
  }

  void buildDistanceMatrix(const vector<Coordinate> &centroids) {
    for (int c = 0; c < nCluster; ++c)
      for (int dim = 0; dim < nDim; ++dim)
        centroidsT[dim * nCluster + c] = centroids[c][dim];

    for (int dataIdx = 0; dataIdx < nData; ++dataIdx) {
      const double *x = &dataset[dataIdx * nDim];
      double *out = &distance[dataIdx * nCluster];

      for (int c = 0; c < nCluster; ++c)
        out[c] = 0.0;

      for (int dim = 0; dim < nDim; ++dim) {
        const double xv = x[dim];
        const double *row = &centroidsT[dim * nCluster];

        for (int c = 0; c < nCluster; ++c) {
          const double diff = row[c] - xv;
          out[c] += diff * diff;
        }
      }
    }
  }

  double computeSquaredL2(const Coordinate &c1, int dataIdx) const {
    double sum = 0.0;
    int baseIdx = dataIdx * nDim;

    for (int dim = 0; dim < nDim; ++dim) {
      double diff = c1[dim] - dataset[baseIdx + dim];
      sum += (diff * diff);
    }
    return sum;
  }

  double computeSquaredL2(const Coordinate &c1, const Coordinate &c2) const {
    double sum = 0.0;
    for (int dim = 0; dim < nDim; ++dim) {
      double diff = c1[dim] - c2[dim];
      sum += (diff * diff);
    }
    return sum;
  }

  double computeSSE(const Output &output) const {
    double sse = 0.0;

    for (int clusterIdx = 0; clusterIdx < config.nCluster; ++clusterIdx) {
      const auto &centroid = output.centroids[clusterIdx];
      int nElements = output.capacity[clusterIdx];

      for (int elementIdx = 0; elementIdx < nElements; ++elementIdx) {
        int dataIdx = output.clusters[clusterIdx][elementIdx].second;
        sse += computeSquaredL2(centroid, dataIdx);
      }
    }

    return sse;
  }

  void initCentroids() {
    auto &centroids = curOutput.centroids;

    if (config.initMethod == InitMethod::Random) {
      vector<int> idx(nData);
      iota(idx.begin(), idx.end(), 0);
      shuffle(idx.begin(), idx.end(), gen);

      for (int centroidIdx = 0; centroidIdx < config.nCluster; ++centroidIdx) {
        int dataIdx = idx[centroidIdx];

        for (int dim = 0; dim < nDim; ++dim) {
          centroids[centroidIdx][dim] = dataset[dataIdx * nDim + dim];
        }
      }
    } else {
      uniform_int_distribution<int> uniform(0, nData - 1);
      int firstIdx = uniform(gen);

      for (int dim = 0; dim < nDim; ++dim) {
        centroids[0][dim] = dataset[firstIdx * nDim + dim];
      }

      vector<double> minDist(nData, numeric_limits<double>::max());

      for (int centroidIdx = 1; centroidIdx < config.nCluster; ++centroidIdx) {
        for (int dataIdx = 0; dataIdx < nData; ++dataIdx) {
          double dist = computeSquaredL2(centroids[centroidIdx - 1], dataIdx);
          if (dist < minDist[dataIdx]) {
            minDist[dataIdx] = dist;
          }
        }

        discrete_distribution<int> discrete(minDist.begin(), minDist.end());
        int nextIdx = discrete(gen);
        for (int dim = 0; dim < nDim; ++dim) {
          centroids[centroidIdx][dim] = dataset[nextIdx * nDim + dim];
        }
      }
    }
  }

  void updateCentroids() {
    auto &centroids = curOutput.centroids;
    auto &capacity = curOutput.capacity;
    auto &clusters = curOutput.clusters;

    for (int clusterIdx = 0; clusterIdx < config.nCluster; ++clusterIdx) {
      fill(centroids[clusterIdx].begin(), centroids[clusterIdx].end(), 0.0);

      for (int elementIdx = 0; elementIdx < capacity[clusterIdx];
           ++elementIdx) {
        int dataIdx = clusters[clusterIdx][elementIdx].second;
        int baseIdx = dataIdx * nDim;
        for (int dim = 0; dim < nDim; ++dim) {
          centroids[clusterIdx][dim] += dataset[baseIdx + dim];
        }
      }

      if (capacity[clusterIdx] > 0) {
        for (int dim = 0; dim < nDim; ++dim) {
          centroids[clusterIdx][dim] /= capacity[clusterIdx];
        }
      }
    }
  }

  void perturbation() {
#ifdef ANALYSIS
    BC_WATCH(watch);
#endif
    auto &iter = curOutput.iter;
    auto &centroids = curOutput.centroids;
    auto &capacity = curOutput.capacity;
    auto &clusters = curOutput.clusters;

    if (iter == 1)
      initCentroids();
    else
      updateCentroids();
#ifdef ANALYSIS
    BC_LAP(watch, probe.perturbation.updateCentroids);
#endif

    buildDistanceMatrix(centroids);
#ifdef ANALYSIS
    BC_LAP(watch, probe.perturbation.updateDistMatrix);
#endif

    rankPreferences();
#ifdef ANALYSIS
    BC_LAP(watch, probe.perturbation.rankPreferences);
#endif

    fill(capacity.begin(), capacity.end(), 0);

    fill(nextClusterToTry.begin(), nextClusterToTry.end(), 0);

    int remainingFullSlots = capacityConstraint.nFullClusters;

    vector<bool> isHeapified(config.nCluster, false);

    queue<int> q;
    for (int dataIdx = 0; dataIdx < nData; ++dataIdx)
      q.push(dataIdx);

    while (!q.empty()) {
      int dataIdx = q.front();
      q.pop();

      int closestIdx = preferenceAt(dataIdx, nextClusterToTry[dataIdx]++);
      auto &cluster = clusters[closestIdx];
      auto &curCap = capacity[closestIdx];

      if (curCap < capacityConstraint.baseCapacity) {
        cluster[curCap] = {distance[dataIdx * nCluster + closestIdx], dataIdx};
        curCap++;
      } else if (curCap == capacityConstraint.baseCapacity &&
                 remainingFullSlots > 0) {

        cluster[curCap] = {distance[dataIdx * nCluster + closestIdx], dataIdx};
        curCap++;
        remainingFullSlots--;

        make_heap(cluster.begin(), cluster.begin() + curCap);
        isHeapified[closestIdx] = true;
      } else {
        if (!isHeapified[closestIdx]) {
          make_heap(cluster.begin(), cluster.begin() + curCap);
          isHeapified[closestIdx] = true;
        }

        auto [worstDist, worstIdx] = cluster.front();

        if (distance[dataIdx * nCluster + closestIdx] < worstDist) {
          pop_heap(cluster.begin(), cluster.begin() + curCap);

          int evictedIdx = cluster[curCap - 1].second;
          q.push(evictedIdx);

          cluster[curCap - 1] = {distance[dataIdx * nCluster + closestIdx],
                                 dataIdx};
          push_heap(cluster.begin(), cluster.begin() + curCap);
        } else {
          q.push(dataIdx);
        }
      }
    }
#ifdef ANALYSIS
    BC_LAP(watch, probe.perturbation.assign);
#endif

#ifdef ANALYSIS
    probe.ranking.preferenceOffered += static_cast<long long>(nData) * nCluster;
    for (int i = 0; i < nData; ++i) {
      probe.ranking.preferenceConsumed += nextClusterToTry[i];
      probe.ranking.preferenceMaxDepth =
          max<long long>(probe.ranking.preferenceMaxDepth, nextClusterToTry[i]);
    }
#endif

    for (int clusterIdx = 0; clusterIdx < config.nCluster; ++clusterIdx) {
      if (!isHeapified[clusterIdx]) {
        auto &cluster = clusters[clusterIdx];
        auto &curCap = capacity[clusterIdx];

        make_heap(cluster.begin(), cluster.begin() + curCap);
        isHeapified[clusterIdx] = true;
      }

      auto [maxDist, dataIdx] = clusters[clusterIdx].front();
      clusterRadius[clusterIdx] = sqrt(maxDist);
    }
#ifdef ANALYSIS
    BC_LAP(watch, probe.perturbation.updateClusterRadius);
#endif
  }

  void refreshCluster(int clusterIdx) {
    auto &centroid = curOutput.centroids[clusterIdx];
    const int size = curOutput.capacity[clusterIdx];
    fill(centroid.begin(), centroid.end(), 0.0);
    for (int elementIdx = 0; elementIdx < size; ++elementIdx) {
      const int dataIdx = curOutput.clusters[clusterIdx][elementIdx].second;
      const int base = dataIdx * nDim;
      for (int dim = 0; dim < nDim; ++dim)
        centroid[dim] += dataset[base + dim];
    }
    if (size > 0)
      for (int dim = 0; dim < nDim; ++dim)
        centroid[dim] /= size;

    for (int dataIdx = 0; dataIdx < nData; ++dataIdx)
      distance[dataIdx * nCluster + clusterIdx] =
          computeSquaredL2(centroid, dataIdx);

    double worst = 0.0;
    for (int elementIdx = 0; elementIdx < size; ++elementIdx) {
      const int dataIdx = curOutput.clusters[clusterIdx][elementIdx].second;
      worst = max(worst, distance[dataIdx * nCluster + clusterIdx]);
    }
    clusterRadius[clusterIdx] = sqrt(worst);
  }

  void localSearch() {
    int nRepeatLocalSearch = 0;
    double prevSSE;
    curOutput.sse = computeSSE(curOutput);

    while (true) {
#ifdef ANALYSIS
      BC_WATCH(watch);
#endif
      nRepeatLocalSearch++;

      if (config.passCentroids && nRepeatLocalSearch > 1) {
        for (int clusterIdx = 0; clusterIdx < nCluster; ++clusterIdx)
          refreshCluster(clusterIdx);
        curOutput.sse = computeSSE(curOutput);
      }

      prevSSE = curOutput.sse;

      overlappedClusterPairs.clear();

      for (int firstClusterIdx = 0; firstClusterIdx < config.nCluster - 1;
           ++firstClusterIdx) {
        const auto &firstCentroid = curOutput.centroids[firstClusterIdx];

        for (int secondClusterIdx = firstClusterIdx + 1;
             secondClusterIdx < config.nCluster; ++secondClusterIdx) {
          if (config.enableGeo) {
            const auto &secondCentroid = curOutput.centroids[secondClusterIdx];
            double radiusSum = clusterRadius[firstClusterIdx] +
                               clusterRadius[secondClusterIdx];
            double diff = radiusSum * radiusSum -
                          computeSquaredL2(firstCentroid, secondCentroid);

            if (diff > 0)
              overlappedClusterPairs.push_back(
                  {firstClusterIdx, secondClusterIdx});
          } else
            overlappedClusterPairs.push_back(
                {firstClusterIdx, secondClusterIdx});
        }
      }
#ifdef ANALYSIS
      BC_LAP(watch, probe.localSearch.getClusterPairs);
      probe.pruning.total += (config.nCluster * (config.nCluster - 1)) / 2;
      probe.pruning.afterC1 += overlappedClusterPairs.size();
#endif

      vector<char> examined;
      if (config.rebuildPairs)
        examined.assign(static_cast<size_t>(nCluster) * nCluster, 0);

      size_t pairIdx = 0;
      while (true) {
      for (; pairIdx < overlappedClusterPairs.size(); ++pairIdx) {
        const auto [firstClusterIdx, secondClusterIdx] =
            overlappedClusterPairs[pairIdx];
        if (config.rebuildPairs)
          examined[static_cast<size_t>(firstClusterIdx) * nCluster +
                   secondClusterIdx] = 1;
#ifdef ANALYSIS
        BC_WATCH(pairWatch);
#endif

        double maxFirstProfit = numeric_limits<double>::lowest();
        double maxSecondProfit = numeric_limits<double>::lowest();

        int firstCapacity = curOutput.capacity[firstClusterIdx];
        int secondCapacity = curOutput.capacity[secondClusterIdx];
        Cluster &firstCluster = curOutput.clusters[firstClusterIdx];
        Cluster &secondCluster = curOutput.clusters[secondClusterIdx];

        for (int elementIdx = 0; elementIdx < firstCapacity; ++elementIdx) {
          auto dataIdx = firstCluster[elementIdx].second;
          double profit = distance[dataIdx * nCluster + firstClusterIdx] -
                          distance[dataIdx * nCluster + secondClusterIdx];

          firstProfits[elementIdx] = {profit, elementIdx};

          if (profit > maxFirstProfit)
            maxFirstProfit = profit;
        }

        for (int elementIdx = 0; elementIdx < secondCapacity; ++elementIdx) {
          auto dataIdx = secondCluster[elementIdx].second;
          double profit = distance[dataIdx * nCluster + secondClusterIdx] -
                          distance[dataIdx * nCluster + firstClusterIdx];
          secondProfits[elementIdx] = {profit, elementIdx};

          if (profit > maxSecondProfit)
            maxSecondProfit = profit;
        }

        if (config.enableGeo && (maxFirstProfit + maxSecondProfit <= 0)) {
#ifdef ANALYSIS
          BC_LAP(pairWatch, probe.localSearch.getProfits);
#endif
          continue;
        }
#ifdef ANALYSIS
        BC_LAP(pairWatch, probe.localSearch.getProfits);
        probe.pruning.afterC2++;
#endif

        int idxBound = min(firstCapacity, secondCapacity);
        int ordered = 0;

        if (config.ranking == Ranking::Full) {
          sort(firstProfits.begin(), firstProfits.begin() + firstCapacity,
               greater<>());
          sort(secondProfits.begin(), secondProfits.begin() + secondCapacity,
               greater<>());
          ordered = idxBound;
        } else {
          for (int m = min(idxBound, kProfitPrefix);; m = min(idxBound, m * 2)) {
            partial_sort(firstProfits.begin(), firstProfits.begin() + m,
                         firstProfits.begin() + firstCapacity, greater<>());
            partial_sort(secondProfits.begin(), secondProfits.begin() + m,
                         secondProfits.begin() + secondCapacity, greater<>());
            ordered = m;

            if (m == idxBound)
              break;
            if (firstProfits[m - 1].first + secondProfits[m - 1].first <= 0)
              break;
          }
        }
#ifdef ANALYSIS
        BC_LAP(pairWatch, probe.localSearch.rankProfits);
#endif

        auto rng = std::views::iota(0, ordered);

        auto it = std::ranges::partition_point(rng, [&](int mid) {
          return firstProfits[mid].first + secondProfits[mid].first > 0;
        });
#ifdef ANALYSIS
        BC_LAP(pairWatch, probe.localSearch.findSwapCount);
#endif

        int swapCount = std::distance(rng.begin(), it);
#ifdef ANALYSIS
        probe.ranking.profitRangeFull += idxBound;
        probe.ranking.profitRangeOrdered += ordered;
        probe.ranking.profitConsumed += swapCount;
#endif

        for (int elementIdx = 0; elementIdx < swapCount; ++elementIdx) {
          auto &[firstProfit, firstElementIdx] = firstProfits[elementIdx];
          auto &[secondProfit, secondElementIdx] = secondProfits[elementIdx];
          curOutput.sse -= (firstProfit + secondProfit);

          swap(firstCluster[firstElementIdx], secondCluster[secondElementIdx]);
        }

        if (config.exactCentroids && swapCount > 0) {
          refreshCluster(firstClusterIdx);
          refreshCluster(secondClusterIdx);
          curOutput.sse = computeSSE(curOutput);
        }
#ifdef ANALYSIS
        BC_LAP(pairWatch, probe.localSearch.swap);
#endif
      }

        if (!config.rebuildPairs)
          break;
        const size_t beforeRebuild = overlappedClusterPairs.size();
        for (int c = 0; c < nCluster; ++c)
          refreshCluster(c);
        curOutput.sse = computeSSE(curOutput);
        for (int a = 0; a < nCluster - 1; ++a)
          for (int b = a + 1; b < nCluster; ++b) {
            if (examined[static_cast<size_t>(a) * nCluster + b])
              continue;
            const double radiusSum = clusterRadius[a] + clusterRadius[b];
            if (radiusSum * radiusSum -
                    computeSquaredL2(curOutput.centroids[a],
                                     curOutput.centroids[b]) >
                0)
              overlappedClusterPairs.push_back({a, b});
          }
        if (overlappedClusterPairs.size() == beforeRebuild)
          break;
      }

#ifdef ANALYSIS
      BC_SKIP(watch);
#endif

      const bool budgetSpent = curOutput.iter == nRepeatLocalSearch;
      const bool passGainedNothing = fabs(curOutput.sse - prevSSE) < 0.001;

      if (config.localSearch == LocalSearch::Once)
        break;
      else if (config.localSearch == LocalSearch::Sync && budgetSpent)
        break;
      else if (config.localSearch == LocalSearch::SyncEarly &&
               (budgetSpent || passGainedNothing))
        break;
      else if (config.localSearch == LocalSearch::Adaptive && passGainedNothing)
        break;

      for (int clusterIdx = 0; clusterIdx < config.nCluster; ++clusterIdx) {
        clusterRadius[clusterIdx] = numeric_limits<double>::lowest();
        int capacity = curOutput.capacity[clusterIdx];

        for (int elementIdx = 0; elementIdx < capacity; ++elementIdx) {
          int dataIdx = curOutput.clusters[clusterIdx][elementIdx].second;
          double dist = distance[dataIdx * nCluster + clusterIdx];

          if (dist > clusterRadius[clusterIdx])
            clusterRadius[clusterIdx] = dist;
        }

        clusterRadius[clusterIdx] = sqrt(clusterRadius[clusterIdx]);
      }
#ifdef ANALYSIS
      BC_LAP(watch, probe.localSearch.updateClusterRadius);
#endif
    }
  }

  int effectiveTau() const {
    return record::tauEff(nData, config.nCluster,
                          config.hardTau ? 0.0 : config.tauRatio);
  }

  void tauConversion() {
#ifdef ANALYSIS
    BC_WATCH(watch);
#endif

    auto &centroids = bestOutput.centroids;
    auto &clusters = bestOutput.clusters;
    auto &capacity = bestOutput.capacity;
    auto &sse = bestOutput.sse;

    const int tau = effectiveTau();

    for (auto &cluster : clusters)
      cluster.resize(capacityConstraint.maxCapacity + tau);

    vector<pair<int, int>> labels(nData);

    for (int clusterIdx = 0; clusterIdx < config.nCluster; ++clusterIdx) {
      int curCap = capacity[clusterIdx];
      const auto &cluster = clusters[clusterIdx];

      for (int elementIdx = 0; elementIdx < curCap; ++elementIdx) {
        int dataIdx = cluster[elementIdx].second;
        labels[dataIdx] = {clusterIdx, elementIdx};
      }
    }
#ifdef ANALYSIS
    BC_LAP(watch, probe.tauConversion.readyForTau);
#endif

    constexpr int kMaxRelaxRounds = 1;
    double kMinRoundGain = 0.001;
    for (const auto &t : config.terminations)
      if (t.type == TerminationType::CV)
        kMinRoundGain = t.param2;
    finalizeCentroids();
    double roundStartSse = bestOutput.sse;
    for (int round = 1;; ++round) {
      buildDistanceMatrix(centroids);
#ifdef ANALYSIS
      BC_LAP(watch, probe.tauConversion.updateDistMatrix);
#endif

      rankPreferences();
#ifdef ANALYSIS
      BC_LAP(watch, probe.tauConversion.rankPreferences);
#endif

      vector<int> nTry(nData, 0);
      priority_queue<tuple<double, int, int>> pq;

      multiset<int> activeCaps(capacity.begin(), capacity.end());

      for (int dataIdx = 0; dataIdx < nData; ++dataIdx) {
        int srcIdx = labels[dataIdx].first;
        int tarIdx = preferenceAt(dataIdx, nTry[dataIdx]);
        nTry[dataIdx]++;

        if (srcIdx == tarIdx)
          continue;

        double srcDist = distance[dataIdx * nCluster + srcIdx];
        double tarDist = distance[dataIdx * nCluster + tarIdx];
        double profit = srcDist - tarDist;

        if (profit > 0)
          pq.push({profit, tarIdx, dataIdx});
      }

      vector<Coordinate> liveCentroids;
      if (config.revalidateTau)
        liveCentroids = centroids;

      auto shiftCentroid = [&](int clusterIdx, int dataIdx, int oldSize,
                               double sign) {
        Coordinate &c = liveCentroids[clusterIdx];
        const int newSize = oldSize + static_cast<int>(sign);
        if (newSize <= 0)
          return;
        const int base = dataIdx * nDim;
        for (int dim = 0; dim < nDim; ++dim)
          c[dim] = (c[dim] * oldSize + sign * dataset[base + dim]) / newSize;
      };

      int moved = 0;
      while (!pq.empty()) {
        auto [profit, tarIdx, dataIdx] = pq.top();
        pq.pop();

        auto [srcIdx, elementIdx] = labels[dataIdx];

        if (config.revalidateTau) {
          const double live = computeSquaredL2(liveCentroids[srcIdx], dataIdx) -
                              computeSquaredL2(liveCentroids[tarIdx], dataIdx);
          if (live <= 0)
            continue;
          if (live < profit * (1 - 1e-12)) {
            pq.push({live, tarIdx, dataIdx});
            continue;
          }
          profit = live;
        }
        auto &srcCap = capacity[srcIdx];
        auto &tarCap = capacity[tarIdx];

        activeCaps.erase(activeCaps.find(srcCap));
        activeCaps.erase(activeCaps.find(tarCap));
        activeCaps.insert(srcCap - 1);
        activeCaps.insert(tarCap + 1);
        const bool withinSpread =
            *activeCaps.rbegin() - *activeCaps.begin() <= tau;

        if (!withinSpread) {
          activeCaps.erase(activeCaps.find(srcCap - 1));
          activeCaps.erase(activeCaps.find(tarCap + 1));
          activeCaps.insert(srcCap);
          activeCaps.insert(tarCap);
          int nextTarIdx = preferenceAt(dataIdx, nTry[dataIdx]);
          nTry[dataIdx]++;

          double srcDist = distance[dataIdx * nCluster + srcIdx];
          double tarDist = distance[dataIdx * nCluster + nextTarIdx];
          double newProfit = srcDist - tarDist;

          if (newProfit > 0)
            pq.push({newProfit, nextTarIdx, dataIdx});
          continue;
        }

        auto &srcCluster = clusters[srcIdx];
        auto &tarCluster = clusters[tarIdx];

        tarCluster[tarCap] = srcCluster[elementIdx];
        srcCluster[elementIdx] = srcCluster[srcCap - 1];

        labels[srcCluster[elementIdx].second].second = elementIdx;
        labels[dataIdx] = {tarIdx, tarCap};

        if (config.revalidateTau) {
          shiftCentroid(srcIdx, dataIdx, srcCap, -1.0);
          shiftCentroid(tarIdx, dataIdx, tarCap, +1.0);
        }

        tarCap += 1;
        srcCap -= 1;
        sse -= profit;
        ++moved;
      }
      if (moved == 0)
        break;
      finalizeCentroids();
      const double gain = roundStartSse > 0.0
                              ? (roundStartSse - bestOutput.sse) / roundStartSse
                              : 0.0;
      if (gain < kMinRoundGain || round == kMaxRelaxRounds)
        break;
      roundStartSse = bestOutput.sse;
    }
#ifdef ANALYSIS
    BC_LAP(watch, probe.tauConversion.transfer);
#endif
  }

  void finalizeCentroids() {
    auto &centroids = bestOutput.centroids;
    auto &clusters = bestOutput.clusters;
    auto &capacity = bestOutput.capacity;

    for (int clusterIdx = 0; clusterIdx < nCluster; ++clusterIdx) {
      fill(centroids[clusterIdx].begin(), centroids[clusterIdx].end(), 0.0);

      for (int elementIdx = 0; elementIdx < capacity[clusterIdx]; ++elementIdx) {
        int dataIdx = clusters[clusterIdx][elementIdx].second;
        int baseIdx = dataIdx * nDim;
        for (int dim = 0; dim < nDim; ++dim)
          centroids[clusterIdx][dim] += dataset[baseIdx + dim];
      }

      if (capacity[clusterIdx] > 0)
        for (int dim = 0; dim < nDim; ++dim)
          centroids[clusterIdx][dim] /= capacity[clusterIdx];
    }

    bestOutput.sse = computeSSE(bestOutput);
  }

  bool shouldAccept() {
    if (curOutput.iter == 1 || config.acceptance == Acceptance::RandomWalk)
      return true;

    if (config.acceptance == Acceptance::Better)
      return curOutput.sse <= prevOutput.sse;

    return false;
  }

  bool shouldTerminate(bool isAccepted) {
    if (curOutput.iter >= config.maxIter)
      return true;

    if (curOutput.iter == 1)
      return false;

    if (isAccepted) {
      bool isConverged = true;
      double tolerance = 1e-4;

      for (int c = 0; c < config.nCluster; ++c) {
        if (computeSquaredL2(prevOutput.centroids[c], curOutput.centroids[c]) >
            tolerance) {
          isConverged = false;
          break;
        }
      }

      if (isConverged)
        return true;
    }

    for (const auto &[type, param1, param2] : config.terminations) {
      if (type == TerminationType::CV) {
        if (sseHistory.size() >= static_cast<size_t>(param1)) {
          auto start = sseHistory.end() - static_cast<size_t>(param1);
          auto end = sseHistory.end();

          double mean =
              accumulate(start, end, 0.0) / static_cast<double>(param1);
          double sumSqDiff = 0.0;

          for (auto it = start; it != end; ++it) {
            sumSqDiff += (*it - mean) * (*it - mean);
          }

          double stdDev = sqrt(sumSqDiff / static_cast<double>(param1));

          if (mean < 1e-9) {
#ifdef ANALYSIS
          probe.termination = static_cast<int>(type);
#endif
            return true;
          }

          double cv = stdDev / mean;
          if (cv < param2) {
#ifdef ANALYSIS
          probe.termination = static_cast<int>(type);
#endif
            return true;
          }
        }
      } else if (type == TerminationType::W) {
        int diff = curOutput.iter - bestOutput.iter;
        if (diff > static_cast<int>(param1)) {
#ifdef ANALYSIS
          probe.termination = static_cast<int>(type);
#endif
          return true;
        }
      } else if (type == TerminationType::Delta) {
        if (prevOutput.sse < 1e-9) {
#ifdef ANALYSIS
          probe.termination = static_cast<int>(type);
#endif

          return true;
        }

        double delta = fabs(prevOutput.sse - curOutput.sse) / prevOutput.sse;
        if (delta < param1) {
#ifdef ANALYSIS
          probe.termination = static_cast<int>(type);
#endif
          return true;
        }
      }
    }

    return false;
  }

  Algorithm(Config _config, const vector<double> &_dataset, int _nData,
            int _nDim)
      : gen(_config.seed), config(_config), nDim(_nDim), nData(_nData),
        nCluster(_config.nCluster),
        dataset(_dataset.data()), capacityConstraint(nData, config.nCluster),
        distance(config.nCluster * nData),
        curOutput(nData, config.nCluster, nDim), clusterRadius(config.nCluster),
        nextClusterToTry(nData),
        preferredClusters(nData, vector<int>(config.nCluster)),
        prefValid(nData, 0), prefScratch(config.nCluster),
        centroidsT(static_cast<size_t>(_nDim) * _config.nCluster),
        firstProfits(capacityConstraint.maxCapacity),
        secondProfits(capacityConstraint.maxCapacity),
        overlappedClusterPairs(config.nCluster * (config.nCluster - 1) / 2),
        prevOutput(nData, config.nCluster, nDim),
        bestOutput(nData, config.nCluster, nDim) {
  }

  void run() {
    auto start = system_clock::now();

    while (true) {
      ++curOutput.iter;

      perturbation();
      localSearch();
#ifdef ANALYSIS
      BC_WATCH(otherWatch);
      probe.mseHistory.push_back(curOutput.sse / this->config.nCluster);
      probe.timeHistory.push_back(
          duration<double>(system_clock::now() - start).count());
#endif

      bool isAccepted = shouldAccept();

      if (curOutput.sse < bestOutput.sse) {
        bestOutput = curOutput;
      }

      if (shouldTerminate(isAccepted))
        break;

      if (isAccepted) {
        prevOutput = curOutput;
        sseHistory.push_back(curOutput.sse);
      } else {
        const int performed = curOutput.iter;
        curOutput = prevOutput;
        curOutput.iter = performed;
      }
#ifdef ANALYSIS
      BC_LAP(otherWatch, probe.acceptAndTerminate);
#endif
    }

    if ((config.enableTau || config.hardTau) &&
        !record::relaxationIsNoop(nData, config.nCluster, effectiveTau()))
      tauConversion();

    finalizeCentroids();

    auto end = system_clock::now();
    bestOutput.time = duration<double>(end - start).count();
  }

  const Config &getConfig() { return config; }
  const Output &getBestOutput() { return bestOutput; }
};

class AlgorithmWrapper {
private:
  const Config &config;

public:
  fs::path dataDir = "data";

  fs::path labelDir;

  vector<bench::Benchmark> datasets = bench::all();

  record::Writer raw;
  std::set<std::string> resumeKeys;

  AlgorithmWrapper(const Config &_config) : config(_config) {}

  void runSingleDataset(const fs::path &datasetPath) {
    string datasetName = datasetPath.filename();
    auto optDataset = DatasetLoader::load(datasetPath);

    if (!optDataset.has_value()) {
      cerr << "[ERROR] Failed to load dataset." << endl;
      return;
    }

    cout << "[INFO] Starting algorithm configuration..." << endl;
    cout << "[INFO] dataset: " << datasetName << endl;
    cout << config << endl;

    cout << "[RUN] Algorithm execution started." << endl;

    Algorithm algorithm(config, optDataset->flatData,
                        optDataset->nData, optDataset->nDim);
    algorithm.run();

    const auto &bestOutput = algorithm.getBestOutput();
    cout << "[DONE] Algorithm execution finished." << endl;
    cout << "[DONE] Elapsed Time: " << fixed << setprecision(8)
         << bestOutput.time << scientific
         << ", MSE: " << bestOutput.sse / optDataset->nData
         << ", Iteration: " << bestOutput.iter << endl;
  }

  struct DatasetFinalResult {
    string name;
    double timeMean, timeStd;
    double mseMean, mseStd;
    double iterMean, iterStd;
  };

  vector<DatasetFinalResult> runAllDatasets(int iterPerDataset,
                                            const fs::path summaryPath) {
    vector<DatasetFinalResult> finalResults;

    cout << "[INFO] Start to run the our algorithm with all benchmarks" << endl;
    for (const auto &benchmark : datasets) {
      const string &name = benchmark.name;
      const int nCluster = benchmark.k;
      fs::path datasetPath = dataDir / (name + ".txt");
      auto optDataset = DatasetLoader::load(datasetPath);

      if (!optDataset.has_value()) {
        cerr << "[ERROR] Skipping " << name << ": Load failed." << endl;
        continue;
      }
      if (auto problem = bench::verify(benchmark, optDataset->nData,
                                       optDataset->nDim);
          !problem.empty()) {
        cerr << "[ERROR] " << problem << endl;
        exit(1);
      }

      stats::Accumulator accTime, accMse, accIter;
      cout << "[RUN] Benchmarking Dataset: " << name << " (" << iterPerDataset
           << " runs)" << endl;

      for (int i = 0; i < iterPerDataset; ++i) {
        Config cfg = config;
        cfg.nCluster = nCluster;
        cfg.seed = config.seed + static_cast<unsigned int>(i);

        if (!resumeKeys.empty() &&
            resumeKeys.count(record::key(
                cfg.enableTau ? "BBC" : "HBC", benchmark.name, cfg.seed)) > 0)
          continue;

        Algorithm algorithm(cfg, optDataset->flatData, optDataset->nData,
                            optDataset->nDim);
        algorithm.run();

        const auto &best = algorithm.getBestOutput();
        const double mse = best.sse / optDataset->nData;
        accTime.push(best.time);
        accMse.push(mse);
        accIter.push(static_cast<double>(best.iter));

        if (raw.enabled())
          raw.write(makeRecord(benchmark, cfg, *optDataset, best, mse, i));

        if (!labelDir.empty()) {
          fs::create_directories(labelDir);
          vector<int> labels(optDataset->nData, 0);
          for (int c = 0; c < cfg.nCluster; ++c)
            for (int e = 0; e < best.capacity[c]; ++e)
              labels[best.clusters[c][e].second] = c;
          ofstream out(labelDir / (string(cfg.enableTau ? "BBC" : "HBC") + "_" +
                                   name + "_seed" + to_string(cfg.seed) + ".txt"));
          for (int label : labels)
            out << label << '\n';
        }
      }

      if (accTime.n() == 0) {
        cout << "[INFO] " << name << ": all seeds already recorded; "
             << "no summary row written" << endl;
        continue;
      }
      finalResults.push_back({name, accTime.mean(), accTime.sampleSd(),
                              accMse.mean(), accMse.sampleSd(), accIter.mean(),
                              accIter.sampleSd()});
    }

    writeSummary(finalResults, summaryPath);
    return finalResults;
  }

private:
  record::Run makeRecord(const bench::Benchmark &benchmark, const Config &cfg,
                         const LoadedData &data, const Output &best, double mse,
                         int run) const {
    record::Run r;
    r.algorithm = cfg.enableTau ? "BBC" : "HBC";
    r.dataset = benchmark.name;
    r.scale = bench::scaleName(benchmark.scale);
    r.n = data.nData;
    r.d = data.nDim;
    r.k = cfg.nCluster;
    r.seed = cfg.seed;
    r.run = run;
    r.mse = mse;
    r.sse = best.sse;
    r.timeSeconds = best.time;
    r.iters = best.iter;
    r.rho = cfg.enableTau ? cfg.tauRatio : 0.0;
    r.tau = (cfg.enableTau || cfg.hardTau)
                ? static_cast<double>(record::tauEff(r.n, r.k, r.rho))
                : 0.0;

    if (!best.capacity.empty()) {
      r.minClusterSize =
          *min_element(best.capacity.begin(), best.capacity.end());
      r.maxClusterSize =
          *max_element(best.capacity.begin(), best.capacity.end());
      r.feasible = record::withinBalance(r.minClusterSize, r.maxClusterSize,
                                         r.n, r.k, r.tau);
    }

    ostringstream cfgStr;
    cfgStr << "maxIter=" << cfg.maxIter
           << ",init=" << Config::InitNames[static_cast<int>(cfg.initMethod)]
           << ",ls=" << Config::LocalNames[static_cast<int>(cfg.localSearch)]
           << ",accept="
           << Config::AcceptNames[static_cast<int>(cfg.acceptance)]
           << ",geo=" << (cfg.enableGeo ? 1 : 0)
           << ",tau=" << (cfg.enableTau ? 1 : 0);
    if (cfg.hardTau && r.n % r.k != 0)
      cfgStr << ",hardtau=1";
    if (cfg.exactCentroids)
      cfgStr << ",exact=centroids";
    if (cfg.passCentroids)
      cfgStr << ",exact=passcentroids";
    if (cfg.rebuildPairs)
      cfgStr << ",exact=pairs";
    if (cfg.revalidateTau)
      cfgStr << ",exact=tau";
    for (const auto &t : cfg.terminations)
      cfgStr << ",term=" << Config::TermNames[static_cast<int>(t.type)] << '('
             << t.param1 << ',' << t.param2 << ')';
    r.config = cfgStr.str();
    return r;
  }

  void writeSummary(const vector<DatasetFinalResult> &finalResults,
                    const fs::path &summaryPath) const {
    ofstream summaryFile(summaryPath);

    const int wName = 25;
    const int wVal = 15;

    summaryFile << left << setw(wName) << "Dataset" << setw(wVal) << "Time_Mean"
                << setw(wVal) << "Time_Std" << setw(wVal) << "MSE_Mean"
                << setw(wVal) << "MSE_Std" << setw(wVal) << "Iter_Mean"
                << setw(wVal) << "Iter_Std" << endl;

    summaryFile << string(wName + wVal * 6, '-') << endl;

    for (const auto &res : finalResults) {
      summaryFile << left << setw(wName) << res.name << fixed << setprecision(6)
                  << setw(wVal) << res.timeMean << setw(wVal) << res.timeStd
                  << scientific << setprecision(4) << setw(wVal) << res.mseMean
                  << setw(wVal) << res.mseStd << fixed << setprecision(2)
                  << setw(wVal) << res.iterMean << setw(wVal) << res.iterStd
                  << endl;
    }

    summaryFile.close();
    cout << "[DONE] Sorted summary written to: " << summaryPath << endl;
  }

public:

#ifdef ANALYSIS
  void measure(int runsPerDataset, const fs::path saveDir) {
    fs::create_directories(saveDir);
    ofstream phases(saveDir / "phases.tsv");
    ofstream pairs(saveDir / "pairs.tsv");
    ofstream convergence(saveDir / "convergence.tsv");
    ofstream summary(saveDir / "runs.tsv");
    ofstream meta(saveDir / "meta.tsv");
    ofstream ranking(saveDir / "ranking.tsv");

    if (!phases || !pairs || !convergence || !summary || !meta || !ranking) {
      cerr << "[ERROR] Failed to open measurement files in " << saveDir << endl;
      return;
    }

    phases << "dataset\tn\td\tk\tseed\trun\tpart\tphase\tseconds\tcalls\n";
    pairs << "dataset\tn\td\tk\tseed\trun\tstage\tcount\n";
    convergence << "dataset\tn\td\tk\tseed\trun\titer\tmse\ttime_s\n";
    summary << "dataset\tn\td\tk\tseed\trun\tmse\ttime_seconds\titers"
               "\tmin_size\tmax_size\ttermination\n";

    ranking << "dataset\tn\td\tk\tseed\trun\tquantity\tvalue\n";

    meta << "key\tvalue\n";
    meta << "lap_cost_seconds\t" << setprecision(17) << probe::lapCostSeconds()
         << '\n';

    cout << "[INFO] Measuring over " << datasets.size() << " datasets, "
         << runsPerDataset << " run(s) each" << endl;

    for (const auto &benchmark : datasets) {
      const string &name = benchmark.name;
      auto optDataset = DatasetLoader::load(dataDir / (name + ".txt"));
      if (!optDataset) {
        cerr << "[ERROR] Skipping " << name << ": load failed." << endl;
        continue;
      }
      if (auto problem = bench::verify(benchmark, optDataset->nData,
                                       optDataset->nDim);
          !problem.empty()) {
        cerr << "[ERROR] " << problem << endl;
        exit(1);
      }
      const int n = optDataset->nData, d = optDataset->nDim, k = benchmark.k;
      cout << "[RUN] " << name << endl;

      for (int run = 0; run < runsPerDataset; ++run) {
        Config cfg = config;
        cfg.nCluster = k;
        cfg.seed = config.seed + static_cast<unsigned int>(run);

        Algorithm algorithm(cfg, optDataset->flatData, n, d);
        algorithm.run();

        const auto &best = algorithm.getBestOutput();
        const auto &pr = algorithm.probe;
        ostringstream key;
        key << name << '\t' << n << '\t' << d << '\t' << k << '\t' << cfg.seed
            << '\t' << run;

        for (const auto &[part, phase] : pr.phases())
          phases << key.str() << '\t' << part << '\t' << phase->name << '\t'
                 << setprecision(17) << phase->seconds << '\t' << phase->calls
                 << '\n';

        for (const auto &[quantity, value] :
             {pair<const char *, long long>{"profit_range_full",
                                            pr.ranking.profitRangeFull},
              {"profit_range_ordered", pr.ranking.profitRangeOrdered},
              {"profit_consumed", pr.ranking.profitConsumed},
              {"preference_offered", pr.ranking.preferenceOffered},
              {"preference_consumed", pr.ranking.preferenceConsumed},
              {"preference_max_depth", pr.ranking.preferenceMaxDepth}})
          ranking << key.str() << '\t' << quantity << '\t' << value << '\n';

        pairs << key.str() << "\ttotal\t" << pr.pruning.total << '\n'
              << key.str() << "\tafter_c1\t" << pr.pruning.afterC1 << '\n'
              << key.str() << "\tafter_c2\t" << pr.pruning.afterC2 << '\n';

        for (size_t i = 0; i < pr.mseHistory.size(); ++i)
          convergence << key.str() << '\t' << (i + 1) << '\t'
                      << setprecision(17) << pr.mseHistory[i] << '\t'
                      << pr.timeHistory[i] << '\n';

        int minSize = numeric_limits<int>::max(), maxSize = 0;
        for (int c : best.capacity) {
          minSize = min(minSize, c);
          maxSize = max(maxSize, c);
        }
        summary << key.str() << '\t' << setprecision(17) << best.sse / n << '\t'
                << best.time << '\t' << setprecision(6) << best.iter << '\t'
                << minSize << '\t' << maxSize << '\t'
                << Config::TermNames[pr.termination] << '\n';
      }
    }

    cout << "[DONE] runs.tsv, phases.tsv, pairs.tsv, convergence.tsv -> "
         << saveDir << endl;
  }
#endif
};
