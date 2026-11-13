#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { int value=7; double result=T09Cpp::most(2.25,&value,-7,true); return result==9.5 && value==19 ? 0 : 1; }
