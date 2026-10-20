CXX       = g++
CXXFLAGS  = --std=c++23 -O3

RUNS     ?= 10
BIN       = bin

H        = src/harness
B        = src/baselines
O        = src/ours
SHARED_HDR = $(wildcard $(H)/*.hpp)
OURS_INC = -I$(O) -I$(H)
COMMON   = $(B)/shared/io.cpp $(B)/shared/metric.cpp $(B)/bkm/distance.cpp
BINC     = -I$(H) -I$(B)/shared -I$(B)/bkm

OURS      = $(BIN)/bc-ours $(BIN)/bc-ours-analysis $(BIN)/bc-tau
BASELINES = $(BIN)/bkm $(BIN)/bkmpp $(BIN)/lima $(BIN)/rkm $(BIN)/tbc \
            $(BIN)/mabmssc $(BIN)/sobmssc $(BIN)/sobmssc.jar

ifdef JAVA_HOME
JAVAC      = $(JAVA_HOME)/bin/javac
JAR        = $(JAVA_HOME)/bin/jar
else
JAVAC      = javac
JAR        = jar
endif
SO         = $(B)/sobmssc
SO_SRC     = $(shell find $(SO)/upstream -name '*.java' 2>/dev/null) $(SO)/Runner.java
SOBMSSC_SRC = $(COMMON) $(SO)/main.cpp

.PHONY: all ours baselines verify-data clean \
        sobmssc exp-pruning exp-runtime exp-optimization exp-convergence exp-calibration exp-benchmark exp-bbc exp-feasibility exp-exactness exp-acceptance exp-relaxation exp-comparison
all: ours baselines
ours: $(OURS)
baselines: $(BASELINES)

$(BIN):
	@mkdir -p $(BIN)

$(BIN)/bc-ours: $(O)/main.cpp $(O)/*.hpp $(SHARED_HDR) | $(BIN)
	$(CXX) $(CXXFLAGS) $(OURS_INC) $< -o $@.tmp && mv -f $@.tmp $@

$(BIN)/bc-ours-analysis: $(O)/main.cpp $(O)/*.hpp $(SHARED_HDR) | $(BIN)
	$(CXX) $(CXXFLAGS) -DANALYSIS $(OURS_INC) $< -o $@.tmp && mv -f $@.tmp $@

$(BIN)/bc-tau: $(O)/tau.cpp $(O)/*.hpp $(SHARED_HDR) | $(BIN)
	$(CXX) $(CXXFLAGS) $(OURS_INC) $< -o $@.tmp && mv -f $@.tmp $@

COMMON_HDR  = $(B)/shared/io.hpp $(B)/shared/metric.hpp $(B)/shared/stat.hpp \
              $(SHARED_HDR)
BKM_SRC     = $(COMMON) $(B)/bkm/interface.cpp $(B)/bkm/bkm.cpp \
              $(B)/bkm/hungarian.cpp $(B)/bkm/main.cpp
BKMPP_SRC   = $(COMMON) $(B)/bkmpp/cluster.cpp $(B)/bkmpp/coordinate.cpp \
              $(B)/bkmpp/kMeans.cpp $(B)/bkmpp/point.cpp $(B)/bkmpp/main.cpp
LIMA_SRC    = $(COMMON) $(B)/lima/DistanceMatrix.cpp $(B)/lima/LocalSearch.cpp \
              $(B)/lima/Pair.cpp $(B)/lima/Point.cpp $(B)/lima/Random.cpp \
              $(B)/lima/Solution.cpp $(B)/lima/Vns.cpp $(B)/lima/main.cpp
RKM_SRC     = $(COMMON) $(B)/rkm/k_means.cc $(B)/rkm/network_simplex.cc \
              $(B)/rkm/regularized_k_means.cc $(B)/rkm/main.cpp
TBC_SRC     = $(COMMON) $(B)/tbc/tau_balanced_clustering.cpp $(B)/tbc/main.cpp
MABMSSC_SRC = $(COMMON) $(B)/mabmssc/ma_bmssc_upstream.cpp $(B)/mabmssc/main.cpp

$(BIN)/bkm: $(BKM_SRC) $(B)/bkm/distance.cpp $(COMMON_HDR) | $(BIN)
	$(CXX) $(CXXFLAGS) $(BINC) $(BKM_SRC) -o $@.tmp && mv -f $@.tmp $@

$(BIN)/bkmpp: $(BKMPP_SRC) $(COMMON_HDR) | $(BIN)
	$(CXX) $(CXXFLAGS) $(BINC) -I$(B)/bkmpp $(BKMPP_SRC) -o $@.tmp && mv -f $@.tmp $@

$(BIN)/lima: $(LIMA_SRC) $(COMMON_HDR) | $(BIN)
	$(CXX) $(CXXFLAGS) $(BINC) -I$(B)/lima $(LIMA_SRC) -o $@.tmp && mv -f $@.tmp $@

$(BIN)/rkm: $(RKM_SRC) $(COMMON_HDR) | $(BIN)
	$(CXX) $(CXXFLAGS) $(BINC) -I$(B)/rkm $(RKM_SRC) -pthread -o $@.tmp && mv -f $@.tmp $@

$(BIN)/tbc: $(TBC_SRC) $(COMMON_HDR) | $(BIN)
	$(CXX) $(CXXFLAGS) $(BINC) -I$(B)/tbc $(TBC_SRC) -o $@.tmp && mv -f $@.tmp $@

MABMSSC_UPSTREAM_FLAGS = -include cmath -Dmain=ma_bmssc_distributed_main
$(BIN)/mabmssc: $(MABMSSC_SRC) $(COMMON_HDR) | $(BIN)
	$(CXX) $(filter-out -s,$(CXXFLAGS)) $(MABMSSC_UPSTREAM_FLAGS) -c $(B)/mabmssc/ma_bmssc_upstream.cpp -o $(BIN)/.ma_bmssc_upstream.o
	$(CXX) $(CXXFLAGS) $(BINC) -I$(B)/mabmssc $(COMMON) $(B)/mabmssc/main.cpp $(BIN)/.ma_bmssc_upstream.o -o $@.tmp && mv -f $@.tmp $@

$(BIN)/sobmssc.jar: $(SO_SRC) | $(BIN)
	@command -v $(JAVAC) >/dev/null || { \
	  echo "no javac found; install JDK 17 or set JAVA_HOME (see README)" >&2; \
	  exit 1; }
	@rm -rf $(BIN)/.sobmssc-classes && mkdir -p $(BIN)/.sobmssc-classes
	$(JAVAC) -nowarn -d $(BIN)/.sobmssc-classes -sourcepath $(SO)/upstream:$(SO) $(SO_SRC)
	$(JAR) cf $@ -C $(BIN)/.sobmssc-classes .
	@rm -rf $(BIN)/.sobmssc-classes

$(BIN)/sobmssc: $(SOBMSSC_SRC) $(COMMON_HDR) | $(BIN)
	$(CXX) $(CXXFLAGS) $(BINC) $(SOBMSSC_SRC) -o $@.tmp && mv -f $@.tmp $@

sobmssc: $(BIN)/sobmssc.jar $(BIN)/sobmssc

verify-data:
	@python3 tools/prepare/prepare.py verify -v

exp-pruning: $(BIN)/bc-ours-analysis
	@RUNS=$(RUNS) experiments/pruning/run.sh
	@python3 experiments/pruning/tables.py

exp-runtime: $(BIN)/bc-ours-analysis
	@RUNS=$(RUNS) experiments/runtime/run.sh
	@python3 experiments/runtime/tables.py

exp-optimization: $(BIN)/bc-ours
	@RUNS=$(RUNS) experiments/optimization/run.sh
	@python3 experiments/optimization/tables.py

exp-convergence:
	@python3 experiments/convergence/tables.py
	@python3 experiments/convergence/figure.py

exp-calibration: $(BIN)/bc-ours $(BIN)/rkm
	@RUNS=$(RUNS) experiments/calibration/run.sh
	@python3 experiments/calibration/figure.py --grid ''

exp-comparison:
	@python3 experiments/comparison/figure.py

exp-benchmark: all
	@RUNS=$(RUNS) experiments/benchmark/run.sh
	@python3 experiments/benchmark/tables.py
	@python3 experiments/benchmark/significance.py

exp-bbc: all
	@RUNS=$(RUNS) experiments/bbc/run.sh
	@python3 experiments/bbc/tables.py

exp-feasibility: $(BIN)/tbc
	@RUNS=$(RUNS) experiments/feasibility/run.sh
	@python3 experiments/feasibility/tables.py

exp-exactness: $(BIN)/bc-ours
	@RUNS=$(RUNS) experiments/exactness/run.sh
	@python3 experiments/exactness/tables.py

exp-acceptance: $(BIN)/bc-ours
	@RUNS=$(RUNS) experiments/acceptance/run.sh
	@python3 experiments/acceptance/tables.py

exp-relaxation: all
	@RUNS=$(RUNS) experiments/relaxation/run.sh
	@python3 experiments/relaxation/tables.py

clean:
	@rm -f $(BIN)/* 2>/dev/null || true
	@rmdir $(BIN) 2>/dev/null || true
	@echo "cleaned"
