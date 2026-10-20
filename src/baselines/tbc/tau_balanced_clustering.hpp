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
#ifndef TBC__TAU_BALANCED_CLUSTERING_H
#define TBC__TAU_BALANCED_CLUSTERING_H

#include <queue>
#include <string>
#include <vector>
#include <random>
#include <climits>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unordered_map>

namespace tbc {
    using pdi = std::pair<double, int>;

    struct ClusterCenter {
        int label;
        std::vector<double> value;
        std::priority_queue<pdi, std::vector<pdi>, std::less<pdi>> cluster;
    };
}

class TBC {
public:
    enum class Balance { Published, Repaired };

    TBC(const std::vector<std::vector<double>> &data, int k, int tau,
        unsigned int seed = std::random_device()(),
        Balance balance = Balance::Published, bool band = false,
        bool pairwise = false);
    ~TBC();
    double start();
    void initClusterCenters();
    void updateClusterCenters();
    void reassign();
    void assignToNearestCenter(int dataIndex, std::vector<double> &center);
    double distance(const std::vector<double> &data1, const std::vector<double> &data2) const;
    std::vector<int> getAssignments() const;

    int k_;
    size_t size_;
    size_t dims_;
    size_t bound_;
    std::vector<int> assignments_;
    std::vector<std::vector<double>> data_;
    std::vector<tbc::ClusterCenter> cluster_centers_;
    std::default_random_engine random_engine_;
    Balance balance_;
    size_t lower_;
    int tau_;
    bool pairwise_;
    std::vector<long> nearestCounts_;
    bool countNearest_ = false;

private:
    void chooseBand();
    void repairLowerBound();
    void rebuildClustersFromAssignments();
    void finalizeCenters();
};

#endif
