#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

namespace nativesharp::frontend
{
std::uint8_t ProjectMethodBodyModel::ExpressionAnalysisRequest::optionMask() const noexcept
{
    return static_cast<std::uint8_t>(
        (allowMethodGroup ? 1U : 0U) |
        (allowEvent ? 2U : 0U) |
        (allowMove ? 4U : 0U) |
        (allowConstructorPropertyWrite ? 8U : 0U));
}

std::size_t ProjectMethodBodyModel::ExpressionAnalysisCacheKeyHash::operator()(
    const ExpressionAnalysisCacheKey& key) const noexcept
{
    const std::size_t syntaxHash = std::hash<const ast::AstNode*>{}(key.syntax);
    return syntaxHash ^ (
        static_cast<std::size_t>(key.optionMask) +
        std::size_t{0x9e3779b9U} +
        (syntaxHash << 6U) +
        (syntaxHash >> 2U));
}

ProjectMethodBodyModel::ConstantMemberResolver::ConstantMemberResolver(
    const ProjectMethodBodyModel& model,
    std::size_t callableOrdinal) noexcept
    : model_(model), callableOrdinal_(callableOrdinal)
{
}
}
