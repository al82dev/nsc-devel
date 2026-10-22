#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Parser/KnownTypeNameSet.h"

#include <string>
#include <vector>

namespace nativesharp::parser
{
// Generic parameter names are types only inside the declaration that introduces them.  The
// statement lookahead intentionally consults knownTypeNames_ so it can distinguish `T value`
// from an expression; keeping T globally would make an unrelated declaration in a later type
// parse differently.  This guard installs only type parameters (never const parameters) and
// removes exactly the names it inserted, preserving an equally named outer generic parameter.
class KnownGenericTypeScope final
{
public:
    KnownGenericTypeScope(
        KnownTypeNameSet& knownTypeNames,
        const ast::AstNode* declaration);

    ~KnownGenericTypeScope();

    KnownGenericTypeScope(const KnownGenericTypeScope&) = delete;
    KnownGenericTypeScope& operator=(const KnownGenericTypeScope&) = delete;

private:
    KnownTypeNameSet& knownTypeNames_;
    std::vector<std::string> inserted_;
};
}
