#pragma once
enum Mode : short { Low = -32768, High = 32767 };
struct Carrier { Mode value; int guard; };
int enum_anchor();
