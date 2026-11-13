#include "api.h"
int main() {
    First::Mode a = static_cast<First::Mode>(10);
    Second::Mode b = static_cast<Second::Mode>(20);
    First::Mode c = First::Value;
    Second::Mode d = Second::Value;
    First::Box x; x.field = First::Value; x.padding = 1;
    Second::Box y; y.field = Second::Value; y.padding = 2;
    return static_cast<int>(a) == 10 && static_cast<int>(b) == 20 &&
        static_cast<int>(c) == 10 && static_cast<int>(d) == 20 &&
        static_cast<int>(x.field) == 10 && static_cast<int>(y.field) == 20 &&
        x.padding == 1 && y.padding == 2 ? 0 : 1;
}
