#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp { double __attribute__((preserve_none)) most(double x,int* p,short s,bool b) { if(x!=2.25||*p!=7||s!=-7||!b)return -1.0; *p=19; return 9.5; } }
