#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
API_STORAGE int calculate(int a, int b, int c);
API_STORAGE int calculate(long long a, int b, int c);
}
