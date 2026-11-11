#include "api.h"
int main() { T09RegCallback callback = T09Cpp::get_callback(); return T09Cpp::calculate(true) || !T09Cpp::calculate(false) || callback(true) || !callback(false); }
