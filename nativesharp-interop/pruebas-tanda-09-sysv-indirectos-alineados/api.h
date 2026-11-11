#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvAlignedMix { long long x, y, z; } T09SysvAlignedMix;
API_STORAGE __attribute__((sysv_abi)) T09SysvAlignedMix sysv_aligned_mix(float factor, T09SysvAlignedMix first, int n, T09SysvAlignedMix second);
