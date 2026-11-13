#include "api.h"
int main() {
    First::Mode a = static_cast<First::Mode>(10);
    Second::Mode b = static_cast<Second::Mode>(20);
    First::Mode c = First::Value;
    Second::Mode d = Second::Value;
    return static_cast<int>(a) == 10 && static_cast<int>(b) == 20 &&
        static_cast<int>(c) == 10 && static_cast<int>(d) == 20 ? 0 : 1;
}
