#include "Lifetime.h"
extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" Parent* create_parent(void*);
extern "C" void destroy_parent(Parent*);
int destroyed = 0;
extern "C" void mainCRTStartup() {
    alignas(Parent) unsigned char storage[sizeof(Parent)];
    Parent* object = create_parent(storage);
    if (object->child.value != 17 || destroyed != 0) ExitProcess(1);
    destroy_parent(object);
    ExitProcess(destroyed == 1 ? 0 : 2);
}
