constexpr int select(int*) { return 1; }
constexpr int select(const int*) { return 2; }
constexpr bool verify_selection()
{
    int value = 0;
    const int* readonly = &value;
    return select(&value) == 1 && select(readonly) == 2;
}
static_assert(verify_selection());
template <class T> concept Selectable = requires(T value) { select(value); };
static_assert(!Selectable<decltype(nullptr)>);
