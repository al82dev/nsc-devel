#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include <cstddef>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
enum class GenericTemplateKind
{
    Class,
    Struct,
    Delegate,
    Method
};

struct GenericTemplateRecord final
{
    GenericTemplateKind kind = GenericTemplateKind::Class;
    const ast::AstNode* syntax = nullptr;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    std::string moduleName;
    std::string namespaceName;
    std::string ownerName;
    std::string name;
    std::vector<std::string> parameters;
    std::string canonicalKey;
};

struct GenericSpecializationRecord final
{
    GenericTemplateKind kind = GenericTemplateKind::Class;
    const ast::AstNode* templateSyntax = nullptr;
    const ast::AstNode* specializedSyntax = nullptr;
    std::string templateKey;
    std::string specializationKey;
    std::string specializedName;
    std::vector<std::string> typeArguments;
    std::size_t cacheOrdinal = 0;
};

class ProjectGenericModel final
{
public:
    bool build(
        const compilation::ProjectGraph& graph,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const GenericTemplateRecord> templates() const noexcept;
    std::span<const GenericSpecializationRecord> specializations() const noexcept;
    bool isBuilt() const noexcept;

private:
    struct TemplateState final
    {
        GenericTemplateRecord record;
        ProjectSyntaxUnit* unit = nullptr;
        ast::AstNode* syntax = nullptr;
        ast::AstNode* parent = nullptr;
    };

    void collectTemplates(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        ast::AstNode* parent,
        const std::string& namespaceName,
        const std::string& ownerName,
        bool insideGenericType,
        diagnostics::DiagnosticEngine& diagnostics);

    void processNode(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& namespaceName,
        const std::string& ownerName,
        bool insideTemplate,
        diagnostics::DiagnosticEngine& diagnostics);

    bool processTypeSyntax(
        ProjectSyntaxUnit& unit,
        ast::TypeSyntax* syntax,
        const std::string& namespaceName,
        diagnostics::DiagnosticEngine& diagnostics);

    bool processGenericCall(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const std::string& namespaceName,
        const std::string& ownerName,
        diagnostics::DiagnosticEngine& diagnostics);

    TemplateState* resolveTypeTemplate(
        const ProjectSyntaxUnit& unit,
        std::string_view namespaceName,
        std::string_view writtenName,
        std::size_t arity);
    // ID 132. `argumentCount` es cuantos argumentos de VALOR trae la llamada. Sirve para elegir
    // entre dos plantillas genericas con el mismo nombre y la misma aridad de TIPOS -- que antes
    // eran el mismo simbolo y daban "duplicate generic template identity".
    TemplateState* resolveMethodTemplate(
        const ProjectSyntaxUnit& unit,
        std::string_view ownerName,
        std::string_view methodName,
        std::size_t arity,
        std::size_t argumentCount);

    std::string ensureTypeSpecialization(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        diagnostics::DiagnosticEngine& diagnostics);
    std::string ensureMethodSpecialization(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        diagnostics::DiagnosticEngine& diagnostics);

    ast::TypeSyntax* cloneType(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions);
    ast::AstNode* cloneNode(
        ast::AstArena& arena,
        const ast::AstNode* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::string_view rewrittenContainingTypeName,
        std::string_view originalContainingTypeName);

    std::string typeSpelling(const ast::TypeSyntax* syntax) const;
    static std::string namespaceAppend(std::string_view prefix, std::string_view suffix);
    static std::string sanitize(std::string_view text);
    static std::string methodCalleeName(const ast::AstNode* call);
    static ast::AstNode* methodCalleeSyntax(ast::AstNode* call);
    void buildVisibility(const compilation::ProjectGraph& graph);
    bool projectVisible(std::size_t from, std::size_t target) const noexcept;

    std::vector<TemplateState> templateStates_;
    std::vector<GenericTemplateRecord> templateRecords_;
    std::vector<GenericSpecializationRecord> specializations_;
    std::unordered_map<std::string, std::size_t> specializationByKey_;
    std::vector<std::vector<std::size_t>> visibleProjects_;
    bool built_ = false;

    // Stage 52 fix: the project ordinal and lexical namespace of whichever call site is
    // CURRENTLY requesting a specialization (set immediately before each
    // ensureTypeSpecialization/ensureMethodSpecialization call, in processTypeSyntax/
    // processGenericCall, where the requesting ProjectSyntaxUnit and namespace are available).
    // cloneType reads these when substituting a generic parameter for a plain (non-builtin,
    // non-generic) type argument, to qualify the substituted name with the REQUESTING
    // namespace and tag it with the REQUESTING project's ordinal (TypeSyntax::
    // originProjectOrdinal) -- see that field's comment for why this is necessary: without it,
    // a type argument that only the requesting project (not the template's own defining
    // project) can see fails to resolve once it is cloned into the template's specialization
    // body. This is call-site state, not persistent instance state -- it is only ever read
    // synchronously within the ensureTypeSpecialization/ensureMethodSpecialization call that
    // immediately follows each assignment, never across a later, unrelated call.
    std::size_t currentInstantiationProjectOrdinal_ = 0;
    std::string currentInstantiationNamespace_;
};

const char* genericTemplateKindName(GenericTemplateKind kind) noexcept;
}
