#include "api.h"

int main() {
    int value = 7;
    T09ObjectSize::Pair pair;
    T09ObjectSize::ArrayPair array;
    const int constantIndex = 1;
    return T09ObjectSize::size0(&value) != 4 || T09ObjectSize::size1(&value) != 4 ||
           T09ObjectSize::size2(&value) != 4 || T09ObjectSize::size3(&value) != 4 ||
           T09ObjectSize::dynamic0(&value) != 4 || T09ObjectSize::dynamic2(&value) != 4 ||
           T09ObjectSize::dynamic1(&value) != 4 || T09ObjectSize::dynamic3(&value) != 4 ||
           T09ObjectSize::mixed(1, &value, 2, &value, 3) != 14 ||
           T09ObjectSize::size0(T09ObjectSize::next()) != -1 ||
           T09ObjectSize::size2(T09ObjectSize::next()) != 0 ||
           T09ObjectSize::size1(T09ObjectSize::next()) != -1 ||
           T09ObjectSize::size3(T09ObjectSize::next()) != 0 || T09ObjectSize::calls() != 4 ||
           T09ObjectSize::size0(&pair.first) != 8 || T09ObjectSize::size2(&pair.first) != 8 ||
           T09ObjectSize::size1(&pair.first) != 4 || T09ObjectSize::size3(&pair.first) != 4 ||
           T09ObjectSize::dynamic1(&pair.first) != 4 || T09ObjectSize::dynamic3(&pair.first) != 4 ||
           T09ObjectSize::size1(&array.values[0]) != 8 || T09ObjectSize::size1(&array.values[1]) != 4 ||
           T09ObjectSize::size3(&array.values[0]) != 8 || T09ObjectSize::size3(&array.values[1]) != 4 ||
           T09ObjectSize::dynamic1(&array.values[1]) != 4 || T09ObjectSize::dynamic3(&array.values[1]) != 4 ||
           T09ObjectSize::size1(&array.values[constantIndex]) != 4 || T09ObjectSize::size3(&array.values[constantIndex]) != 4;
}
