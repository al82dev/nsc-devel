enum Mode : char8_t { Low = 0, High = 255 };
struct Carrier { Mode value; int guard; };
int enum_anchor();
