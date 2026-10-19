#pragma once

namespace nativesharp::language
{
enum class DebugEntityKind
{
    Local,
    InlineSite,
    ClosureCapture,
    VariantPayload,
    OwnershipState,
    ExceptionValue,
    CoroutineState,
    Temporary
};
}
