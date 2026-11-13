#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
struct T09VarValue { double x; int y; };
namespace T09Cpp { enum class Mode : unsigned short { Chosen=65535 }; API_STORAGE int __attribute__((sysv_abi)) sum(int count,...); API_STORAGE int sum(int a,int b); }