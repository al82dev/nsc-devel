#pragma once
enum Mode : signed char { Low = -128, High = 127 };
struct Carrier { Mode value; int guard; };
int enum_anchor();
