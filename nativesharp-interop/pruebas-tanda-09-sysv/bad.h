typedef struct T09Unsupported { int x, y, z, w, extra; } T09Unsupported;
typedef struct T09Other { long long x; unsigned int y; } T09Other;
__attribute__((sysv_abi)) T09Unsupported sysv_record(float first, T09Other second, float third, ...);
