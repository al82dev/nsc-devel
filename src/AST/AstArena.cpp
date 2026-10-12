#include "nativesharp/AST/AstArena.h"

namespace nativesharp::ast
{
AstArena::~AstArena() noexcept
{
    for (auto iterator = entries_.rbegin(); iterator != entries_.rend(); ++iterator)
    {
        try
        {
            iterator->deleter(iterator->pointer);
        }
        catch (...)
        {
            // Arena teardown is best-effort: one invalid throwing destructor
            // must not prevent the remaining owned objects from being released.
        }
    }
}
}
