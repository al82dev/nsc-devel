#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct __attribute__((packed)) T09Packed { unsigned char tag; double value; } T09Packed;
API_STORAGE __attribute__((sysv_abi)) int sysv_packed_small_scalar(float scale, T09Packed original, int extra);
