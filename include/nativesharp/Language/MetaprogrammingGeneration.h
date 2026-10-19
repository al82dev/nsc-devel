#pragma once

#include "nativesharp/Language/GeneratedDeclaration.h"
#include "nativesharp/Language/GeneratedDeclarationKind.h"
#include "nativesharp/Language/MetaprogrammingAstBuilder.h"
#include "nativesharp/Language/MetaprogrammingSemantics.h"

#include <string>
#include <string_view>

namespace nativesharp::language
{
// NS304-T14-F05: the unified identity of a generated member — the same fully-qualified form as a
// source member's identity.key(), so a generated member participates in the partial merge, the
// extension lookup, and associated items by the identical identity (deterministic, so cycle
// resolution compares the same stable string).
std::string generatedMemberIdentity(
    std::string_view enclosingTypeIdentity,
    std::string_view memberName);
bool sameGeneratedIdentity(std::string_view left, std::string_view right) noexcept;
}
