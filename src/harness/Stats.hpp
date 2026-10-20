#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace stats {

class Accumulator {
public:
  void push(double x) {
    ++n_;
    const double delta = x - mean_;
    mean_ += delta / static_cast<double>(n_);
    m2_ += delta * (x - mean_);
    min_ = std::min(min_, x);
    max_ = std::max(max_, x);
  }

  std::size_t n() const { return n_; }
  double mean() const { return n_ ? mean_ : 0.0; }

  double sampleSd() const {
    if (n_ < 2)
      return std::numeric_limits<double>::quiet_NaN();
    return std::sqrt(m2_ / static_cast<double>(n_ - 1));
  }

  double populationSd() const {
    return n_ ? std::sqrt(m2_ / static_cast<double>(n_)) : 0.0;
  }

  double min() const {
    return n_ ? min_ : std::numeric_limits<double>::quiet_NaN();
  }
  double max() const {
    return n_ ? max_ : std::numeric_limits<double>::quiet_NaN();
  }

private:
  std::size_t n_ = 0;
  double mean_ = 0.0;
  double m2_ = 0.0;
  double min_ = std::numeric_limits<double>::infinity();
  double max_ = -std::numeric_limits<double>::infinity();
};

inline Accumulator summarize(const std::vector<double> &values) {
  Accumulator acc;
  for (double v : values)
    acc.push(v);
  return acc;
}

inline double mean(const std::vector<double> &v) { return summarize(v).mean(); }
inline double sampleSd(const std::vector<double> &v) {
  return summarize(v).sampleSd();
}

}
