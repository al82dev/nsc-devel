#include "api.h"

int main() {
    int value = 7;
    T09ObjectSize::Pair pair;
    return T09ObjectSize::size0(&value) != 4 || T09ObjectSize::size1(&value) != 4 ||
           T09ObjectSize::size2(&value) != 4 || T09ObjectSize::size3(&value) != 4 ||
           T09ObjectSize::dynamic0(&value) != 4 || T09ObjectSize::dynamic2(&value) != 4 ||
           T09ObjectSize::mixed(1, &value, 2, &value, 3) != 14 ||
           T09ObjectSize::size0(T09ObjectSize::next()) != -1 ||
           T09ObjectSize::size2(T09ObjectSize::next()) != 0 || T09ObjectSize::calls() != 2 ||
           T09ObjectSize::size0(&pair.first) != 8 || T09ObjectSize::size2(&pair.first) != 8;
}
