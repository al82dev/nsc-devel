#pragma once
typedef struct Pair { float x, y; } Pair;
typedef struct Triple { Pair pair; float z; } Triple;
typedef struct Result { double x, y; } Result;
Result __attribute__((regcall)) combine(Triple value, int bias);
typedef Result (__attribute__((regcall)) *CombineFn)(Triple, int);
CombineFn get_combine(void);
