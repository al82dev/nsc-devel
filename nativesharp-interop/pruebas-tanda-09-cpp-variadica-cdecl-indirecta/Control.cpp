#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { auto callback=T09Cpp::fetchCallback(); return callback(3,(signed char)-3,2.5f,(unsigned short)7)==32 && T09Cpp::sum(2,9)==911 ? 0 : 1; }
