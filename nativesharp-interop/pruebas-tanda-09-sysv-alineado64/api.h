#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct __declspec(align(64)) T09SysvAlign64 { float x, y, z, w; } T09SysvAlign64;
API_STORAGE __attribute__((sysv_abi)) T09SysvAlign64 sysv_align64(T09SysvAlign64 value);
