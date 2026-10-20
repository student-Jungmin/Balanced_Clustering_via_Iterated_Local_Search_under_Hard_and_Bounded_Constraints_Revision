#include "Algorithm.hpp"
#include "RunProtocol.hpp"
#include <iostream>
#include <string>

namespace {

bool parseRanking(const std::string &v, Ranking &out) {
  if (v == "prefix")
    out = Ranking::Prefix;
  else if (v == "full")
    out = Ranking::Full;
  else
    return false;
  return true;
}

bool parseLocalSearch(const std::string &v, LocalSearch &out) {
  if (v == "once")
    out = LocalSearch::Once;
  else if (v == "sync")
    out = LocalSearch::Sync;
  else if (v == "adaptive")
    out = LocalSearch::Adaptive;
  else if (v == "syncearly")
    out = LocalSearch::SyncEarly;
  else
    return false;
  return true;
}

bool parseInit(const std::string &v, InitMethod &out) {
  if (v == "random")
    out = InitMethod::Random;
  else if (v == "kmeanspp")
    out = InitMethod::KMeansPP;
  else
    return false;
  return true;
}

bool parseAccept(const std::string &v, Acceptance &out) {
  if (v == "better")
    out = Acceptance::Better;
  else if (v == "randomwalk")
    out = Acceptance::RandomWalk;
  else
    return false;
  return true;
}

bool parseBool(const std::string &v, bool &out) {
  if (v == "on" || v == "1" || v == "true")
    out = true;
  else if (v == "off" || v == "0" || v == "false")
    out = false;
  else
    return false;
  return true;
}

const char *kExtraHelp =
    "  --local-search M    once | sync | adaptive | syncearly (default sync)\n"
      "  --ranking R         prefix | full (default prefix)\n"
    "  --geo on|off        geometric pruning (default on)\n"
    "  --rho R             bounded balance; tau = floor(R*N/K). 0 = hard balance\n"
    "  --max-iter N        maximum ILS iterations (default 100)\n"
    "  --cv-window W       CV termination window, omega (default 3)\n"
    "  --cv-threshold D    CV termination threshold, delta (default 0.001)\n"
    "  --labels DIR        write each run's assignment there, one cluster\n"
    "                      index per line, for bc-tau (R1-4b)\n"
    "  --exact M           exactness variant for the E6 ablation; repeatable:\n"
    "                      centroids (R3-3, per pair) | passcentroids (R3-3,\n"
    "                      per pass) | pairs (R3-4) | tau (R3-5)\n"
    "  --init M            random | kmeanspp (default kmeanspp)\n"
    "  --accept M          better | randomwalk (default randomwalk)\n"
    "  --hard-tau on       hard balance with the relaxation at tau = 1 where\n"
    "                      K does not divide N (0 where it does); recorded as HBC\n"
    "  --stall N           also stop after N iterations with no improvement\n"
    "                      on the best solution (default off)\n";

}

