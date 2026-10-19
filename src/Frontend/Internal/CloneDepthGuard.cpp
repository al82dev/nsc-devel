#include "CloneDepthGuard.h"

#include <utility>

namespace nativesharp::frontend::internal
{
CloneDepthGuard::CloneDepthGuard(std::size_t& value) : depth(value) { ++depth; }

CloneDepthGuard::~CloneDepthGuard() { --depth; }
}
