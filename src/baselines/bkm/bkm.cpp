#include <chrono>
#include <limits>
#include <numeric>
#include <algorithm>

#include "bkm.hpp"
#include "distance.hpp"
#include "hungarian.hpp"

using namespace std::chrono;

BKM::BKM(): Clustering(){}
BKM::BKM(const int _max_iters, const int _n_clusters, const vector<vector<double>>& _data, unsigned int _seed)
: Clustering(_max_iters, _n_clusters, _data, _seed)
, n_slots(_n_clusters, 0)
{
    int base = n_data / n_clusters;
    int extra = n_data % n_clusters;

    for(int i = 0; i < n_clusters; ++i){
        n_slots[i] = (i < extra) ? (base + 1) : base;
    }
    
}

void BKM::init_centroids(){
    mt19937 gen(seed);
    vector<int> idx(n_data);

    iota(idx.begin(), idx.end(), 0);
    shuffle(idx.begin(), idx.end(), gen);

    for(int i = 0; i < n_clusters; ++i){
        centroids[i] = data[idx[i]];
    }

    return;
}

void BKM::assign_clusters(){
    vector<vector<double>> X;
    vector<int> map;
    
    for(int i = 0; i < n_clusters; ++i){
        for(int j = 0; j < n_slots[i]; ++j){
            X.push_back(centroids[i]);
            map.push_back(i);
        }
    }

    auto rdm = compute_reverse_distance_matrix(X, data);
    Hungarian hungarian(rdm);
    hungarian.run();

    vector<int> ny = hungarian.get_ny();

    for(int i = 0; i < n_data; ++i){
        labels[i] = map[ny[i]];
    }

    return;
}

void BKM::update_centroids(){
    vector<vector<double>> new_centroids(n_clusters, vector<double>(dim, 0));

    for(int i = 0; i < dim; ++i){
        for(int j = 0; j < n_data; ++j){
            new_centroids[labels[j]][i] += data[j][i];
        }

        for(int j = 0; j < n_clusters; ++j){
            new_centroids[j][i] /=  n_slots[j];
        }
    }

    centroids = move(new_centroids);

    return;
}

bool BKM::is_changed(const vector<vector<double>>& prev_centroids){
    bool is_changed = false;
    double eps = 1e-4;

    for(int i = 0; i < n_clusters; ++i){
        if(compute_squared_deviation(prev_centroids[i], centroids[i]) > eps){
            is_changed = true;
            break;
        }
    }

    return is_changed;
}

void BKM::run(){
    auto start = system_clock::now();
    n_iters = 0;

    init_centroids();

    vector<vector<double>> prev_centroids(n_clusters, vector<double>(dim));

    while(true){
        ++n_iters;
        copy(centroids.begin(), centroids.end(), prev_centroids.begin());

        assign_clusters();
        update_centroids();

        if((!is_changed(prev_centroids)) || (n_iters == max_iters)){
            break;
        }         
    }

    auto end = system_clock::now();
    elapsed_time = duration<double>(end-start).count();

    is_valid = true;
    
    return;
}



