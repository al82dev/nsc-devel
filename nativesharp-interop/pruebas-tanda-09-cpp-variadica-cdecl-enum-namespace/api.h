#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace Other { enum Mode : unsigned short { Chosen=5, Changed=9 }; }
namespace T09Cpp { enum Mode : unsigned short { Chosen=65535, Changed=11 }; API_STORAGE int sum(int count,...); API_STORAGE int sum(int a,int b); }
