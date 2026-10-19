#pragma once

#include "nativesharp/Frontend/ClassMemberKind.h"

#include "nativesharp/Language/OwnershipSemantics.h"

#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct PrecompiledMemberIdentity final
{
    ClassMemberKind kind = ClassMemberKind::Field;
    std::string declaringTypeKey;
    std::string name;
    std::string signatureKey;
    std::string mangledName;
    std::vector<language::EscapeClass> parameterEscapeSummary;
};
}
