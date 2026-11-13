#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09VarValue v = {{10,20,30},2.25}; return T09Cpp::sum(1,v)==32 && v.x[0]==10 && v.x[1]==20 && v.x[2]==30 && v.y==2.25 ? 0 : 1; }
