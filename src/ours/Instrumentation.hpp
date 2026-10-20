#pragma once

#ifdef ANALYSIS
#include <chrono>
#include <string>
#include <vector>

namespace probe {

struct Phase {
  const char *name;
  double seconds = 0.0;
  long long calls = 0;
};

class Stopwatch {
public:
  Stopwatch() : mark_(clock::now()) {}

  void lap(Phase &phase) {
    const auto now = clock::now();
    phase.seconds += std::chrono::duration<double>(now - mark_).count();
    phase.calls++;
    mark_ = now;
  }

  void skip() { mark_ = clock::now(); }

private:
  using clock = std::chrono::system_clock;
  clock::time_point mark_;
};

inline double lapCostSeconds() {
  static const double cost = [] {
    Phase scratch{"calibration"};
    Stopwatch watch;
    constexpr long long kReps = 2'000'000;
    const auto begin = std::chrono::system_clock::now();
    for (long long i = 0; i < kReps; ++i)
      watch.lap(scratch);
    const auto end = std::chrono::system_clock::now();
    return std::chrono::duration<double>(end - begin).count() / kReps;
  }();
  return cost;
}

class Scope {
public:
  explicit Scope(Phase &phase) : phase_(phase), start_(clock::now()) {}
  ~Scope() {
    phase_.seconds +=
        std::chrono::duration<double>(clock::now() - start_).count();
    phase_.calls++;
  }
  Scope(const Scope &) = delete;
  Scope &operator=(const Scope &) = delete;

private:
  using clock = std::chrono::system_clock;
  Phase &phase_;
  clock::time_point start_;
};

struct Perturbation {
  Phase updateCentroids{"UpdateCentroids"};
  Phase updateDistMatrix{"UpdateDistMatrix"};
  Phase rankPreferences{"RankPreferences"};
  Phase assign{"Assign"};
  Phase updateClusterRadius{"UpdateClusterRadius"};
};

struct LocalSearch {
  Phase getClusterPairs{"GetClusterPairs"};
  Phase getProfits{"GetProfits"};
  Phase rankProfits{"RankProfits"};
  Phase findSwapCount{"FindSwapCount"};
  Phase swap{"Swap"};
  Phase updateClusterRadius{"UpdateClusterRadius"};
};

struct TauConversion {
  Phase readyForTau{"ReadyForTau"};
  Phase updateDistMatrix{"UpdateDistMatrix"};
  Phase rankPreferences{"RankPreferences"};
  Phase transfer{"Transfer"};
};

struct Pruning {
  long long total = 0;
  long long afterC1 = 0;
  long long afterC2 = 0;
};

struct Ranking {
  long long profitRangeFull = 0;
  long long profitRangeOrdered = 0;
  long long profitConsumed = 0;

  long long preferenceOffered = 0;
  long long preferenceConsumed = 0;
  long long preferenceMaxDepth = 0;
};

struct Probe {
  Perturbation perturbation;
  LocalSearch localSearch;
  TauConversion tauConversion;
  Phase acceptAndTerminate{"AcceptAndTerminate"};

  Pruning pruning;
  Ranking ranking;
  std::vector<double> mseHistory;
  std::vector<double> timeHistory;
  int termination = 0;

  std::vector<std::pair<const char *, const Phase *>> phases() const {
    return {
        {"perturbation", &perturbation.updateCentroids},
        {"perturbation", &perturbation.updateDistMatrix},
        {"perturbation", &perturbation.rankPreferences},
        {"perturbation", &perturbation.assign},
        {"perturbation", &perturbation.updateClusterRadius},
        {"local_search", &localSearch.getClusterPairs},
        {"local_search", &localSearch.getProfits},
        {"local_search", &localSearch.rankProfits},
        {"local_search", &localSearch.findSwapCount},
        {"local_search", &localSearch.swap},
        {"local_search", &localSearch.updateClusterRadius},
        {"tau_conversion", &tauConversion.readyForTau},
        {"tau_conversion", &tauConversion.updateDistMatrix},
        {"tau_conversion", &tauConversion.rankPreferences},
        {"tau_conversion", &tauConversion.transfer},
        {"other", &acceptAndTerminate},
    };
  }
};

}

#define BC_LAP(watch, phase) (watch).lap(phase)
#define BC_SCOPE(name, phase) probe::Scope name(phase)
#define BC_WATCH(name) probe::Stopwatch name
#define BC_SKIP(watch) (watch).skip()
#define BC_COUNT(expr) (expr)
#else
#define BC_LAP(watch, phase) ((void)0)
#define BC_SCOPE(name, phase) ((void)0)
#define BC_WATCH(name) ((void)0)
#define BC_SKIP(watch) ((void)0)
#define BC_COUNT(expr) ((void)0)
#endif
