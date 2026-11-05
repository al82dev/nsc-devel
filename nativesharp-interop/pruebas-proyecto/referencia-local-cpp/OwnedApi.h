namespace BorrowedDemo {
struct Widget {
    int value;
    Widget(int initial) noexcept;
    ~Widget() noexcept;
    int add(int amount) const noexcept;
};
Widget* make() noexcept;
int destroyed() noexcept;
void releaseObject() noexcept;
}
