#pragma once

namespace nativesharp::language
{
enum class CancellationReason { Requested, FutureDropped, ParentScopeExited };
}
