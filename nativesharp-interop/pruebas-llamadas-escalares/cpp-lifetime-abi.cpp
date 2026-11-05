struct PlainLifetime {
    int value;
    PlainLifetime(int) noexcept;
    ~PlainLifetime() noexcept;
};
PlainLifetime::PlainLifetime(int v) noexcept : value(v) {}
PlainLifetime::~PlainLifetime() noexcept { value = 0; }
void* operator new(unsigned long long, void* place) noexcept { return place; }
extern "C" void make_and_destroy(void* place) {
    auto* instance = new (place) PlainLifetime(7);
    instance->~PlainLifetime();
}
