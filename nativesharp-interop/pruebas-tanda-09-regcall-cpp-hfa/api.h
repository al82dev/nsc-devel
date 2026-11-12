#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09HfaCpp {
struct Pair { float x, y; };
API Pair __attribute__((regcall)) transform(Pair value);
}
