#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct __declspec(align(128)) T09SysvAlign128 { float x, y, z, w; } T09SysvAlign128;
API_STORAGE __attribute__((sysv_abi)) T09SysvAlign128 sysv_align128(T09SysvAlign128 value);
