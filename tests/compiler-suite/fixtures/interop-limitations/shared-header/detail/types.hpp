#pragma once
template <typename T> struct vec2
{
    union { T x; T r; };
    T y;
    vec2() = default;
    vec2(T x, T y);
};
template <typename T> vec2<T>::vec2(T _x, T _y) : x(_x), y(_y) {}
