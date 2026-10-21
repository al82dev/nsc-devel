#pragma once

namespace nativesharp::language_service
{
enum class MemberCompletionEntryKind
{
    Field,
    Property,
    Method,
    Event,
    EnumMember,
    Constant,
    Type,
    Namespace,
    Function,
    Variable
};
}
