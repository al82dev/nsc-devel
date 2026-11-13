#pragma once
enum UMode : unsigned int { UZero = 0, UHigh = 0xffffffffu };
enum SignedMode : int { SignedValue = 0 };
struct UCarrier { UMode value; int guard; };
int enum_anchor();
