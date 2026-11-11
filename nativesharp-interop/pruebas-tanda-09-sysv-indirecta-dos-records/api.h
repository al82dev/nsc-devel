#pragma once
typedef struct T09MixedPair { double x; int y; } T09MixedPair;
typedef int (__attribute__((sysv_abi)) *T09MixedPairCallback)(T09MixedPair, T09MixedPair, int);
T09MixedPairCallback t09_get_mixed_pair_callback(void);
