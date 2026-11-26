// A second C++ header that also includes lim_vec.hpp (as glm/gtc/matrix_transform.hpp includes glm's types).
#pragma once
#include "lim_vec.hpp"
namespace lim
{
inline float length2(vec3 const& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
}
