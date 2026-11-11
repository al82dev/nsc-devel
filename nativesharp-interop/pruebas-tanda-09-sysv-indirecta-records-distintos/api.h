#pragma once
typedef struct T09MixedDistinct { double x; int y; } T09MixedDistinct;
typedef struct T09TripleDistinct { int x, y, z; } T09TripleDistinct;
typedef int (__attribute__((sysv_abi)) *T09DistinctCallback)(T09MixedDistinct, T09TripleDistinct, int);
T09DistinctCallback t09_get_distinct_callback(void);
