// C++ header whose namespace declares non-template operator overloads (as GLM does for some of its types).
#pragma once
namespace lim
{
struct vec { float x; };
inline bool operator==(vec const& a, vec const& b) { return a.x == b.x; }
inline bool operator!=(vec const& a, vec const& b) { return a.x != b.x; }
inline int base() { return 0; }
}
