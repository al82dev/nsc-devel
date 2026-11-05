#include "cpp-throw-method.h"
namespace Demo {
static Thrower instance;
Thrower* make() noexcept { return &instance; }
int Thrower::raise(int value) { throw value; }
int Thrower::raiseStatic(int value) { throw value; }
}
