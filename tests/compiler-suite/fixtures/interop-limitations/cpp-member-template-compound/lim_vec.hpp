// GLM-like C++ vector: compound operators are member function templates (glm/detail/type_vec3.hpp).
#pragma once
namespace lim
{
template <typename T> struct tvec3
{
    T x, y, z;
    tvec3() = default;
    tvec3(T a, T b, T c) : x(a), y(b), z(c) {}
    template <typename U> tvec3& operator+=(U scalar) { x += T(scalar); y += T(scalar); z += T(scalar); return *this; }
    template <typename U> tvec3& operator+=(tvec3<U> const& v) { x += T(v.x); y += T(v.y); z += T(v.z); return *this; }
    template <typename U> tvec3& operator-=(tvec3<U> const& v) { x -= T(v.x); y -= T(v.y); z -= T(v.z); return *this; }
};
template <typename T> tvec3<T> operator*(tvec3<T> const& v, T s) { return tvec3<T>(v.x * s, v.y * s, v.z * s); }
typedef tvec3<float> vec3;
}
