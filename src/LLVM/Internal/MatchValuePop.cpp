#include "MatchValuePop.h"

namespace nativesharp::llvm_backend
{
MatchValuePop::~MatchValuePop()
{
    stack.pop_back();
}
}
