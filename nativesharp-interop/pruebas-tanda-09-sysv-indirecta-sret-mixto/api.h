#pragma once
typedef struct T09SretSplit { double x; int y; } T09SretSplit;
typedef struct T09SretLarge { int a, b, c, d, e; } T09SretLarge;
typedef T09SretLarge (__attribute__((sysv_abi)) *T09SretMixedCallback)(int, T09SretSplit, T09SretLarge, int);
T09SretMixedCallback t09_get_sret_mixed_callback(void);
