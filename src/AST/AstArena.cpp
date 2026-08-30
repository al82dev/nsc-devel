#include "nativesharp/AST/AstArena.h"

namespace nativesharp::ast
{
AstArena::~AstArena()
{
    for (auto iterator = entries_.rbegin(); iterator != entries_.rend(); ++iterator)
    {
        iterator->deleter(iterator->pointer);
    }
}
}
