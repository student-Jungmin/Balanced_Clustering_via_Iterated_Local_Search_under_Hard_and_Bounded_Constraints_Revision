#pragma once
#include <vector>
#include <random>

using namespace std;

class Clustering{
    public:

    int dim;
    int n_data;
    int n_iters;
    int max_iters;
    int n_clusters;
    unsigned int seed;

    bool is_valid;

    double elapsed_time;

    vector<int> labels;
    vector<vector<double>> data;
    vector<vector<double>> centroids;
    
    Clustering();
    Clustering(const int _max_iters, const int _n_clusters, const vector<vector<double>>& _data, unsigned int _seed = random_device()());

    const double get_elapsed_time() const;
    const vector<int> get_labels() const;
    const vector<vector<double>> get_centroids() const;
    
    virtual void run() = 0;
};