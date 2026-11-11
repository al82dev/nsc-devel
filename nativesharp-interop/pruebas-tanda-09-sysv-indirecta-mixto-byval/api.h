#pragma once
typedef struct T09SplitValue { double x; int y; } T09SplitValue;
typedef struct T09ByvalValue { int a, b, c, d, e; } T09ByvalValue;
typedef int (__attribute__((sysv_abi)) *T09SplitByvalCallback)(int, T09SplitValue, T09ByvalValue, int);
T09SplitByvalCallback t09_get_split_byval_callback(void);
