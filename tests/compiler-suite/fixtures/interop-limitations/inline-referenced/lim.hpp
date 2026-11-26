#pragma once
template <typename T> struct Box
{
    T v;
    T get() const { return v; }
    T twice() const { return v + v; }
    T thrice() const { return v + v + v; }
};
inline int unusedOne() { return Box<long long>{1}.thrice() > 0 ? 1 : 0; }
inline double unusedTwo(double x) { return Box<double>{x}.get(); }
inline int answer() { Box<int> b{21}; return b.twice(); }
