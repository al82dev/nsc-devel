#pragma once
enum Mode : wchar_t { Low = 0, High = 65535 };
struct Carrier { Mode value; int guard; };
int enum_anchor();
