#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct __declspec(align(16)) T09SysvAlign16Indirect { float x, y, z, w; int a; } T09SysvAlign16Indirect;
API_STORAGE __attribute__((sysv_abi)) T09SysvAlign16Indirect sysv_align16_indirect(T09SysvAlign16Indirect value);
