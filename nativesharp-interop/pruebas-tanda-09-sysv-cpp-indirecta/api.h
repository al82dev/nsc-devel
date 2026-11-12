#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp { using Callback = double (__attribute__((sysv_abi)) *)(double,int*,short,bool); API_STORAGE double __attribute__((sysv_abi)) most(double x,int* p,short s,bool b); API_STORAGE Callback fetchCallback(); }
