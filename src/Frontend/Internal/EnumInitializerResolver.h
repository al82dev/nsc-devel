#pragma once

#include "nativesharp/Frontend/ProjectConstantMemberResolver.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace nativesharp::frontend::internal
{
class EnumInitializerResolver final : public ProjectConstantMemberResolver
{
public:
    EnumInitializerResolver(
        const ProjectMemberModel& members,
        const ProjectNameLookup& names) noexcept;

    std::optional<std::size_t> resolveConstantMember(
        const ast::AstNode* syntax,
        std::optional<std::size_t> declaringTypeOrdinal) const override;

    bool describeConstantMember(
        std::size_t memberOrdinal,
        bool& isEnumConstant,
        std::int64_t& enumConstantValue,
        std::string& enumUnderlyingTypeKey,
        const ast::AstNode*& initializer,
        std::string& declaredTypeKey,
        std::optional<std::size_t>& declaringTypeOrdinal) const override;

    std::string resolveTypeKey(const ast::TypeSyntax* typeSyntax) const override;

private:
    const ProjectMemberModel& members_;
    const ProjectNameLookup& names_;
};
}
