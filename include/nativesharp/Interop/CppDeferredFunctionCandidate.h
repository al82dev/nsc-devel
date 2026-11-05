#pragma once

#include "nativesharp/Interop/CppFunctionDeclaration.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::ast { class AstNode; }

namespace nativesharp::interop
{
struct CppConflictingFunctionOrigin
{
    std::size_t declarationProject = 0;
    std::size_t referenceIndex = 0;
    std::filesystem::path declarationProjectFile;
    CppFunctionDeclaration declaration;
};

// The declaration is owned; syntax remains valid only while its compilation is alive.
struct CppDeferredFunctionCandidate
{
    const ast::AstNode* syntax = nullptr;
    bool directCall = false;
    std::size_t consumerProject = 0;
    std::size_t declarationProject = 0;
    std::size_t referenceIndex = 0;
    std::filesystem::path declarationProjectFile;
    CppFunctionDeclaration declaration;
    std::string reason;
    std::vector<CppConflictingFunctionOrigin> conflictingOrigins;
};
}
