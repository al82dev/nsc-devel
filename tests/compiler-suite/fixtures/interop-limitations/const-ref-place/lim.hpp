#pragma once
struct Pair { int a; int b; };
inline Pair makePair(int a, int b) { return Pair{a, b}; }
inline int sum(const Pair& p) { return p.a + p.b; }
