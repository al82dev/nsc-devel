#pragma once
typedef struct PairDouble { double x, y; } PairDouble;
double __attribute__((regcall)) sum_pair(PairDouble value, int bias);
typedef double (__attribute__((regcall)) *SumFn)(PairDouble, int);
SumFn get_sum(void);
