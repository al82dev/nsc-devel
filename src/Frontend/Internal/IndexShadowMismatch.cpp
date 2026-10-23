#include "IndexShadowMismatch.h"

#include <cstdio>
#include <cstdlib>

namespace nativesharp::frontend::internal
{
// Prints the function and the lookup key to stderr and stops the process, so a wrong index can
// never produce a silently different compilation.
void reportIndexShadowMismatch(std::string_view function, std::string_view key)
{
    std::fprintf(stderr, "nsc: index shadow check failed in %.*s for key '%.*s'\n",
        static_cast<int>(function.size()), function.data(),
        static_cast<int>(key.size()), key.data());
    std::fflush(stderr);
    std::abort();
}
}
