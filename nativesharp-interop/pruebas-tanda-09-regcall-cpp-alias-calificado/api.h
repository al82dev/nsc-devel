#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
typedef bool (__attribute__((regcall)) *T09RegCallback)(bool value);
API_STORAGE bool __attribute__((regcall)) calculate(bool value);
API_STORAGE T09RegCallback get_callback();
}
