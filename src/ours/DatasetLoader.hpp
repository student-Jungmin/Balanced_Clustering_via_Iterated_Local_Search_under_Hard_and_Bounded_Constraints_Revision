#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using Dataset = vector<double>; 

namespace fs = std::filesystem;

struct LoadedData {
    Dataset flatData;
    int nData;
    int nDim;
};

class DatasetLoader {
    public:
    DatasetLoader() = delete;

    static optional<LoadedData> load(const fs::path& fpath) {
        LoadedData result;
        result.nData = 0;
        result.nDim = 0;
        
        if (!fs::exists(fpath)) {
            cerr << "[ERROR] File not found: " << fpath << endl;
            return nullopt;
        }
            
        ifstream file(fpath);
        if (!file.is_open()) {
            cerr << "[ERROR] Could not open file: " << fpath << endl;
            file.close();
            return nullopt;
        }
        
        string line;
        bool isFirstLine = true;

        while (getline(file, line)) {
            if (line.empty()) continue;

            if (isFirstLine && line.find_first_of(",;") != string::npos) {
                cerr << "[ERROR] " << fpath << " is not whitespace-separated: "
                     << "found a comma or semicolon on the first row." << endl;
                cerr << "[HINT ] Run `python3 tools/prepare/prepare.py verify`."
                     << endl;
                return nullopt;
            }

            stringstream ss(line);
            double val;
            int currentDim = 0;

            while (ss >> val) {
                result.flatData.push_back(val);
                currentDim++;
            }

            if (currentDim > 0) {
                if (isFirstLine) {
                    result.nDim = currentDim;
                    isFirstLine = false;
                } else if (currentDim != result.nDim) {
                    cerr << "[ERROR] " << fpath << ": row " << result.nData + 1
                         << " has " << currentDim << " values, not "
                         << result.nDim << "." << endl;
                    return nullopt;
                }
                result.nData++;
            }
        }

        file.close();
        return result;
    }
};