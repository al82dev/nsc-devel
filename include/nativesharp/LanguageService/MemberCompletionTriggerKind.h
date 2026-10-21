#pragma once

namespace nativesharp::language_service
{
enum class MemberCompletionTriggerKind
{
    Dot,
    NullConditionalDot,
    Explicit
};
}
