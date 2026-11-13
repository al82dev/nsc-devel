#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09VarValue v = {2.25,7}; return T09Cpp::sum(1,v)==32 && v.x==2.25 && v.y==7 ? 0 : 1; }
