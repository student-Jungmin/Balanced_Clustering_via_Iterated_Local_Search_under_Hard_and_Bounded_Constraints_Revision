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
#include "tau_balanced_clustering.hpp"
#include <algorithm>
#include <limits>
#include <cmath>

static size_t capacityBound(size_t n, int k, int tau, TBC::Balance balance) {
    if (balance == TBC::Balance::Published)
        return (n - tau) / k + tau;
    const size_t ceilAverage = (n + k - 1) / k;
    return std::max(ceilAverage, (n + tau) / k);
}

TBC::TBC(const std::vector<std::vector<double>> &data, int k, int tau, unsigned int seed,
         Balance balance, bool band, bool pairwise)
        : data_(data),
          k_(k),
          size_(data.size()),
          dims_(data.front().size()),
          bound_(capacityBound(data.size(), k, tau, balance)),
          random_engine_(seed),
          balance_(balance),
          lower_(0),
          tau_(tau),
          pairwise_(balance == Balance::Repaired && !band && pairwise && tau >= 2) {
    if (balance_ == Balance::Repaired && band) {
        const size_t n = size_, kk = static_cast<size_t>(k_);
        const size_t t = static_cast<size_t>(tau);
        bound_ = (n + kk - 1) / kk + t;
        lower_ = n / kk > t ? n / kk - t : 0;
    } else if (balance_ == Balance::Repaired) {
        lower_ = bound_ > static_cast<size_t>(tau) ? bound_ - tau : 0;
        lower_ = std::min(lower_, size_ / static_cast<size_t>(k_));
    }
}

