#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    T09HfaCpp::Pair a{11.0f, 23.0f};
    auto b = T09HfaCpp::transform(a);
    auto callback = T09HfaCpp::fetch(); auto c = callback(a);
    return !(a.x == 11.0f && a.y == 23.0f && b.x == 14.0f && b.y == 18.0f && c.x == 14.0f && c.y == 18.0f);
}
