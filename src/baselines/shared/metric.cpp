#include <set>
#include <cmath>
#include <limits>
#include <numeric>
#include <algorithm>

#include "metric.hpp"
#include "distance.hpp"

const double eps = 1e-10;

const double comb2(int n){return static_cast<double>(n * (n - 1)) / 2.0;}

const double compute_L2(const vector<double>& x, const vector<double>& y){
    return sqrt(compute_squared_deviation(x, y));
}

const double compute_sse(const vector<int>& labels, const vector<vector<double>>& centroids, const vector<vector<double>>& data){
    auto n_data = data.size();
    
    double sse = 0;

    for(auto i = 0; i < n_data; ++i){
        sse += compute_squared_deviation(data[i], centroids[labels[i]]);
    }  

    return sse;
}

const double compute_sse(const vector<int>& capacity, const vector<vector<int>>& clusters, const vector<vector<double>>& centroids, const vector<vector<double>>& data){
    auto n_clusters = clusters.size();
    double sse = 0;
    
    for(auto i = 0; i < n_clusters; ++i){
        for(auto j = 0; j < capacity[i]; ++j){
            sse += compute_squared_deviation(data[clusters[i][j]], centroids[i]);
        }
    }

    return sse;
}

const double compute_mse(const vector<int>& labels, const vector<vector<double>>& centroids, const vector<vector<double>>& data){
    auto n_data = data.size();
    
    return compute_sse(labels, centroids, data) / static_cast<double>(n_data);
}

const double compute_mse(const vector<int>& capacity, const vector<vector<int>>& clusters, const vector<vector<double>>& centroids, const vector<vector<double>>& data){
  auto n_data = data.size();

    return compute_sse(capacity, clusters, centroids, data) / static_cast<double>(n_data);
}

const double compute_ss(const int n_clusters, const vector<int>& labels, const vector<vector<double>>& data) {
    int n_data = static_cast<int>(data.size());
    double total_score = 0;

    for (int i = 0; i < n_data; ++i) {
        int cluster_i = labels[i];
        double a = 0;
        int a_cnt = 0;

        for (int j = 0; j < n_data; ++j) {
            if ((i != j) && (labels[j] == cluster_i)) {
                a += compute_L2(data[i], data[j]);
                a_cnt++;
            }
        }
        if (a_cnt > 0) {
            a /= static_cast<double>(a_cnt);
        }

        double b = numeric_limits<double>::max();
        for (int c = 0; c < n_clusters; ++c) {
            if (c == cluster_i){
                continue;
            }

            int cnt = 0;
            double dist_sum = 0;
        
            for (int j = 0; j < n_data; ++j) {
                if (labels[j] == c) {
                    dist_sum += compute_L2(data[i], data[j]);
                    cnt++;
                }
            }

            if (cnt > 0) {
                double avg_dist = dist_sum / static_cast<double>(cnt);
                b = min(b, avg_dist);
            }
        }

        double denom = max(a, b);
        double s = (denom < eps) ? 0 : (b - a) / denom;

        total_score += s;
    }

    return total_score / static_cast<double>(n_data);
}

const double compute_dbi(const vector<int>& labels, const vector<vector<double>>& centroids, const vector<vector<double>>& data) {
    int n_data = static_cast<int>(data.size());
    int n_clusters = static_cast<int>(centroids.size());
    vector<double> S(n_clusters, 0);
    vector<int> counts(n_clusters, 0);

    for (int i = 0; i < n_data; ++i) {
        double dist = compute_L2(data[i], centroids[labels[i]]);

        S[labels[i]] += dist;
        counts[labels[i]]++;
    }

    for (int i = 0; i < n_clusters; ++i){
        if (counts[i]){
            S[i] /= static_cast<double>(counts[i]);
        } 
    }
        

    double dbi = 0;

    for (int i = 0; i < n_clusters; ++i) {
        double max_r = 0;

        for (int j = 0; j < n_clusters; ++j) {
            if (i == j){
                continue;
            }

            double dist = compute_L2(centroids[i], centroids[j]);
            
            if (dist != 0){
                max_r = max(max_r, (S[i] + S[j]) / dist);
            }
        }

        dbi += max_r;
    }

    return dbi / static_cast<double>(n_clusters);
}

const double compute_ari(const vector<int>& pred, const vector<int>& ground_truth){
    int n_data = static_cast<int>(ground_truth.size());

    unordered_map<int, unordered_map<int, int>> contingency;
    unordered_map<int, int> true_counts, pred_counts;

    for (int i = 0; i < n_data; ++i) {
        contingency[ground_truth[i]][pred[i]]++;
        true_counts[ground_truth[i]]++;
        pred_counts[pred[i]]++;
    }

    double sum_comb = 0;

    for (auto& row : contingency){
        for (auto& col : row.second){
            sum_comb += comb2(col.second);
        }
    }
    
    double sum_true = 0;
    double sum_pred = 0;

    for (auto& it : true_counts){
        sum_true += comb2(it.second);
    }

    for (auto& it : pred_counts){
        sum_pred += comb2(it.second);
    } 

    double expected_idx = (sum_true * sum_pred) / comb2(n_data);
    double max_idx = (sum_true + sum_pred) / 2.0;

    return (sum_comb - expected_idx) / (max_idx - expected_idx + eps);
}

const double compute_entropy(const unordered_map<int, int>& counts, int n) {
    double entropy = 0;

    for (const auto& kv : counts) {
        double p = static_cast<double>(kv.second) / static_cast<double>(n);
        entropy -= p * log(p + eps);
    }

    return entropy;
}

const double compute_nmi(const vector<int>& pred, const vector<int>& ground_truth) {
    int n_data = static_cast<int>(ground_truth.size());
    unordered_map<int, int> true_counts, pred_counts;
    unordered_map<int, unordered_map<int, int>> contingency;

    for (int i = 0; i < n_data; ++i) {
        true_counts[ground_truth[i]]++;
        pred_counts[pred[i]]++;
        contingency[ground_truth[i]][pred[i]]++;
    }

    double mi = 0;

    for (const auto& x : contingency) {
        for (const auto& y : x.second) {
            double joint = static_cast<double>(y.second) / static_cast<double>(n_data);
            double px = static_cast<double>(true_counts[x.first]) / static_cast<double>(n_data);
            double py = static_cast<double>(pred_counts[y.first]) / static_cast<double>(n_data);
            mi += joint * log(joint / (px * py) + eps);
        }
    }

    double h_true = compute_entropy(true_counts, n_data);
    double h_pred = compute_entropy(pred_counts, n_data);
    
    return 2 * mi / (h_true + h_pred + eps);
}

const double compute_avg(const vector<double>& metric){
    return accumulate(metric.begin(), metric.end(), 0.0) / static_cast<double>(metric.size());
}

const double compute_sd(const double avg, const vector<double>& metric){
    int runs = static_cast<int>(metric.size());
    vector<double> se(runs);

    transform(metric.begin(), metric.end(), se.begin(), [&](double m){return pow((m-avg), 2);});
    
    return sqrt(accumulate(se.begin(), se.end(), 0.0) / static_cast<double>(runs));
}