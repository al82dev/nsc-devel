#pragma once

namespace nativesharp::language
{
enum class TaskScopeExitReason { Normal, EarlyError, Cancellation, Panic };
}
