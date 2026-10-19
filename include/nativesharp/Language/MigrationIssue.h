#pragma once

namespace nativesharp::language
{
enum class MigrationIssue
{
    NewlyContextualWord,
    OverloadResolution,
    CopyToMove,
    Nullability,
    ImportLookup,
    NumericNarrowing,
    CleanupOrder
};
}
