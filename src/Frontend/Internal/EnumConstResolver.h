#pragma once

#include "nativesharp/Frontend/ProjectConstantMemberResolver.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend::internal
{
class EnumConstResolver final : public ProjectConstantMemberResolver
{
public:
    EnumConstResolver(std::string enumName, std::string underlyingType);

    void add(std::string name, std::uint64_t bits);

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

    std::string resolveTypeKey(const ast::TypeSyntax* syntax) const override;

private:
    std::string enumName_;
    std::string underlyingType_;
    std::unordered_map<std::string, std::size_t> names_;
    std::vector<std::uint64_t> values_;
};
}
