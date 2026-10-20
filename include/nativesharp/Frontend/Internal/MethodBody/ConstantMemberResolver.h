#pragma once

// Adaptador que le da al evaluador compartido la resolucion de miembros y de tipos de ESTE
// modelo, sin que el evaluador tenga que conocer el modelo entero.
class ConstantMemberResolver final : public ProjectConstantMemberResolver
{
public:
    ConstantMemberResolver(
        const ProjectMethodBodyModel& model,
        std::size_t callableOrdinal) noexcept;

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
    const ProjectMethodBodyModel& model_;
    std::size_t callableOrdinal_ = 0;
};
