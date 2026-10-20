#include "interface.hpp"

class BKM: public Clustering{
    private:
    vector<int> n_slots;

    void init_centroids();
    
    void assign_clusters();
    void update_centroids();
    bool is_changed(const vector<vector<double>>& prev_centroids);

    public:
    BKM();
    BKM(const int _max_iters, const int _n_clusters, const vector<vector<double>>& _data, unsigned int _seed = random_device()());
    
    void run() override;
};


