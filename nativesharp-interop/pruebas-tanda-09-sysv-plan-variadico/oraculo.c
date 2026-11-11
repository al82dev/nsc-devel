typedef struct Small { int x, y; } Small;
typedef struct Mixed { double x; int y; } Mixed;
typedef struct Large { long long x, y, z; } Large;
__attribute__((sysv_abi)) int vf(int count, ...);
int first(Small a, Mixed b, Large c) { return vf(3, a, b, c); }
int pressure(Mixed a) { return vf(6, 1, 2, 3, 4, 5, a); }
int sse_pressure(Mixed a) { return vf(9, 1., 2., 3., 4., 5., 6., 7., 8., a); }
