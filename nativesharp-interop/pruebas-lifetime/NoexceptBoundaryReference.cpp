using Destructor = void (*)(void*);
extern "C" void cpp_unwind_destructor_boundary(Destructor destructor, void* object) noexcept {
    destructor(object);
}
