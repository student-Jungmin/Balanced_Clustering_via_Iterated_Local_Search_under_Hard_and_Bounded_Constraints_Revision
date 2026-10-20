#include <cmath>
#include <limits>

#include "distance.hpp"

const double compute_squared_deviation(const vector<double>& x, const vector<double>& y){
    auto dim = x.size();
    double sd = 0;

    for(auto i = 0; i < dim; ++i){
        sd += (x[i] - y[i]) * (x[i] - y[i]);
    }

    return sd;
}

const double compute_euclidean(const vector<double>& x, const vector<double>& y){
    return sqrt(compute_squared_deviation(x, y));
}

const vector<vector<double>> compute_reverse_distance_matrix(const vector<vector<double>>& X, const vector<vector<double>>& Y){
    auto n_row = X.size();
    auto n_col = Y.size();

    double max = numeric_limits<double>::min();
    vector<vector<double>> rdm(n_row, vector<double>(n_col));

    for(auto i = 0; i < n_row; ++i){
        for(auto j = 0; j < n_col; ++j){
            rdm[i][j] = compute_euclidean(X[i], Y[j]);

            if(rdm[i][j] > max){
                max = rdm[i][j];
            }
        }
    }

    for(auto i = 0; i < n_row; ++i){
        for(auto j = 0; j < n_col; ++j){
            rdm[i][j] = max - rdm[i][j];
        }
    }

    return rdm;
}

