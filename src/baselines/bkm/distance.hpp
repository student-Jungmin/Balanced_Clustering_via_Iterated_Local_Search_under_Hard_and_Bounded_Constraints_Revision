#pragma once
#include <vector>

using namespace std;

const double compute_squared_deviation(const vector<double>& x, const vector<double>& y);
const double compute_euclidean(const vector<double>& x, const vector<double>& y);
const vector<vector<double>> compute_reverse_distance_matrix(const vector<vector<double>>& X, const vector<vector<double>>& Y);