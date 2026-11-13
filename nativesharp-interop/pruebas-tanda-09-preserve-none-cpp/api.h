#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp { API_STORAGE double __attribute__((preserve_none)) most(double x,int* p,short s,bool b); }
