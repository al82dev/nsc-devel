struct ThrowingMember {
    ThrowingMember() noexcept(false);
    ~ThrowingMember() noexcept(false);
};
struct ImplicitThrowing { ThrowingMember member; };
static int constructed = 0;
static int destroyed = 0;
ThrowingMember::ThrowingMember() noexcept(false) { ++constructed; }
ThrowingMember::~ThrowingMember() noexcept(false) { ++destroyed; }
extern "C" void* invoke_ctor(void*) __asm__("??0ImplicitThrowing@@QEAA@XZ");
extern "C" void invoke_dtor(void*) __asm__("??1ImplicitThrowing@@QEAA@XZ");
extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" void mainCRTStartup() {
    alignas(ImplicitThrowing) unsigned char storage[sizeof(ImplicitThrowing)];
    void* object = invoke_ctor(storage);
    if (object != storage || constructed != 1 || destroyed != 0) ExitProcess(1);
    invoke_dtor(storage);
    ExitProcess(destroyed == 1 ? 0 : 2);
}
