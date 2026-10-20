#pragma once
#include "Benchmarks.hpp"
#include "io.hpp"
#include "RunProtocol.hpp"
#include "RunRecord.hpp"
#include "Stats.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

inline const int max_iters = 100;
inline const int bkmpp_max_iters = 100000;

inline const string ext = ".txt";
inline const string default_data_root = "data";
inline const string default_stat_root = "results/benchmark/baselines";

inline const vector<string> stat_headers = {"name", "mse(avg)", "mse(sd)",
                                            "time(avg)", "time(sd)"};

class BaselineHarness {
public:
  BaselineHarness(const protocol::Options &opt, const string &algorithm)
      : opt_(opt), algorithm_(algorithm) {
    namespace fs = std::filesystem;

    fs::path outDir = opt_.outDir.empty() ? fs::path(default_stat_root)
                                          : opt_.outDir;
    fs::create_directories(outDir);

    const fs::path summaryPath = outDir / (algorithm + ext);
    summary_.open(summaryPath);
    if (!summary_.is_open()) {
      cerr << "[ERROR] Cannot open " << summaryPath << endl;
      ok_ = false;
      return;
    }
    for (const auto &h : stat_headers)
      summary_ << h << "\t";
    summary_ << endl;

    const auto rawPath = protocol::rawPathOr(opt_, outDir / "raw_runs.tsv");
    if (!protocol::extraValues(opt_, "--resume").empty() ||
        protocol::hasFlag(opt_, "--resume")) {
      done_ = record::existingKeys(rawPath);
      if (!done_.empty())
        cout << "[INFO] resume    : " << done_.size()
             << " run(s) already recorded in " << rawPath << endl;
    }
    raw_.open(rawPath);
    summaryPath_ = summaryPath;

    if (auto dirs = protocol::extraValues(opt_, "--labels"); !dirs.empty())
      labelDir_ = dirs.back();
  }

  bool ok() const { return ok_; }
  const protocol::Options &options() const { return opt_; }

  bool alreadyDone(const string &dataset, unsigned int seed) const {
    return !done_.empty() &&
           done_.count(record::key(algorithm_, dataset, seed)) > 0;
  }

  string datasetPath(const string &name) const {
    const std::filesystem::path root =
        opt_.dataDir.empty() ? std::filesystem::path(default_data_root)
                             : opt_.dataDir;
    return (root / (name + ext)).string();
  }

  void requireShape(const bench::Benchmark &b, int n, int d) const {
    if (auto problem = bench::verify(b, n, d); !problem.empty()) {
      cerr << "[ERROR] " << problem << endl;
      exit(1);
    }
  }

  void record(record::Run rec, const vector<int> &labels) {
    record::setBalance(rec, labels);
    if (!labelDir_.empty()) {
      std::filesystem::create_directories(labelDir_);
      const std::string file = algorithm_ + "_" + rec.dataset + "_seed" +
                               std::to_string(rec.seed) + ".txt";
      write_vector(labels, (labelDir_ / file).string());
    }
    record(std::move(rec));
  }

  void record(record::Run rec) {
    rec.algorithm = algorithm_;
    raw_.write(rec);
    mse_.push(rec.mse);
    time_.push(rec.timeSeconds);

    cout << algorithm_ << "/" << rec.dataset << "/seed " << rec.seed
         << " -> mse " << rec.mse << ", time " << rec.timeSeconds << "s"
         << endl;
  }

  void finishDataset(const string &name) {
    summary_ << name << "\t" << mse_.mean() << "\t" << mse_.sampleSd() << "\t"
             << time_.mean() << "\t" << time_.sampleSd() << endl;
    mse_ = stats::Accumulator{};
    time_ = stats::Accumulator{};
  }

  ~BaselineHarness() {
    if (summary_.is_open()) {
      summary_.close();
      cout << "[DONE] summary -> " << summaryPath_ << endl;
    }
  }

private:
  protocol::Options opt_;
  string algorithm_;
  ofstream summary_;
  std::filesystem::path summaryPath_;
  std::filesystem::path labelDir_;
  record::Writer raw_;
  std::set<std::string> done_;
  stats::Accumulator mse_;
  stats::Accumulator time_;
  bool ok_ = true;
};
