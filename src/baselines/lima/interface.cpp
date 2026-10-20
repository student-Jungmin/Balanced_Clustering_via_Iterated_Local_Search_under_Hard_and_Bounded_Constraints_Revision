#include <iostream>

#include "interface.hpp"

using namespace std;

Clustering::Clustering(){}
Clustering::Clustering(const int _max_iters, const int _n_clusters, const vector<vector<double>>& _data, unsigned int _seed)
    : dim(static_cast<int>(_data[0].size()))
    , n_data(static_cast<int>(_data.size()))
    , n_iters(0)
    , max_iters(_max_iters)
    , n_clusters(_n_clusters)
    , seed(_seed)
    , is_valid(false)
    , elapsed_time(0)
    , labels(n_data, -1)
    , data(_data)
    , centroids(n_clusters, vector<double>(dim, 0)){}

const double Clustering::get_elapsed_time() const {
    if(!is_valid){
        cout << "clustering has not been performed yet, so the results are not available." << endl;
        exit(-1);
    }

    return elapsed_time;
}

const vector<int> Clustering::get_labels() const {
    if(!is_valid){
        cout << "clustering has not been performed yet, so the results are not available." << endl;
        exit(-1);
    }

    return labels;
}

const vector<vector<double>> Clustering::get_centroids() const {
    if(!is_valid){
        cout << "clustering has not been performed yet, so the results are not available." << endl;
        exit(-1);
    }

    return centroids;
}