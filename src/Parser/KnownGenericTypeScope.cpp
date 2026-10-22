#include "KnownGenericTypeScope.h"

namespace nativesharp::parser
{
KnownGenericTypeScope::KnownGenericTypeScope(
    KnownTypeNameSet& knownTypeNames,
    const ast::AstNode* declaration)
    : knownTypeNames_(knownTypeNames)
{
    if (declaration == nullptr) return;
    for (const ast::GenericParameterSyntax& parameter :
         declaration->genericParameterSyntaxes())
    {
        if (parameter.kind != ast::GenericParameterSyntaxKind::Type &&
            parameter.kind != ast::GenericParameterSyntaxKind::TypePack)
        {
            continue;
        }
        if (knownTypeNames_.insert(parameter.name).second)
        {
            inserted_.push_back(parameter.name);
        }
    }
}

KnownGenericTypeScope::~KnownGenericTypeScope()
{
    for (const std::string& name : inserted_) knownTypeNames_.erase(name);
}
}
