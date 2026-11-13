#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { return T09Cpp::scalar(-3.5f)==-71 && T09Cpp::scalar(8.0f)==42 ? 0 : 1; }
