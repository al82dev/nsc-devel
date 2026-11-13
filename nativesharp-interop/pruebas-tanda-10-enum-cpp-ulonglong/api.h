#pragma once
enum Mode : unsigned long long { Low = 0, High = 0xffffffffffffffffULL };
struct Carrier { Mode value; int guard; };
int enum_anchor();
