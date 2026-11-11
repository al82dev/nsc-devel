#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvMixedID { int x, y; double z; } T09SysvMixedID;
API_STORAGE __attribute__((sysv_abi)) T09SysvMixedID sysv_mixed_id(T09SysvMixedID value);
