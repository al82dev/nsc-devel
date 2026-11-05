#include "OwnedApi.h"
void* operator new(decltype(sizeof(0)), void* address) noexcept { return address; }
namespace BorrowedDemo {
static int destructionCount = 0;
Widget::Widget(int initial) noexcept : value(initial) {}
Widget::~Widget() noexcept { ++destructionCount; }
int Widget::add(int amount) const noexcept { return value + amount; }
alignas(Widget) static unsigned char storage[sizeof(Widget)];
static bool initialized = false;
Widget* make() noexcept {
    if (!initialized) { new (storage) Widget(10); initialized = true; }
    return reinterpret_cast<Widget*>(storage);
}
int destroyed() noexcept { return destructionCount; }
void releaseObject() noexcept {
    if (initialized) { reinterpret_cast<Widget*>(storage)->~Widget(); initialized = false; }
}
}
