#pragma once
#include <vector>
#include <unordered_map>

using namespace std;

const double compute_sse(const vector<int>& labels, const vector<vector<double>>& centroids, const vector<vector<double>>& data);
const double compute_sse(const vector<int>& capacity, const vector<vector<int>>& clusters, const vector<vector<double>>& centroids, const vector<vector<double>>& data);
const double compute_mse(const vector<int>& labels, const vector<vector<double>>& centroids, const vector<vector<double>>& data);
const double compute_mse(const vector<int>& capacity, const vector<vector<int>>& clusters, const vector<vector<double>>& centroids, const vector<vector<double>>& data);
const double compute_ss(const int n_clusters, const vector<int>& labels, const vector<vector<double>>& data);
const double compute_dbi(const vector<int>& labels, const vector<vector<double>>& centroids, const vector<vector<double>>& data);
const double compute_ari(const vector<int>& pred, const vector<int>& ground_truth);
const double compute_entropy(const unordered_map<int, int>& counts, int n);
const double compute_nmi(const vector<int>& pred, const vector<int>& ground_truth);

const double compute_avg(const vector<double>& metric);
const double compute_sd(const double avg, const vector<double>& metric);




