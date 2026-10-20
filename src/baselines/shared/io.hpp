#pragma once
#include <vector>
#include <string>
#include <fstream>

using namespace std;

const vector<int> load_vector(const string& path);
const vector<vector<double>> load_matrix(const string& path);
void write_vector(const vector<int>& vec, const string& path);
void write_matrix(const vector<vector<double>>& matrix, const string& path);
void write_log(const vector<double>& metrics, const string& path, const ios::openmode mode);
void write_summary(const int runs, const string& log_path, const string& summary_path, const ios::openmode mode);
void print_summary(
    const bool write_to_file,
    const bool only_last_step,
    const string& input_name,
    const int n_clusters,
    const int max_iters,
    const vector<vector<double>>& metrics,
    const string& summary_path="");