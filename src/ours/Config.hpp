#pragma once
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

enum class TerminationType { Default, W, CV, Delta };
enum class InitMethod { Random, KMeansPP };
enum class LocalSearch { Once, Sync, Adaptive, SyncEarly };

enum class Ranking { Prefix, Full };
enum class Acceptance { Better, RandomWalk };

struct Termination {
    TerminationType type;
    double param1;
    double param2;
};

struct Config {
    int maxIter;
    int nCluster;
    unsigned int seed;

    bool enableTau;
    bool enableGeo;

    bool exactCentroids;
    bool passCentroids;
    bool rebuildPairs;
    bool revalidateTau;

    double tauRatio;
    bool hardTau = false;

    InitMethod initMethod;
    LocalSearch localSearch;
    Ranking ranking;
    Acceptance acceptance;
    vector<Termination> terminations;

    Config(int _maxIter = 100
        , int _nCluster = 10   
        , unsigned int _seed = 42
        , bool _enableTau = false
        , bool _enableGeo = true 
        , bool _exactCentroids = false
        , bool _passCentroids = false
        , bool _rebuildPairs = false
        , bool _revalidateTau = false
        , double _tauRatio = 0.0
        , InitMethod _initMethod = InitMethod::KMeansPP
        , LocalSearch _localSearch = LocalSearch::Sync
        , Ranking _ranking = Ranking::Prefix
        , Acceptance _acceptance = Acceptance::RandomWalk
        , vector<Termination> _terminations = {{TerminationType::CV, 3, 0.001}})
        : maxIter(_maxIter)
        , nCluster(_nCluster)
        , seed(_seed)
        , enableTau(_enableTau)
        , enableGeo(_enableGeo)
        , exactCentroids(_exactCentroids)
        , passCentroids(_passCentroids)
        , rebuildPairs(_rebuildPairs)
        , revalidateTau(_revalidateTau)
        , tauRatio(_tauRatio)
        , initMethod(_initMethod)
        , localSearch(_localSearch)
        , ranking(_ranking)
        , acceptance(_acceptance)
        , terminations(_terminations) {}

    static inline const vector<string> LogNames = { "None", "Console", "SimpleConsole", "File", "SimpleFile" };
    static inline const vector<string> InitNames  = { "Random", "KMeansPP" };
    static inline const vector<string> LocalNames = { "Once", "Sync", "Adaptive", "SyncEarly" };
        static inline const vector<string> RankingNames = { "Prefix", "Full" };
    static inline const vector<string> AcceptNames = { "Better", "RandomWalk" };
    static inline const vector<string> TermNames   = { "Default", "W", "CV", "Delta" };

    friend ostream& operator<<(ostream& os, const Config& config) {
        os << "[INFO] Selected Configurations" << endl;
        os << "[INFO] maxIter        : " << config.maxIter << endl;
        os << "[INFO] seed           : " << config.seed << endl;
        os << "[INFO] nCluster       : " << config.nCluster << endl;
        os << "[INFO] enableTau      : " << (config.enableTau ? "true" : "false") << endl;
        os << "[INFO] enableGeo      : " << (config.enableGeo ? "true" : "false") << endl;
        if (config.exactCentroids || config.passCentroids || config.rebuildPairs || config.revalidateTau) {
            os << "[INFO] exactness      : "
               << (config.exactCentroids ? "exactCentroids " : "")
               << (config.passCentroids ? "passCentroids " : "")
               << (config.rebuildPairs ? "rebuildPairs " : "")
               << (config.revalidateTau ? "revalidateTau" : "") << endl;
        }
        os << "[INFO] tauRatio       : " << fixed << setprecision(2) << config.tauRatio << endl;
        os << "[INFO] initMethod     : " << config.InitNames[static_cast<int>(config.initMethod)] << endl;
        os << "[INFO] localSearch    : " << config.LocalNames[static_cast<int>(config.localSearch)] << endl;
        os << "[INFO] ranking        : " << config.RankingNames[static_cast<int>(config.ranking)] << endl;
        os << "[INFO] acceptance     : " << config.AcceptNames[static_cast<int>(config.acceptance)] << endl;
        os << "[INFO] terminations   : ";
        for (size_t i = 0; i < config.terminations.size(); ++i) {
            const auto& t = config.terminations[i];
            os << "{" << config.TermNames[static_cast<int>(t.type)] 
               << ", p1:" << t.param1 << ", p2:" << t.param2 << "}";
            if (i < config.terminations.size() - 1) os << ", ";
        }

        return os;
    }
};
