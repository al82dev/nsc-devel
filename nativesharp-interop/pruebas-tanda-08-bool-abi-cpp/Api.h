namespace Demo {
struct Switch {
    int marker;
    bool flip(bool value) const;
    static bool choose(int prefix, bool value, int suffix);
};
Switch* instance();
bool invert(bool value);
}
