#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp { int sum(int a,int b) { return 900+a+b; } int sum(int count,...) { __builtin_va_list ap; __builtin_va_start(ap,count); int a=__builtin_va_arg(ap,int); double b=__builtin_va_arg(ap,double); int c=__builtin_va_arg(ap,int); __builtin_va_end(ap); return count+a+(int)(b*10.0)+c; } Callback fetchCallback() { return static_cast<Callback>(sum); } }
