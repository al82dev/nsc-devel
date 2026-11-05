#include "CppClassApi.h"
namespace Demo {
static Widget instance{3, 10};
int Widget::shared = 7;
int Widget::add(int amount) const { return value + amount; }
int Widget::scale(int amount) { return amount * 2; }
Widget* make() noexcept { return &instance; }
}
