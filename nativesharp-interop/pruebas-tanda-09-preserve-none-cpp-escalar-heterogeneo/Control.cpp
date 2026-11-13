#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { return T09Cpp::scalar(-7,2.5f,9,-3,4.25)==-71.5 && T09Cpp::scalar(7,2.5f,9,-3,4.25)==42.5 ? 0 : 1; }
