typedef struct R3 { unsigned char x, y, z; } R3;
typedef struct R4 { int x; } R4;
typedef struct R8 { int x, y; } R8;
typedef struct R16 { long long x, y; } R16;
typedef struct R24 { long long x, y, z; } R24;
typedef struct RF { double x; int y; } RF;
#define PASS(T) T __attribute__((sysv_abi)) pass_##T(T value) { return value; }
PASS(R3) PASS(R4) PASS(R8) PASS(R16) PASS(R24) PASS(RF)
