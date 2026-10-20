#include <limits>

#include "hungarian.hpp"

Hungarian::Hungarian(){}
Hungarian::Hungarian(const vector<vector<double>>& _weight)
: n_data(_weight.size())
, weight(_weight)
, nx(n_data, -1)
, ny(n_data, -1) {}

const vector<int> Hungarian::get_nx() const{
    return nx;
}
const vector<int> Hungarian::get_ny() const{
    return ny;
}

double const Hungarian::run(){
    const int n = n_data;
    const double INF = std::numeric_limits<double>::infinity();
    vector<double> u(n + 1, 0.0), v(n + 1, 0.0), minv(n + 1);
    vector<int> p(n + 1, 0), way(n + 1, 0);
    vector<bool> used(n + 1);

    for(int i = 1; i <= n; ++i){
        p[0] = i;
        int j0 = 0;
        fill(minv.begin(), minv.end(), INF);
        fill(used.begin(), used.end(), false);
        do{
            used[j0] = true;
            const int i0 = p[j0];
            double delta = INF;
            int j1 = 0;
            for(int j = 1; j <= n; ++j){
                if(used[j]) continue;
                const double cur = -weight[i0 - 1][j - 1] - u[i0] - v[j];
                if(cur < minv[j]){
                    minv[j] = cur;
                    way[j] = j0;
                }
                if(minv[j] < delta){
                    delta = minv[j];
                    j1 = j;
                }
            }
            for(int j = 0; j <= n; ++j){
                if(used[j]){
                    u[p[j]] += delta;
                    v[j] -= delta;
                }
                else{
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while(p[j0] != 0);
        do{
            const int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while(j0);
    }

    for(int j = 1; j <= n; ++j){
        nx[p[j] - 1] = j - 1;
        ny[j - 1] = p[j] - 1;
    }

    double maximum_weight = 0.0;
    for(int x = 0; x < n; ++x){
        maximum_weight += weight[x][nx[x]];
    }
    return maximum_weight;
}
