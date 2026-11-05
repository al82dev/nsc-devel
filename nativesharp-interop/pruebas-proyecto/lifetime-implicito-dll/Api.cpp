#include "Api.h"
namespace Demo {
static int stage = 0;
Member::Member() noexcept { stage = stage == 0 ? 1 : 99; }
Member::~Member() noexcept { stage = stage == 1 ? 2 : 99; }
int check() noexcept { return stage == 2 ? 73 : 1; }
}
