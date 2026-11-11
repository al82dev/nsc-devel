#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvR1 { unsigned char x; } T09SysvR1;
API_STORAGE __attribute__((sysv_abi)) T09SysvR1 sysv_r1(T09SysvR1 value);
