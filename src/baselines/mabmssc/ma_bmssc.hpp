#pragma once

#include <string>

extern int N;
extern int K;
extern int D;
extern int Runs;
extern int RTS_ITER;
extern double StartTime;
extern double ObjBest;
extern int *BestS;
extern int *CluLen;

void initialing(std::string file);
void allocateMemory();
void buildNeighbors();
void memetic(double maxTime);
void freeMemory1();
void freeMemory();
