#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <vector>

#include "SummaryReader.hpp"
#include "io.hpp"
#include "metric.hpp"
#include "stat.hpp"

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

namespace {

bool writeCsv(const vector<vector<double>> &dataset, const fs::path &out) {
  ofstream f(out);
  if (!f)
    return false;
  f << setprecision(17);
  for (const auto &row : dataset) {
    for (size_t j = 0; j < row.size(); ++j)
      f << (j ? "," : "") << row[j];
    f << '\n';
  }
  return static_cast<bool>(f);
}

string findJava(const protocol::Options &opt) {
  if (auto v = protocol::extraValues(opt, "--java"); !v.empty())
    return v.back();
  if (const char *home = getenv("JAVA_HOME"))
    return string(home) + "/bin/java";
  if (const char *home = getenv("HOME")) {
    const string candidate = string(home) + "/opt/jdk17/bin/java";
    if (fs::exists(candidate))
      return candidate;
  }
  return "java";
}

string shellQuote(const string &s) {
  string out = "'";
  for (char c : s)
    out += (c == '\'') ? string("'\\''") : string(1, c);
  return out + "'";
}

}

int main(int argc, char **argv) {
  const char *extraHelp =
      "  --java PATH         java executable (default $JAVA_HOME/bin/java,\n"
      "                      then ~/opt/jdk17/bin/java, then java)\n"
      "  --jar PATH          harness jar (default bin/sobmssc.jar)\n"
      "  --budget FILE       summary written by ours, supplying the per-dataset\n"
      "                      time budget. Without it the published\n"
      "                      100-iteration configuration runs as-is.\n"
      "  --scale-factor X    budget multiplier, fractional allowed; repeatable\n"
      "                      (only meaningful with --budget)\n"
      "  --labels DIR        write each run's assignment there, one cluster\n"
      "                      index per line, for bc-tau (R1-4b)\n"
      "  --heap-mb N         JVM maximum heap in MiB (default 2048). Without a\n"
      "                      bound the JVM sizes the heap from the machine's\n"
      "                      RAM, so the footprint would not be reproducible.\n";
  auto opt = protocol::parse(argc, argv, extraHelp,
                             {"--java", "--jar", "--budget", "--scale-factor",
                              "--labels", "--heap-mb"});
  if (opt.exitEarly)
    return 0;
  if (!opt.ok)
    return 2;

  const string java = findJava(opt);
  auto heapArgs = protocol::extraValues(opt, "--heap-mb");
  const int heapMb = heapArgs.empty() ? 2048 : stoi(heapArgs.back());
  auto jarArgs = protocol::extraValues(opt, "--jar");
  const fs::path jar = jarArgs.empty() ? fs::path("bin/sobmssc.jar")
                                       : fs::path(jarArgs.back());
  if (!fs::exists(jar)) {
    cerr << "[ERROR] jar not found: " << jar << "\n"
         << "[HINT ] Run `make sobmssc` (needs JDK 17; see README)."
         << endl;
    return 1;
  }

  map<string, double> budgets;
  auto budgetArgs = protocol::extraValues(opt, "--budget");
  if (!budgetArgs.empty()) {
    budgets = summary::timeMeans(budgetArgs.back());
    if (budgets.empty()) {
      cerr << "[ERROR] No time budgets read from " << budgetArgs.back() << endl;
      return 1;
    }
    cout << "[INFO] budgets   : " << budgets.size() << " datasets from "
         << budgetArgs.back() << endl;
  }
  vector<double> scales;
  for (const auto &s : protocol::extraValues(opt, "--scale-factor"))
    scales.push_back(stod(s));
  if (scales.empty())
    scales = {1};
  if (budgets.empty())
    scales = {1};

  const fs::path scratch =
      fs::temp_directory_path() / ("sobmssc-" + to_string(getpid()));
  fs::create_directories(scratch);

  int status = 0;
  for (double scale : scales) {
    string algo_name = "SOBMSSC";
    if (!budgets.empty())
      algo_name += "x" + protocol::formatMultiplier(scale);
    protocol::report(opt, algo_name);
    BaselineHarness harness(opt, algo_name);
    if (!harness.ok()) {
      status = 1;
      break;
    }

    for (const auto &b : opt.datasets) {
      if (b.scale == bench::Scale::Large) {
        cerr << "[SKIP ] " << b.name
             << ": SO-BMSSC builds a full n x n distance matrix and is "
                "confined to the standard-scale datasets." << endl;
        continue;
      }

      auto dataset = load_matrix(harness.datasetPath(b.name));
      if (dataset.empty()) {
        cerr << "[ERROR] Skipping " << b.name << ": load failed." << endl;
        continue;
      }
      const int n_data = static_cast<int>(dataset.size());
      const int dim = static_cast<int>(dataset[0].size());
      harness.requireShape(b, n_data, dim);

      const fs::path csv = scratch / (b.name + ".csv");
      if (!fs::exists(csv) && !writeCsv(dataset, csv)) {
        cerr << "[ERROR] Skipping " << b.name << ": cannot write " << csv << endl;
        continue;
      }

      double maxTime = 0;
      if (!budgets.empty()) {
        auto it = budgets.find(b.name);
        if (it == budgets.end()) {
          cerr << "[ERROR] Skipping " << b.name
               << ": no time budget in the summary." << endl;
          continue;
        }
        maxTime = it->second * scale;
      }

      for (int i = 0; i < opt.runs; ++i) {
        const unsigned int seed = protocol::seedFor(opt, i);
        if (harness.alreadyDone(b.name, seed))
          continue;
        const fs::path labelPath = scratch / "labels.txt";

        ostringstream cmd;
        cmd << shellQuote(java) << " -Xmx" << heapMb << "m"
            << " -cp " << shellQuote(jar.string())
            << " harness.Runner " << shellQuote(csv.string()) << ' ' << n_data
            << ' ' << dim << ' ' << b.k << ' ' << seed << ' '
            << shellQuote(labelPath.string());
        if (maxTime > 0)
          cmd << " --budget " << setprecision(17) << maxTime;

        cmd << " 2>/dev/null";

        const auto start = system_clock::now();
        FILE *pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) {
          cerr << "[ERROR] " << b.name << " seed " << seed
               << ": cannot start the JVM" << endl;
          continue;
        }
        string out;
        char buf[512];
        while (fgets(buf, sizeof buf, pipe))
          out += buf;
        const int rc = pclose(pipe);
        const double wall = duration<double>(system_clock::now() - start).count();

        if (rc != 0) {
          cerr << "[ERROR] " << b.name << " seed " << seed
               << ": SO-BMSSC exited with status " << rc << "; skipping" << endl;
          continue;
        }

        double theirObjective = -1, theirSeconds = -1;
        {
          istringstream is(out);
          string tag;
          while (is >> tag) {
            if (tag == "RESULT") {
              is >> theirObjective >> theirSeconds;
              break;
            }
          }
        }
        if (theirSeconds < 0) {
          cerr << "[ERROR] " << b.name << " seed " << seed
               << ": no RESULT line from SO-BMSSC; skipping" << endl;
          continue;
        }

        auto labels = load_vector(labelPath.string());
        if (static_cast<int>(labels.size()) != n_data ||
            any_of(labels.begin(), labels.end(),
                   [&](int c) { return c < 0 || c >= b.k; })) {
          cerr << "[ERROR] " << b.name << " seed " << seed
               << ": SO-BMSSC returned no valid partition; skipping" << endl;
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
        r.timeSeconds = theirSeconds;
        {
          ostringstream cfg;
          cfg << "grasp=0.75,alpha=0.75,ls=first+oscillation+first,";
          if (maxTime > 0)
            cfg << "budget_x" << scale << "=" << maxTime << "s,";
          else
            cfg << "iters=100,";
          cfg << "heap_mb=" << heapMb << ","
              << "jvm_wall=" << setprecision(6) << wall << "s,"
              << "their_obj=" << setprecision(17) << theirObjective;
          r.config = cfg.str();
        }
        harness.record(r, labels);
      }
      harness.finishDataset(b.name);
    }
  }

  error_code ec;
  fs::remove_all(scratch, ec);
  return status;
}
