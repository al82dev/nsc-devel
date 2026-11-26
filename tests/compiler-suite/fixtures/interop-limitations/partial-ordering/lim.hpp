#pragma once
template <typename T> struct Wrap { T v; };
typedef Wrap<int> IntWrap;
inline IntWrap makeWrap() { return IntWrap{7}; }
template <typename T> int pick(const T&) { return 1; }
template <typename T> int pick(const Wrap<T>&) { return 42; }
