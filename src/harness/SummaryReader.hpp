#pragma once
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace summary {

using Row = std::vector<double>;

inline bool isNumeric(const std::string &tok) {
  if (tok.empty())
    return false;
  char *end = nullptr;
  std::strtod(tok.c_str(), &end);
  return end != tok.c_str() && *end == '\0';
}

inline std::map<std::string, Row> read(const std::filesystem::path &path) {
  std::map<std::string, Row> out;
  std::ifstream in(path);
  if (!in.is_open())
    return out;

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line.find_first_not_of("- \t") == std::string::npos)
      continue;

    std::vector<std::string> tok;
    std::istringstream ss(line);
    std::string t;
    while (ss >> t)
      tok.push_back(t);
    if (tok.size() < 2)
      continue;

    std::size_t firstNumeric = tok.size();
    while (firstNumeric > 0 && isNumeric(tok[firstNumeric - 1]))
      --firstNumeric;
    if (firstNumeric == 0 || firstNumeric == tok.size())
      continue;

    std::string name;
    for (std::size_t i = 0; i < firstNumeric; ++i)
      name += (i ? " " : "") + tok[i];

    Row row;
    for (std::size_t i = firstNumeric; i < tok.size(); ++i)
      row.push_back(std::strtod(tok[i].c_str(), nullptr));
    out[name] = row;
  }
  return out;
}

inline std::map<std::string, double> timeMeans(
    const std::filesystem::path &path) {
  std::map<std::string, double> out;
  for (const auto &[name, row] : read(path))
    if (!row.empty())
      out[name] = row[0];
  return out;
}

}
