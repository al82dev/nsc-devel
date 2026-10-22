#pragma once

namespace nativesharp::frontend
{
enum class TemplateParseCacheLookupReason
{
    Hit,
    Disabled,
    InvalidKey,
    Cold,
    IncompletePublication,
    Corruption,
    Unavailable,
};

const char* templateParseCacheLookupReasonName(
    TemplateParseCacheLookupReason reason) noexcept;
} // namespace nativesharp::frontend
