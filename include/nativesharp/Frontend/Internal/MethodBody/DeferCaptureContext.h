#pragma once

// T06-F05. The parser lowers defer to try/finally, but the source capture boundary remains
// normative: a deferred body may only reach outer locals/parameters named in its explicit
// capture list. Generated aliases live immediately outside the marked body scope.
struct DeferCaptureContext final
{
    std::size_t boundaryScopeOrdinal = 0;
    std::unordered_set<std::string> allowedNames;
    bool capturesThis = false;
};
