#include "Lifetime.h"
inline void* operator new(__SIZE_TYPE__, void* address) noexcept { return address; }
extern "C" __declspec(noinline) Parent* create_parent(void* storage) {
    return new (storage) Parent;
}
extern "C" __declspec(noinline) void destroy_parent(Parent* object) {
    object->~Parent();
}
