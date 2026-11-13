#pragma once
enum Mode : unsigned char { Low = 0, High = 255 };
struct Carrier { Mode value; int guard; };
int enum_anchor();
