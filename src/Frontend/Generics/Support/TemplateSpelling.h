#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"

namespace nativesharp::frontend::internal
{
std::string qualifiedName(std::string_view ns, std::string_view name);
std::string appendOwner(std::string_view chain, std::string_view name);
std::string receiverSourceSpelling(const ast::AstNode* receiver);
std::string templateSourceSpelling(
    std::string_view namespaceName,
    std::string_view ownerChain);
std::vector<std::string> splitDotted(std::string_view text);
std::vector<std::string> enclosingTypeSpellings(
    std::string_view namespaceName,
    std::string_view ownerChain);
std::string stableHashSuffix(std::string_view text);
}
