typedef struct T09Unsupported { int x; } T09Unsupported;
typedef struct T09Other { int x; } T09Other;
__attribute__((sysv_abi)) T09Unsupported sysv_record(T09Unsupported first, T09Other second);
