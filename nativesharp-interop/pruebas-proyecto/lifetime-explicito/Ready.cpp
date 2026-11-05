#include "Ready.h"
namespace Demo {
static int stage = 0;
Ready::Ready(int value) noexcept { stage = value == 1 ? 1 : 99; }
Ready::~Ready() noexcept { stage = stage == 1 ? 2 : 99; }
int check() noexcept { return stage == 2 ? 67 : 1; }
}
