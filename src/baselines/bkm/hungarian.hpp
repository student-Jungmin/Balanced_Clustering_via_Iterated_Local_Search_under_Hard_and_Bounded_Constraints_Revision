#pragma once
#include <vector>

using namespace std;

class Hungarian{
    private:
    int n_data;
    vector<vector<double>> weight;
    vector<int> nx, ny;

    public:
    Hungarian();
    Hungarian(const vector<vector<double>>& _weight);

    const vector<int> get_nx() const; 
    const vector<int> get_ny() const; 

    double const run();
};
