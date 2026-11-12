#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct PairMixed { double x, y, z; } PairMixed;
API void __attribute__((regcall)) write_pair(PairMixed value, int* out);
typedef void (__attribute__((regcall)) *WriteFn)(PairMixed, int*);
API WriteFn get_write(void);
