#pragma once
#include <limits>
#include <vector>
#include <utility>

using namespace std;
using Cluster = vector<pair<double, int>>;
using Coordinate = vector<double>; 

struct Output {
    int iter;
    double sse;
    double time;
    vector<int> capacity;
    vector<Cluster> clusters;
    vector<Coordinate> centroids;

    Output(const int nData, const int nCluster, const int nDim) 
    : iter(0)                                       
    , sse(numeric_limits<double>::max())            
    , time(0)
    , capacity(nCluster)                     
    , clusters(nCluster)                        
    , centroids(nCluster, Coordinate(nDim, 0)) {
        int baseCapacity = nData / nCluster;
        int maxCapacity = baseCapacity + 1;
        for(int i = 0; i < nCluster; ++i)
            clusters[i].resize(maxCapacity);
    }
};
