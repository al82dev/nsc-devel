// A C++ record whose constructor and method take C function pointers (ImGui's ImDrawCallback in
// ImDrawList::AddCallback, the event callback of std::ios_base::register_callback).
#ifndef LIM_CPPFN_H
#define LIM_CPPFN_H
struct LimList;
typedef int (*LimCallback)(const LimList* list, int value);
typedef int (*LimSeed)(int value);
struct LimList
{
    int base;
    explicit LimList(LimSeed seed) : base(seed(10)) {}
    int Apply(LimCallback callback, int value) const { return callback(this, value) + base; }
};
#endif
