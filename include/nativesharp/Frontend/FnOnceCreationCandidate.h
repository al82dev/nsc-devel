#pragma once
#include "nativesharp/Frontend/FnOnceCandidateStatus.h"
#include "nativesharp/Frontend/FnOnceCandidateVisit.h"
#include <cstddef>
#include <optional>
namespace nativesharp::ast { class AstNode; }
namespace nativesharp::frontend
{
struct FnOnceCreationCandidate final
{
    const ast::AstNode* declarationSyntax = nullptr;
    const ast::AstNode* initializerSyntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::size_t localOrdinal = 0;
    std::optional<std::size_t> parentLambdaOrdinal;
    FnOnceCandidateStatus status = FnOnceCandidateStatus::Unknown;
    FnOnceCandidateVisit visit = FnOnceCandidateVisit::Unknown;
};
}
