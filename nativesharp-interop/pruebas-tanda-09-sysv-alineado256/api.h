#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct __declspec(align(256)) T09SysvAlign256 { float x, y, z, w; } T09SysvAlign256;
API_STORAGE __attribute__((sysv_abi)) T09SysvAlign256 sysv_align256(T09SysvAlign256 value);
