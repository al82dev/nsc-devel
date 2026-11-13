#pragma once
enum Mode : int { Chosen = 10, Changed = 11, Negative = -7 };
struct Carrier { Mode field; int padding; };
int enum_anchor();
