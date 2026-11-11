#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09Small { unsigned char x, y, z, w; } T09Small;
API_STORAGE __attribute__((sysv_abi)) T09Small sysv_record_pointer(T09Small first, int *second);