int main(int argc, char **argv) {
  auto opt = protocol::parse(
      argc, argv, kExtraHelp,
      {"--local-search", "--ranking", "--geo", "--rho", "--max-iter", "--init", "--accept",
         "--cv-window", "--cv-threshold", "--exact", "--labels", "--stall",
         "--hard-tau"});
  if (opt.exitEarly)
    return 0;
  if (!opt.ok)
    return 2;

  Config config;
  auto last = [&](const char *flag) -> std::string {
    auto v = protocol::extraValues(opt, flag);
    return v.empty() ? std::string{} : v.back();
  };

  if (auto v = last("--local-search"); !v.empty())
    if (!parseLocalSearch(v, config.localSearch)) {
      std::cerr << "[ERROR] --local-search must be once, sync, adaptive or syncearly\n";
      return 2;
    }
  if (auto v = last("--ranking"); !v.empty())
    if (!parseRanking(v, config.ranking)) {
      std::cerr << "[ERROR] --ranking must be prefix or full\n";
      return 2;
    }
  if (auto v = last("--init"); !v.empty())
    if (!parseInit(v, config.initMethod)) {
      std::cerr << "[ERROR] --init must be random or kmeanspp\n";
      return 2;
    }
  if (auto v = last("--accept"); !v.empty())
    if (!parseAccept(v, config.acceptance)) {
      std::cerr << "[ERROR] --accept must be better or randomwalk\n";
      return 2;
    }
  if (auto v = last("--geo"); !v.empty())
    if (!parseBool(v, config.enableGeo)) {
      std::cerr << "[ERROR] --geo must be on or off\n";
      return 2;
    }
  {
    double window = 3, threshold = 0.001;
    for (const auto &t : config.terminations)
      if (t.type == TerminationType::CV) {
        window = t.param1;
        threshold = t.param2;
      }
    bool set = false;
    if (auto v = last("--cv-window"); !v.empty()) {
      window = stod(v);
      set = true;
    }
    if (auto v = last("--cv-threshold"); !v.empty()) {
      threshold = stod(v);
      set = true;
    }
    if (set)
      config.terminations = {{TerminationType::CV, window, threshold}};
  }

  if (auto v = last("--stall"); !v.empty()) {
    const int window = stoi(v);
    if (window < 1) {
      std::cerr << "[ERROR] --stall must be at least 1\n";
      return 2;
    }
    config.terminations.push_back(
        {TerminationType::W, static_cast<double>(window), 0.0});
  }

  for (const auto &v : protocol::extraValues(opt, "--exact")) {
    if (v == "centroids")
      config.exactCentroids = true;
    else if (v == "passcentroids")
      config.passCentroids = true;
    else if (v == "pairs")
      config.rebuildPairs = true;
    else if (v == "tau")
      config.revalidateTau = true;
    else {
      std::cerr << "[ERROR] --exact must be centroids, passcentroids, pairs "
                   "or tau\n";
      return 2;
    }
  }

  if (auto v = last("--max-iter"); !v.empty())
    config.maxIter = std::stoi(v);
  if (auto v = last("--rho"); !v.empty()) {
    config.tauRatio = std::stod(v);
    config.enableTau = config.tauRatio > 0.0;
  }
  if (auto v = last("--hard-tau"); v == "on")
    config.hardTau = true;
  config.seed = opt.seedBase;

  if (!fs::exists(opt.dataDir)) {
    std::cerr << "[ERROR] Dataset directory not found: " << opt.dataDir << "\n"
              << "[HINT ] Run from the repository root, or pass --data DIR."
              << std::endl;
    return 1;
  }

  AlgorithmWrapper wrapper(config);
  wrapper.dataDir = opt.dataDir;
  if (auto dirs = protocol::extraValues(opt, "--labels"); !dirs.empty())
    wrapper.labelDir = dirs.back();
  wrapper.datasets = opt.datasets;

  protocol::report(opt, config.enableTau ? "BBC" : "HBC");
  std::cout << config << std::endl;

#ifdef ANALYSIS
  fs::path outPath = opt.outDir.empty() ? fs::path("results/runtime/analysis")
                                        : opt.outDir;
  fs::create_directories(outPath);
  wrapper.raw.open(protocol::rawPathOr(opt, outPath / "raw_runs.tsv"));
  wrapper.measure(opt.runs, outPath);
#else
  fs::path outPath = opt.outDir.empty()
                         ? fs::path("results/benchmark/ours_summary.txt")
                         : opt.outDir;
  if (fs::is_directory(outPath) || !outPath.has_extension()) {
    fs::create_directories(outPath);
    outPath /= "summary_all_datasets.txt";
  } else if (outPath.has_parent_path()) {
    fs::create_directories(outPath.parent_path());
  }
  {
    const auto rawPath =
        protocol::rawPathOr(opt, outPath.parent_path() / "raw_runs.tsv");
    if (protocol::hasFlag(opt, "--resume")) {
      wrapper.resumeKeys = record::existingKeys(rawPath);
      if (!wrapper.resumeKeys.empty())
        std::cout << "[INFO] resume    : " << wrapper.resumeKeys.size()
                  << " run(s) already recorded in " << rawPath << std::endl;
    }
    wrapper.raw.open(rawPath);
  }
  wrapper.runAllDatasets(opt.runs, outPath);
#endif
  return 0;
}
