#include "Api.h"
namespace Demo {
static Switch object = {7};
Switch* instance() { return &object; }
bool Switch::flip(bool value) const { return marker == 7 && !value; }
bool Switch::choose(int prefix, bool value, int suffix) { return prefix == 3 && value && suffix == 7; }
bool invert(bool value) { return !value; }
}
