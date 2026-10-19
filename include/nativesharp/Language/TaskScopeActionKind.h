#pragma once

namespace nativesharp::language
{
enum class TaskScopeActionKind { Cancel, Join, PropagateError, PropagateCancellation, PropagatePanic };
}
