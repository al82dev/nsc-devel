#include "nativesharp/AST/AstArena.h"

namespace nativesharp::ast
{
void AstArena::absorb(AstArena&& other)
{
    if (this == &other || other.entries_.empty()) return;
    entries_.reserve(entries_.size() + other.entries_.size());
    entries_.insert(entries_.end(), other.entries_.begin(), other.entries_.end());
    other.entries_.clear();
}

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
