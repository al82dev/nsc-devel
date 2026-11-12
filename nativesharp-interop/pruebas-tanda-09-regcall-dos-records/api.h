#pragma once
typedef struct Pair { float x, y; } Pair;
typedef struct Triple { Pair pair; float z; } Triple;
Triple __attribute__((regcall)) combine(Triple a, Triple b, int bias);
typedef Triple (__attribute__((regcall)) *CombineFn)(Triple, Triple, int);
CombineFn get_combine(void);
