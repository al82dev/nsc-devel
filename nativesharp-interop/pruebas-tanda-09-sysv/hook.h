typedef struct T09Hook { int (*callback)(int); int tag; } T09Hook;
__attribute__((sysv_abi)) T09Hook sysv_hook(T09Hook value);