void TBC::repairLowerBound() {
    std::vector<int> counts(k_, 0);
    for (int label : assignments_) counts[label]++;

    std::vector<std::pair<double, int>> order;
    order.reserve(size_);
    for (int target = 0; target < k_; ++target) {
        if (static_cast<size_t>(counts[target]) >= lower_) continue;
        order.clear();
        for (int p = 0; p < static_cast<int>(size_); ++p) {
            const int donor = assignments_[p];
            if (donor == target) continue;
            order.emplace_back(distance(cluster_centers_[target].value, data_[p])
                             - distance(cluster_centers_[donor].value, data_[p]), p);
        }
        std::stable_sort(order.begin(), order.end(),
                         [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &[cost, p] : order) {
            if (static_cast<size_t>(counts[target]) >= lower_) break;
            const int donor = assignments_[p];
            if (static_cast<size_t>(counts[donor]) <= lower_) continue;
            counts[donor]--;
            counts[target]++;
            assignments_[p] = target;
        }
    }
}

void TBC::chooseBand() {
    const long n = static_cast<long>(size_), kk = static_cast<long>(k_), t = tau_;
    const long lo = std::max(0L, (n + kk - 1) / kk - t), hi = n / kk;
    long bestL = hi, bestCost = std::numeric_limits<long>::max();
    double bestOff = std::numeric_limits<double>::max();
    for (long L = lo; L <= hi; ++L) {
        long cost = 0;
        for (long c : nearestCounts_) cost += std::max(0L, c - (L + t)) + std::max(0L, L - c);
        const double off = std::abs(L + t / 2.0 - static_cast<double>(n) / kk);
        if (cost < bestCost || (cost == bestCost && off < bestOff)) {
            bestL = L; bestCost = cost; bestOff = off;
        }
    }
    lower_ = static_cast<size_t>(bestL);
    bound_ = static_cast<size_t>(bestL + t);
}

TBC::~TBC() = default;

double TBC::start() {
    initClusterCenters();
    for (int i = 1;; ++ i) {
        std::vector<int> oldAssignment(assignments_);
        if (pairwise_) chooseBand();
        reassign();
        if (balance_ == Balance::Repaired) {
            repairLowerBound();
            rebuildClustersFromAssignments();
        }
        if (oldAssignment == assignments_ || i > 20) {
            break;
        }
        updateClusterCenters();
    }
    if (balance_ == Balance::Repaired) finalizeCenters();
    return 0;
}

void TBC::rebuildClustersFromAssignments() {
    for (auto &center : cluster_centers_)
        center.cluster = std::priority_queue<tbc::pdi, std::vector<tbc::pdi>, std::less<tbc::pdi>>();

    for (int p = 0; p < static_cast<int>(size_); ++p) {
        const int label = assignments_[p];
        cluster_centers_[label].cluster.push(
            std::make_pair(distance(cluster_centers_[label].value, data_[p]), p));
    }
}

void TBC::finalizeCenters() {
    std::vector<int> counts(k_, 0);
    for (auto &center : cluster_centers_)
        std::fill(center.value.begin(), center.value.end(), 0.0);

    for (int p = 0; p < static_cast<int>(size_); ++p) {
        const int label = assignments_[p];
        counts[label]++;
        for (size_t d = 0; d < dims_; ++d)
            cluster_centers_[label].value[d] += data_[p][d];
    }
    for (int c = 0; c < k_; ++c)
        if (counts[c] > 0)
            for (size_t d = 0; d < dims_; ++d)
                cluster_centers_[c].value[d] /= static_cast<double>(counts[c]);
}

void TBC::initClusterCenters() {
    std::vector<size_t> indices(size_);
    for (size_t i = 0; i < size_; ++i) {
        indices[i] = i;
    }
    std::shuffle(indices.begin(), indices.end(), random_engine_);

    cluster_centers_.resize(k_);
    for (int i = 0; i < k_; ++i) {
        cluster_centers_[i].label = i;
        auto ith = data_[indices[i]];
        cluster_centers_[i].value.resize(dims_);
        std::copy(ith.begin(), ith.end(), cluster_centers_[i].value.begin());
    }
}

void TBC::reassign() {
    assignments_.assign(size_, 0);
    if (pairwise_) nearestCounts_.assign(k_, 0);
    for (int i = 0; i < size_; ++i) {
        countNearest_ = pairwise_;
        assignToNearestCenter(i, data_[i]);
    }
}

void TBC::updateClusterCenters() {
    for (auto& cluster_center: cluster_centers_) {
        size_t clusterSize = cluster_center.cluster.size();
        std::vector<double> newClusterValue(dims_);
        while (!cluster_center.cluster.empty()) {
            tbc::pdi distIdxPair = cluster_center.cluster.top();
            std::vector<double> dataValue = data_[distIdxPair.second];
            for (int i = 0; i < dims_; ++i) {
                newClusterValue[i] += dataValue[i];
            }
            cluster_center.cluster.pop();
        }
        if (clusterSize > 0) {
            for (int i = 0; i < dims_; ++i) {
                cluster_center.value[i] = newClusterValue[i] / double(clusterSize);
            }
        }
    }
}

void TBC::assignToNearestCenter(int dataIndex, std::vector<double> &dataItem) {
    std::priority_queue<tbc::pdi, std::vector<tbc::pdi>, std::greater<tbc::pdi>> nearestClusters;

    for (int i = 0; i < cluster_centers_.size(); ++ i) {
        double dist = distance(cluster_centers_[i].value, dataItem);
        nearestClusters.push( {dist, i} );
    }
    if (countNearest_) {
        nearestCounts_[nearestClusters.top().second]++;
        countNearest_ = false;
    }
    while (!nearestClusters.empty()) {
        double dist = nearestClusters.top().first;
        int nearestIdx = nearestClusters.top().second;
        tbc::ClusterCenter &nearest = cluster_centers_[nearestIdx];
        const size_t capacity = (balance_ == Balance::Published) ? bound_ + 1 : bound_;
        if (nearest.cluster.size() < capacity) {
            nearest.cluster.push(std::make_pair(dist, dataIndex));
            assignments_[dataIndex] = nearest.label;
            return;
        } else {

            tbc::pdi boundary = nearest.cluster.top();

            if (boundary.first > dist) {
                nearest.cluster.pop();
                nearest.cluster.push(std::make_pair(dist, dataIndex));
                assignments_[dataIndex] = nearest.label;
                assignToNearestCenter(boundary.second, data_[boundary.second]);
                return;
            }
        }
        nearestClusters.pop();
    }
}

double TBC::distance(const std::vector<double> &data1, const std::vector<double> &data2) const {
    double result = 0.0;
    for (int i = 0; i < dims_; ++i) {
        result += (data1[i] - data2[i]) * (data1[i] - data2[i]);
    }
    return result;
}

std::vector<int> TBC::getAssignments() const {
    return assignments_;
}
