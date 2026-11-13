#pragma once
enum Mode : unsigned short { Low = 0, High = 65535 };
struct Carrier { Mode value; int guard; };
int enum_anchor();
