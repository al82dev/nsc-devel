#pragma once

namespace nativesharp::language_service
{
enum class MemberCompletionStatus
{
    Complete,
    Incomplete,
    Cancelled,
    StaleDocument,
    Unavailable,
    InvalidPosition
};
}
