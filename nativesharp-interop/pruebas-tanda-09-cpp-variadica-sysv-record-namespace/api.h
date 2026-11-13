#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace Types { struct T09VarValue { double x; int y; }; }
namespace Other { struct T09VarValue { long long x; int y; }; }
namespace T09Cpp { API_STORAGE int __attribute__((sysv_abi)) sum(int count,...); API_STORAGE int sum(int a,int b); }