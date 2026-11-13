#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { return T09Cpp::sum(3,(signed char)-3,2.5f,(unsigned short)7)==32 && T09Cpp::sum(2,9)==911 ? 0 : 1; }
