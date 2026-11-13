#include "api.h"
int main() {
    Space::Mode a = static_cast<Space::Mode>(10);
    Space::Mode b = a;
    Space::Mode c = Space::Mode::Value;
    Space::Mode d = Space::Mode::Negative;
    Space::Other e = Space::Other::Value;
    Space::Wide w = Space::Wide::Max;
    return static_cast<int>(b) == 10 && static_cast<int>(c) == 10 &&
        static_cast<int>(d) == -7 && static_cast<int>(e) == 20 &&
        static_cast<unsigned long long>(w) == 0xffffffffffffffffULL ? 0 : 1;
}
