#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp {
int sum(int a,int b) { return 900+a+b; }
int sum(int count,...) {
    __builtin_va_list ap; __builtin_va_start(ap,count);
    T09VarValue value=__builtin_va_arg(ap,T09VarValue);
    __builtin_va_end(ap);
    return count==1 && value.x==2.25 && value.y==7 ? 32 : 0;
}
}