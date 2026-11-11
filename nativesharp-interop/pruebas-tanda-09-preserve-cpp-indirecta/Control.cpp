#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { auto callback=T09Cpp::fetchCallback(); int value=7; double result=callback(2.25,&value,-7,true);
return callback==&T09Cpp::most && result==9.5 && value==19 ? 0 : 1; }
