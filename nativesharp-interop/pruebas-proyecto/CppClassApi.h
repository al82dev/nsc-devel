namespace Demo {
struct Widget {
    int prefix;
    int value;
    int add(int amount) const;
    static int scale(int amount);
    static int shared;
};
Widget* make() noexcept;
}
