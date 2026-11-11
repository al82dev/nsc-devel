#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp { API_STORAGE float __attribute__((regcall)) calculate(float a, int *b, float c); }
