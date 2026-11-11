#include "api.h"
namespace T09Cpp {
bool __attribute__((regcall)) calculate(bool value) { return !value; }
T09RegCallback get_callback() { return calculate; }
}
