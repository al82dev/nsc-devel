#pragma once
enum Mode : long long { Low = -1099511627776LL, High = 1099511627783LL };
struct Carrier { Mode value; int guard; };
int enum_anchor();
