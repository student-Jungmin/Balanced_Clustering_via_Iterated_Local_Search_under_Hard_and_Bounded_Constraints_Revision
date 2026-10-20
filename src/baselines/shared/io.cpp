#include <cmath>
#include <iomanip>
#include <sstream>
#include <iostream>

#include "metric.hpp"
#include "io.hpp"

const vector<int> load_vector(const string& path){
    vector<int> vec;
    ifstream ifs(path);

    if(!ifs){
        cerr << "Cannot open " << path << endl;
        exit(-1);
    }

    string line;

    while(getline(ifs, line)){
        if(line == "EOF"){
            break;
        }

        vec.push_back(stod(line));
    }

    return vec;
}

const vector<vector<double>> load_matrix(const string& path){
    vector<vector<double>> matrix;
    ifstream ifs(path);

    if(!ifs){
        cerr << "Cannot open " << path << endl;
        exit(-1);
    }

    string line;
    string word;
    istringstream iss;
    vector<double> vec;

    while(getline(ifs, line)){
        if(line == "EOF"){
            break;
        }
        
        iss.clear();
        vec.clear();
        iss.str(line);

        if(matrix.empty() && line.find_first_of(",;") != string::npos){
            cerr << "Not whitespace-separated (comma or semicolon on the first "
                    "row): " << path << endl;
            cerr << "Run `python3 tools/prepare/prepare.py verify`" << endl;
            exit(-1);
        }

        while(iss >> word){
            vec.push_back(stod(word));
        }

        if(!matrix.empty() && vec.size() != matrix.front().size()){
            cerr << "Row " << matrix.size() + 1 << " has " << vec.size()
                 << " values, not " << matrix.front().size() << ": " << path
                 << endl;
            exit(-1);
        }

        matrix.push_back(vec);
    }

    ifs.close();
    
    return matrix;
}


void write_vector(const vector<int>& vec, const string& path){
    ofstream out(path);

    if(!out.is_open()){
        cout << "Unable to open " << path << endl;
        exit(-1);
    }

    for(const auto& e: vec){
        out << e << endl;
    }
    
    out.close();
    
    return;
}

void write_matrix(const vector<vector<double>>& matrix, const string& path){
    ofstream out(path);

    if(!out.is_open()){
        cout << "Unable to open " << path << endl;
        exit(-1);
    }

    for(const auto vec: matrix){
        int dim = static_cast<int>(vec.size());

        for(int i = 0; i < dim; ++i){
            out << vec[i];
            out << ((i < (dim-1)) ? " " : "\n");
        }
    }

    out.close();

    return;
}

void write_log(const vector<double>& metrics, const string& path, const ios::openmode mode){
    ofstream out(path, mode);

    if(!out.is_open()){
        cout << "Unable to open " << path << endl;
        exit(-1);
    }

    int n_metrics = static_cast<int>(metrics.size());

    for(int i = 0; i < n_metrics; ++i){
        out << metrics[i];
        out << ((i < (n_metrics - 1)) ? " " : "\n");
    }
    
    out.close();
}

void write_summary(const int runs, const string& log_path, const string& summary_path, const ios::openmode mode){
    vector<vector<double>> log = load_matrix(log_path);

    int n_metrics = static_cast<int>(log[0].size());
    
    vector<vector<double>> tr_log(n_metrics, vector<double>(runs));

    for(int i = 0; i < n_metrics; ++i){
        for(int j = 0; j < runs; ++j){
            tr_log[i][j] = log[j][i];
        }
    }

    ofstream out(summary_path, mode);

    if(!out.is_open()){
        cout << "Unable to open " << summary_path << endl;
        exit(-1);
    }

    out.precision(2);

    for(int i = 0; i < n_metrics; ++i){
        double avg = compute_avg(tr_log[i]);
        double sd = compute_sd(avg, tr_log[i]); 
        out << avg << "(±" << sd << ")";
        out << ((i < (n_metrics - 1)) ? " " : "\n");
    }

    out.close();
}

void print_summary(
    const bool write_to_file,
    const bool only_last_step,
    const string& input_name,
    const int n_clusters,
    const int max_iters,
    const vector<vector<double>>& metrics,
    const string& summary_path){
    
    int runs = static_cast<int>(metrics.size());
    int n_metrics = static_cast<int>(metrics[0].size());

    vector<string> words;

    words.push_back(input_name);
    words.push_back(to_string(n_clusters));
    words.push_back(to_string(max_iters));

    if(only_last_step){
        int last_idx = runs - 1;

        for(int i = 0; i < n_metrics; ++i){
            ostringstream oss;
            oss << fixed << setprecision(2) << metrics[last_idx][i]; 
            words.push_back(oss.str());
        }
    }
    else{   
        vector<vector<double>> tr_metrics(n_metrics, vector<double>(runs));

        for(int i = 0; i < n_metrics; ++i){
            for(int j = 0; j < runs; ++j){
                tr_metrics[i][j] = metrics[j][i];
            }
        }

        for(int i = 0; i < n_metrics; ++i){
            ostringstream oss;
            double avg = compute_avg(tr_metrics[i]);
            double sd = compute_sd(avg, tr_metrics[i]);
            oss << fixed << setprecision(2) << avg << "(±" << fixed << setprecision(2) << sd << ")"; 
            words.push_back(oss.str());
        }
    }

    vector<string> labels = {
        "File name: ",
        "Number of clusters: ",
        "Maximum iteration: ",
        "Number of iteration: ",
        "Used time: ",
        "MSE: ",
        "Silhouette score: ",
        "Davies-Bouldin index: "
    };

    if(n_metrics == 7){
        labels.push_back("ARI: ");
        labels.push_back("NMI: ");
    }

    string header;
    
    if(only_last_step){
        ostringstream oss;
        oss << "[Summary of " << runs << "-th execution]";
        header = oss.str();
    }
    else{
        header = "[Overall summary of the experimental results]";
    }

    int n_words = static_cast<int>(words.size());

    cout << header << endl;

    for(int i = 0; i < n_words; ++i){
        cout << labels[i] << words[i] << endl;
    }

    cout << endl;

    if(write_to_file){
        ofstream out(summary_path);

        if(!out.is_open()){
            cerr << "Cannot open " << summary_path << endl;
            exit(-1);
        }

        for(int i = 0; i < n_words; ++i){
            out << words[i];
            out << ((i < (n_words - 1)) ? " " : "\n");
        }

        out.close();
    }
}