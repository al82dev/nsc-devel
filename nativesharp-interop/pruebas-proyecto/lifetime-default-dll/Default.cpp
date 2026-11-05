#include "Default.h"
namespace Demo {
static int stage = 0;
Default::Default() noexcept { stage = 1; }
Default::~Default() noexcept { stage = stage == 1 ? 2 : 99; }
int check() noexcept { return stage == 2 ? 67 : 1; }
}
