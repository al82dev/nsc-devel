#pragma once

struct ScopeName final
{
    ScopeNameKind kind = ScopeNameKind::Local;
    std::string name;
    std::size_t semanticOrdinal = 0;
    SourceRange range;
};
