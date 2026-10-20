#pragma once
#include "Benchmarks.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace protocol {

struct Options {
  int runs = 10;
  unsigned int seedBase = 42;
  std::filesystem::path dataDir = "data";
  std::filesystem::path outDir;
  std::filesystem::path rawPath;
  std::vector<bench::Benchmark> datasets = bench::all();
  std::string selection = "all";
  std::map<std::string, std::vector<std::string>> extras;
  bool ok = true;
  bool exitEarly = false;
};

inline std::vector<std::string> extraValues(const Options &o,
                                            const std::string &flag) {
  auto it = o.extras.find(flag);
  return it == o.extras.end() ? std::vector<std::string>{} : it->second;
}

inline bool hasFlag(const Options &o, const std::string &flag) {
  return o.extras.find(flag) != o.extras.end();
}

inline unsigned int seedFor(const Options &o, int run) {
  return o.seedBase + static_cast<unsigned int>(run);
}

inline std::string formatMultiplier(double scale) {
  std::ostringstream ss;
  ss << scale;
  return ss.str();
}

inline std::string seedList(const Options &o) {
  std::ostringstream ss;
  for (int i = 0; i < o.runs; ++i)
    ss << (i ? "," : "") << seedFor(o, i);
  return ss.str();
}

inline void usage(const char *prog, const char *extra = nullptr) {
  std::cerr
      << "usage: " << prog << " [options]\n"
      << "  --runs N            independent runs per dataset (default 10)\n"
      << "  --seed-base S       seed of run 0; run i uses S+i (default 42)\n"
      << "  --data DIR          dataset directory (default data)\n"
      << "  --out DIR|FILE      summary destination\n"
      << "  --raw FILE          per-run records (default <out>/raw_runs.tsv)\n"
      << "  --scale S           standard | large | all (default all)\n"
      << "  --dataset NAME      run only NAME; repeatable\n"
      << "  --list              print the benchmark registry and exit\n"
      << "  --resume            skip (dataset, seed) pairs already present in\n"
      << "                      the raw file, so a killed sweep can continue\n";
  if (extra)
    std::cerr << extra;
}

inline Options parse(int argc, char **argv, const char *extraHelp = nullptr,
                     const std::vector<std::string> &extraFlags = {}) {
  Options o;
  std::vector<bench::Benchmark> picked;

  auto need = [&](int &i, const char *flag) -> const char * {
    if (i + 1 >= argc) {
      std::cerr << "[ERROR] " << flag << " requires a value\n";
      o.ok = false;
      return nullptr;
    }
    return argv[++i];
  };

  for (int i = 1; i < argc && o.ok; ++i) {
    const std::string a = argv[i];
    if (a == "--runs") {
      if (const char *v = need(i, "--runs"))
        o.runs = std::atoi(v);
    } else if (a == "--seed-base") {
      if (const char *v = need(i, "--seed-base"))
        o.seedBase = static_cast<unsigned int>(std::strtoul(v, nullptr, 10));
    } else if (a == "--data") {
      if (const char *v = need(i, "--data"))
        o.dataDir = v;
    } else if (a == "--out") {
      if (const char *v = need(i, "--out"))
        o.outDir = v;
    } else if (a == "--raw") {
      if (const char *v = need(i, "--raw"))
        o.rawPath = v;
    } else if (a == "--scale") {
      const char *v = need(i, "--scale");
      if (!v)
        break;
      o.selection = v;
      if (o.selection == "standard")
        o.datasets = bench::byScale(bench::Scale::Standard);
      else if (o.selection == "large")
        o.datasets = bench::byScale(bench::Scale::Large);
      else if (o.selection == "all")
        o.datasets = bench::all();
      else {
        std::cerr << "[ERROR] --scale must be standard, large or all\n";
        o.ok = false;
      }
    } else if (a == "--dataset") {
      const char *v = need(i, "--dataset");
      if (!v)
        break;
      auto b = bench::byName(v);
      if (!b) {
        std::cerr << "[ERROR] unknown dataset: " << v
                  << " (--list shows the registry)\n";
        o.ok = false;
      } else
        picked.push_back(*b);
    } else if (a == "--list") {
      for (const auto &b : bench::all())
        std::cout << b.name << '\t' << b.k << '\t' << bench::scaleName(b.scale)
                  << '\n';
      for (const auto &r : bench::retired())
        std::cout << r.name << "\t-\tretired: " << r.reason << '\n';
      o.exitEarly = true;
      return o;
    } else if (a == "-h" || a == "--help") {
      usage(argv[0], extraHelp);
      o.exitEarly = true;
      return o;
    } else if (a == "--resume") {
      o.extras["--resume"];
    } else if (std::find(extraFlags.begin(), extraFlags.end(), a) !=
               extraFlags.end()) {
      if (const char *v = need(i, a.c_str()))
        o.extras[a].push_back(v);
    } else {
      std::cerr << "[ERROR] unknown option: " << a << "\n";
      usage(argv[0], extraHelp);
      o.ok = false;
    }
  }

  if (!picked.empty()) {
    o.datasets = picked;
    o.selection = "explicit";
  }
  if (o.runs < 1) {
    std::cerr << "[ERROR] --runs must be >= 1\n";
    o.ok = false;
  }
  return o;
}

inline std::filesystem::path rawPathOr(const Options &o,
                                       const std::filesystem::path &fallback) {
  return o.rawPath.empty() ? fallback : o.rawPath;
}

inline void report(const Options &o, const std::string &algorithm) {
  std::cout << "[INFO] algorithm : " << algorithm << "\n"
            << "[INFO] datasets  : " << o.datasets.size() << " (" << o.selection
            << ")\n"
            << "[INFO] runs      : " << o.runs << "\n"
            << "[INFO] seeds     : " << seedList(o) << "\n"
            << "[INFO] data      : " << o.dataDir << std::endl;
}

}
