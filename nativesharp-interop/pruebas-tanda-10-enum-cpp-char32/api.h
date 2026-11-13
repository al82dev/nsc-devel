enum Mode : char32_t { Low = 0u, High = 0xffffffffu };
struct Carrier { Mode value; int guard; };
int enum_anchor();
