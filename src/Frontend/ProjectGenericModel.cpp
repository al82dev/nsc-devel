#include "nativesharp/Frontend/ProjectGenericModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
bool isGenericTypeTemplateNode(const ast::AstNode* node) noexcept
{
    if (node == nullptr || node->genericParameters().empty()) return false;
    return node->kind() == ast::AstNodeKind::ClassDeclaration ||
        node->kind() == ast::AstNodeKind::StructDeclaration ||
        node->kind() == ast::AstNodeKind::DelegateDeclaration;
}

GenericTemplateKind genericTypeTemplateKind(const ast::AstNode* node) noexcept
{
    if (node->kind() == ast::AstNodeKind::ClassDeclaration) return GenericTemplateKind::Class;
    if (node->kind() == ast::AstNodeKind::DelegateDeclaration) return GenericTemplateKind::Delegate;
    return GenericTemplateKind::Struct;
}

bool isGenericMethodTemplateNode(const ast::AstNode* node) noexcept
{
    return node != nullptr && node->kind() == ast::AstNodeKind::MethodDeclaration &&
        !node->genericParameters().empty();
}

std::string qualifiedName(std::string_view ns, std::string_view name)
{
    if (ns.empty()) return std::string(name);
    return std::string(ns) + "." + std::string(name);
}

std::string stableHashSuffix(std::string_view text)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const char raw : text)
    {
        const unsigned char ch = static_cast<unsigned char>(raw);
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << hash;
    return stream.str();
}
}

const char* genericTemplateKindName(GenericTemplateKind kind) noexcept
{
    switch (kind)
    {
        case GenericTemplateKind::Class: return "class";
        case GenericTemplateKind::Struct: return "struct";
        case GenericTemplateKind::Delegate: return "delegate";
        case GenericTemplateKind::Method: return "method";
    }
    return "generic";
}

bool ProjectGenericModel::build(
    const compilation::ProjectGraph& graph,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_) return !diagnostics.hasErrors();
    buildVisibility(graph);

    for (const std::unique_ptr<ProjectSyntaxUnit>& unitPtr : units)
    {
        if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
        ProjectSyntaxUnit& unit = *unitPtr;
        collectTemplates(unit, unit.syntaxTree(), nullptr, "", "", false, diagnostics);
    }

    std::unordered_set<std::string> templateKeys;
    for (const TemplateState& state : templateStates_)
    {
        if (!templateKeys.insert(state.record.canonicalKey).second)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                state.syntax != nullptr ? state.syntax->range() : SourceRange{},
                "duplicate generic template identity '" + state.record.canonicalKey + "'");
        }
    }

    templateRecords_.reserve(templateStates_.size());
    for (const TemplateState& state : templateStates_) templateRecords_.push_back(state.record);

    for (const std::unique_ptr<ProjectSyntaxUnit>& unitPtr : units)
    {
        if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
        ProjectSyntaxUnit& unit = *unitPtr;
        processNode(unit, unit.syntaxTree(), "", "", false, diagnostics);
    }

    built_ = true;
    return !diagnostics.hasErrors();
}

std::span<const GenericTemplateRecord> ProjectGenericModel::templates() const noexcept
{
    return templateRecords_;
}

std::span<const GenericSpecializationRecord> ProjectGenericModel::specializations() const noexcept
{
    return specializations_;
}

bool ProjectGenericModel::isBuilt() const noexcept
{
    return built_;
}

void ProjectGenericModel::collectTemplates(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    ast::AstNode* parent,
    const std::string& namespaceName,
    const std::string& ownerName,
    bool insideGenericType,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (node == nullptr) return;

    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        currentNamespace = namespaceAppend(namespaceName, node->text());
    }

    const bool genericType = isGenericTypeTemplateNode(node);
    if (genericType)
    {
        if (node->flag())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                node->range(),
                "partial generic types are deferred beyond Stage 44; a monomorphized template must have one canonical body");
        }
        GenericTemplateRecord record;
        record.kind = genericTypeTemplateKind(node);
        record.syntax = node;
        record.projectOrdinal = unit.projectOrdinal();
        record.sourceOrdinal = unit.sourceOrdinal();
        record.moduleName = std::string(unit.moduleName());
        record.namespaceName = currentNamespace;
        record.name = node->text();
        record.parameters = node->genericParameters();
        record.canonicalKey = record.moduleName + "::" + qualifiedName(currentNamespace, record.name) +
            "`" + std::to_string(record.parameters.size());
        templateStates_.push_back(TemplateState{record, &unit, node, parent});
    }

    if (isGenericMethodTemplateNode(node))
    {
        if (insideGenericType)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                node->range(),
                "generic methods declared inside generic types are deliberately deferred beyond Stage 44; specialize one generic dimension at a time");
        }
        GenericTemplateRecord record;
        record.kind = GenericTemplateKind::Method;
        record.syntax = node;
        record.projectOrdinal = unit.projectOrdinal();
        record.sourceOrdinal = unit.sourceOrdinal();
        record.moduleName = std::string(unit.moduleName());
        record.namespaceName = currentNamespace;
        record.ownerName = ownerName;
        record.name = node->text();
        record.parameters = node->genericParameters();
        record.canonicalKey = record.moduleName + "::" + qualifiedName(currentNamespace, ownerName) +
            "." + record.name + "``" + std::to_string(record.parameters.size());
        templateStates_.push_back(TemplateState{record, &unit, node, parent});
    }

    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration || node->kind() == ast::AstNodeKind::StructDeclaration)
            ? node->text() : ownerName;
    const bool childInsideGeneric = insideGenericType || genericType;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    for (ast::AstNode* child : children)
    {
        collectTemplates(unit, child, node, currentNamespace, childOwner, childInsideGeneric, diagnostics);
    }
}

void ProjectGenericModel::processNode(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& namespaceName,
    const std::string& ownerName,
    bool insideTemplate,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (node == nullptr) return;

    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        currentNamespace = namespaceAppend(namespaceName, node->text());
    }

    const bool templateNode = isGenericTypeTemplateNode(node) || isGenericMethodTemplateNode(node);
    if (templateNode || insideTemplate) return;

    if (node->typeSyntax() != nullptr)
    {
        processTypeSyntax(unit, node->typeSyntax(), currentNamespace, diagnostics);
    }

    if (node->kind() == ast::AstNodeKind::CallExpression && !node->typeSyntaxes().empty())
    {
        processGenericCall(unit, node, currentNamespace, ownerName, diagnostics);
    }
    else
    {
        for (ast::TypeSyntax* syntax : node->typeSyntaxes())
        {
            processTypeSyntax(unit, syntax, currentNamespace, diagnostics);
        }
    }

    const auto processChild = [&](ast::AstNode* child)
    {
        processNode(unit, child, currentNamespace, ownerName, false, diagnostics);
    };
    processChild(node->first());
    processChild(node->second());
    processChild(node->third());

    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration || node->kind() == ast::AstNodeKind::StructDeclaration)
            ? node->text() : ownerName;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    for (ast::AstNode* child : children)
    {
        processNode(unit, child, currentNamespace, childOwner, false, diagnostics);
    }
}

bool ProjectGenericModel::processTypeSyntax(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    const std::string& namespaceName,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (syntax == nullptr) return true;
    for (ast::TypeSyntax* argument : syntax->typeArguments())
    {
        if (!processTypeSyntax(unit, argument, namespaceName, diagnostics)) return false;
    }

    TemplateState* state = resolveTypeTemplate(
        unit, namespaceName, syntax->baseName(), syntax->typeArguments().size());
    if (state == nullptr)
    {
        state = resolveTypeTemplate(
            unit, namespaceName, syntax->baseName(), std::numeric_limits<std::size_t>::max());
    }
    if (state == nullptr)
    {
        if (!syntax->typeArguments().empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                syntax->range(),
                "type '" + syntax->baseName() + "' is not a visible generic class/struct template in Stage 44");
            return false;
        }
        return true;
    }

    if (syntax->typeArguments().size() != state->record.parameters.size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "generic type '" + state->record.name + "' expects " +
                std::to_string(state->record.parameters.size()) + " type argument(s) but received " +
                std::to_string(syntax->typeArguments().size()));
        return false;
    }

    currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
    currentInstantiationNamespace_ = namespaceName;
    const std::string specialized = ensureTypeSpecialization(
        *state, syntax->typeArguments(), diagnostics);
    if (specialized.empty()) return false;
    syntax->setBaseName(specialized);
    syntax->clearTypeArguments();
    return true;
}

bool ProjectGenericModel::processGenericCall(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (call == nullptr || call->kind() != ast::AstNodeKind::CallExpression) return false;
    for (ast::TypeSyntax* argument : call->typeSyntaxes())
    {
        if (!processTypeSyntax(unit, argument, namespaceName, diagnostics)) return false;
    }

    ast::AstNode* callee = methodCalleeSyntax(call);
    const std::string methodName = methodCalleeName(call);
    std::string requestedOwner = ownerName;
    if (callee != nullptr && callee->kind() == ast::AstNodeKind::MemberExpression &&
        callee->first() != nullptr && callee->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        requestedOwner = callee->first()->text();
    }

    TemplateState* state = resolveMethodTemplate(
        unit, requestedOwner, methodName, call->typeSyntaxes().size());
    if (state == nullptr)
        state = resolveMethodTemplate(
            unit, requestedOwner, methodName, std::numeric_limits<std::size_t>::max());
    if (state == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            "generic call '" + methodName + "<...>' does not name a unique visible Stage 44 generic method template");
        return false;
    }
    if (call->typeSyntaxes().size() != state->record.parameters.size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            "generic method '" + methodName + "' expects " +
                std::to_string(state->record.parameters.size()) + " type argument(s) but received " +
                std::to_string(call->typeSyntaxes().size()));
        return false;
    }

    currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
    currentInstantiationNamespace_ = namespaceName;
    const std::string specialized = ensureMethodSpecialization(
        *state, call->typeSyntaxes(), diagnostics);
    if (specialized.empty()) return false;
    if (callee != nullptr) callee->setText(specialized);
    call->clearTypeSyntaxes();
    return true;
}

ProjectGenericModel::TemplateState* ProjectGenericModel::resolveTypeTemplate(
    const ProjectSyntaxUnit& unit,
    std::string_view namespaceName,
    std::string_view writtenName,
    std::size_t arity)
{
    TemplateState* fallback = nullptr;
    const std::string qualified = qualifiedName(namespaceName, writtenName);
    for (TemplateState& state : templateStates_)
    {
        if (state.record.kind == GenericTemplateKind::Method || !projectVisible(unit.projectOrdinal(), state.record.projectOrdinal)) continue;
        if (arity != std::numeric_limits<std::size_t>::max() && state.record.parameters.size() != arity) continue;
        const std::string candidate = qualifiedName(state.record.namespaceName, state.record.name);
        if (writtenName.find('.') != std::string_view::npos)
        {
            if (candidate == writtenName) return &state;
            continue;
        }
        if (candidate == qualified) return &state;
        if (state.record.name == writtenName)
        {
            if (fallback != nullptr) return nullptr;
            fallback = &state;
        }
    }
    return fallback;
}

ProjectGenericModel::TemplateState* ProjectGenericModel::resolveMethodTemplate(
    const ProjectSyntaxUnit& unit,
    std::string_view ownerName,
    std::string_view methodName,
    std::size_t arity)
{
    TemplateState* result = nullptr;
    for (TemplateState& state : templateStates_)
    {
        if (state.record.kind != GenericTemplateKind::Method || !projectVisible(unit.projectOrdinal(), state.record.projectOrdinal)) continue;
        if (arity != std::numeric_limits<std::size_t>::max() && state.record.parameters.size() != arity) continue;
        if (state.record.name != methodName) continue;
        if (!ownerName.empty() && state.record.ownerName != ownerName) continue;
        if (result != nullptr) return nullptr;
        result = &state;
    }
    if (result != nullptr) return result;

    // Unqualified fallback is allowed only when exactly one template of that name exists.
    for (TemplateState& state : templateStates_)
    {
        if (state.record.kind != GenericTemplateKind::Method || !projectVisible(unit.projectOrdinal(), state.record.projectOrdinal) ||
            (arity != std::numeric_limits<std::size_t>::max() && state.record.parameters.size() != arity) ||
            state.record.name != methodName) continue;
        if (result != nullptr) return nullptr;
        result = &state;
    }
    return result;
}

std::string ProjectGenericModel::ensureTypeSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<std::string> spellings;
    spellings.reserve(arguments.size());
    for (const ast::TypeSyntax* argument : arguments) spellings.push_back(typeSpelling(argument));

    std::ostringstream keyBuilder;
    keyBuilder << state.record.canonicalKey << '<';
    for (std::size_t i = 0; i < spellings.size(); ++i)
    {
        if (i != 0) keyBuilder << ',';
        keyBuilder << spellings[i];
    }
    keyBuilder << '>';
    const std::string key = keyBuilder.str();
    if (const auto found = specializationByKey_.find(key); found != specializationByKey_.end())
    {
        return specializations_[found->second].specializedName;
    }

    std::string specializedName = state.record.name + "__g" + std::to_string(arguments.size());
    for (const std::string& spelling : spellings) specializedName += "__" + sanitize(spelling);
    specializedName += "__h" + stableHashSuffix(key);

    const std::size_t ordinal = specializations_.size();
    specializationByKey_.emplace(key, ordinal);
    specializations_.push_back(GenericSpecializationRecord{
        state.record.kind,
        state.syntax,
        nullptr,
        state.record.canonicalKey,
        key,
        specializedName,
        spellings,
        ordinal});

    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    for (std::size_t index = 0; index < state.record.parameters.size(); ++index)
        substitutions.emplace(state.record.parameters[index], arguments[index]);

    if (state.unit == nullptr || state.parent == nullptr || state.syntax == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            state.syntax != nullptr ? state.syntax->range() : SourceRange{},
            "Stage 44 generic type template has no materialization parent");
        return {};
    }

    ast::AstNode* clone = cloneNode(
        state.unit->arena(), state.syntax, substitutions, specializedName, state.record.name);
    clone->setText(specializedName);
    clone->clearGenericParameters();
    clone->setAuxiliaryText("stage44-specialization:" + key);
    state.parent->addChild(clone);
    specializations_[ordinal].specializedSyntax = clone;

    processNode(*state.unit, clone, state.record.namespaceName, specializedName, false, diagnostics);
    return specializedName;
}

std::string ProjectGenericModel::ensureMethodSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<std::string> spellings;
    spellings.reserve(arguments.size());
    for (const ast::TypeSyntax* argument : arguments) spellings.push_back(typeSpelling(argument));

    std::ostringstream keyBuilder;
    keyBuilder << state.record.canonicalKey << '<';
    for (std::size_t i = 0; i < spellings.size(); ++i)
    {
        if (i != 0) keyBuilder << ',';
        keyBuilder << spellings[i];
    }
    keyBuilder << '>';
    const std::string key = keyBuilder.str();
    if (const auto found = specializationByKey_.find(key); found != specializationByKey_.end())
        return specializations_[found->second].specializedName;

    std::string specializedName = state.record.name + "__g" + std::to_string(arguments.size());
    for (const std::string& spelling : spellings) specializedName += "__" + sanitize(spelling);
    specializedName += "__h" + stableHashSuffix(key);

    const std::size_t ordinal = specializations_.size();
    specializationByKey_.emplace(key, ordinal);
    specializations_.push_back(GenericSpecializationRecord{
        GenericTemplateKind::Method,
        state.syntax,
        nullptr,
        state.record.canonicalKey,
        key,
        specializedName,
        spellings,
        ordinal});

    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    for (std::size_t index = 0; index < state.record.parameters.size(); ++index)
        substitutions.emplace(state.record.parameters[index], arguments[index]);

    if (state.unit == nullptr || state.parent == nullptr || state.syntax == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            state.syntax != nullptr ? state.syntax->range() : SourceRange{},
            "Stage 44 generic method template has no materialization parent");
        return {};
    }

    ast::AstNode* clone = cloneNode(state.unit->arena(), state.syntax, substitutions, "", "");
    clone->setText(specializedName);
    clone->clearGenericParameters();
    clone->setAuxiliaryText("stage44-specialization:" + key);
    state.parent->addChild(clone);
    specializations_[ordinal].specializedSyntax = clone;
    processNode(*state.unit, clone, state.record.namespaceName, state.record.ownerName, false, diagnostics);
    return specializedName;
}

ast::TypeSyntax* ProjectGenericModel::cloneType(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions)
{
    if (source == nullptr) return nullptr;

    ast::TypeSyntax* result = nullptr;
    bool substituted = false;
    const auto substitution = substitutions.find(source->baseName());
    if (substitution != substitutions.end() && source->typeArguments().empty())
    {
        substituted = true;
        result = cloneType(arena, substitution->second, {});
        // Stage 52 fix: `result` now names a type ARGUMENT the caller supplied at the
        // instantiation site (e.g. the "MyOwnClass" in "List<MyOwnClass>"), not part of the
        // template's own source text -- so it must resolve using the REQUESTING project's
        // visibility, not the template's defining project's (see TypeSyntax::
        // originProjectOrdinal and the currentInstantiationProjectOrdinal_/
        // currentInstantiationNamespace_ fields for the full explanation). A builtin keyword
        // (int, bool, ...) needs neither: it never goes through name resolution at all.
        // Guard against a NESTED substitution re-tagging an argument that a prior, OUTER
        // substitution already tagged correctly. This happens for e.g. List<T>'s own internal
        // use of Predicate<T> (List.ns): specializing List<Payload> substitutes Predicate<T>'s
        // "T" with the SAME (already AppB-tagged) "Stage52LibTest.Payload" node the outer List<T>
        // substitution produced, but that inner Predicate<T>->Predicate<Payload> specialization
        // request itself runs with the LIBRARY's own unit/namespace as ambient context (it is
        // discovered while walking List<Payload>'s cloned body, not from AppB's own source) --
        // so blindly re-tagging here would silently overwrite the correct origin with the
        // library's, right back into the original bug. Once tagged, leave it tagged.
        if (result != nullptr && !lexer::isBuiltinTypeKeyword(result->baseTokenKind()) &&
            !result->originProjectOrdinal().has_value())
        {
            // Deliberately does NOT touch baseName() -- see TypeSyntax::originNamespace's
            // comment for why an earlier version of this fix that qualified the name broke
            // generic specialization identity. Both origin fields are read only by
            // ProjectNameLookup::bindTypeSyntax to override which project/namespace an
            // otherwise-untouched name resolves against.
            result->setOriginProjectOrdinal(currentInstantiationProjectOrdinal_);
            if (!currentInstantiationNamespace_.empty())
            {
                result->setOriginNamespace(currentInstantiationNamespace_);
            }
        }
    }
    else
    {
        result = arena.make<ast::TypeSyntax>(source->range(), source->baseTokenKind(), source->baseName());
        for (const ast::TypeSyntax* argument : source->typeArguments())
            result->addTypeArgument(cloneType(arena, argument, substitutions));
        // Stage 52 fix: this plain (non-substituted) re-clone still needs to carry forward the
        // origin tags the SOURCE node already carries. This branch is exactly what runs when
        // cloneType is called recursively on a substitution's own target a second time (see the
        // `cloneType(arena, substitution->second, {})` call above) -- e.g. re-cloning the already
        // AppB-tagged "Payload" node while specializing List<Payload>'s internal Predicate<T>
        // usage. Without this, the re-clone starts untagged, the guard above sees "not yet
        // tagged" and stamps it with whatever project/namespace happens to be the CURRENT (here,
        // nested and wrong) ambient instantiation context, right back into the original
        // cross-project bug these fields exist to fix.
        if (source->originProjectOrdinal().has_value())
        {
            result->setOriginProjectOrdinal(source->originProjectOrdinal().value());
        }
        if (source->originNamespace().has_value())
        {
            result->setOriginNamespace(source->originNamespace().value());
        }
    }

    for (const ast::TypeModifierSyntax& modifier : source->modifiers())
    {
        result->addModifier(ast::TypeModifierSyntax{
            modifier.kind,
            cloneNode(arena, modifier.arrayCount, substitutions, "", ""),
            modifier.range});
    }
    // Stage 52 fix: an ownership qualifier (owned/borrowed) belongs to THIS occurrence of the
    // type -- e.g. the "owned" in a parameter declared "owned T" -- not to whatever concrete
    // type argument T is substituted with. Previously this was dropped whenever the
    // substitution branch above ran (source->baseName() == the generic parameter name), so a
    // generic method like "void Add(owned T value)" would monomorphize to an UNQUALIFIED
    // parameter once T was closed over a reference type, silently losing the "owned"
    // requirement and rejecting every legitimate move(...) call site with "ownership-transfer
    // argument requires an owned parameter". Carry it over unconditionally, exactly like the
    // pointer/array modifiers just above.
    result->setOwnershipQualifier(source->ownershipQualifier());
    // Stage 52 fix: only stamp the TEMPLATE's own range for the non-substituted branch (the
    // clone still represents source text written at that template position, e.g. `T[]`'s own
    // array-modifier range). For a substituted leaf, `result` already carries the real argument's
    // own range from the inner cloneType call a few lines up -- overwriting it here with the
    // template parameter's range (e.g. Delegates.ns's own "T") is purely a misleading-diagnostic-
    // location bug: it does not affect name resolution (that is namespace/project-ordinal driven,
    // fixed separately above), but it did make an "unknown type name" error for an argument
    // written in a consuming project point at the template's own file instead.
    if (!substituted)
    {
        result->setRange(source->range());
    }
    return result;
}

ast::AstNode* ProjectGenericModel::cloneNode(
    ast::AstArena& arena,
    const ast::AstNode* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::string_view rewrittenContainingTypeName,
    std::string_view originalContainingTypeName)
{
    if (source == nullptr) return nullptr;
    ast::AstNode* result = arena.make<ast::AstNode>(source->kind(), source->range());
    result->setText(source->text());
    result->setAuxiliaryText(source->auxiliaryText());
    result->setTokenKind(source->tokenKind());
    result->setFlag(source->flag());
    result->setIntegerValue(source->integerValue());
    result->setFloatingValue(source->floatingValue());
    for (const lexer::TokenKind modifier : source->modifiers()) result->addModifier(modifier);
    for (const std::string& parameter : source->genericParameters()) result->addGenericParameter(parameter);

    if (!rewrittenContainingTypeName.empty() && !originalContainingTypeName.empty() &&
        (source->kind() == ast::AstNodeKind::ConstructorDeclaration ||
         source->kind() == ast::AstNodeKind::DestructorDeclaration) &&
        source->text() == originalContainingTypeName)
    {
        result->setText(std::string(rewrittenContainingTypeName));
    }

    result->setTypeSyntax(cloneType(arena, source->typeSyntax(), substitutions));
    for (const ast::TypeSyntax* syntax : source->typeSyntaxes())
        result->addTypeSyntax(cloneType(arena, syntax, substitutions));
    result->setFirst(cloneNode(arena, source->first(), substitutions, rewrittenContainingTypeName, originalContainingTypeName));
    result->setSecond(cloneNode(arena, source->second(), substitutions, rewrittenContainingTypeName, originalContainingTypeName));
    result->setThird(cloneNode(arena, source->third(), substitutions, rewrittenContainingTypeName, originalContainingTypeName));
    for (const ast::AstNode* child : source->children())
        result->addChild(cloneNode(arena, child, substitutions, rewrittenContainingTypeName, originalContainingTypeName));
    return result;
}

std::string ProjectGenericModel::typeSpelling(const ast::TypeSyntax* syntax) const
{
    if (syntax == nullptr) return "<missing>";
    std::string result = syntax->baseName();
    if (!syntax->typeArguments().empty())
    {
        result.push_back('<');
        for (std::size_t index = 0; index < syntax->typeArguments().size(); ++index)
        {
            if (index != 0) result.push_back(',');
            result += typeSpelling(syntax->typeArguments()[index]);
        }
        result.push_back('>');
    }
    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer) result.push_back('*');
        else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray) result += "[]";
        else result += "[fixed]";
    }
    return result;
}

std::string ProjectGenericModel::namespaceAppend(std::string_view prefix, std::string_view suffix)
{
    if (prefix.empty()) return std::string(suffix);
    if (suffix.empty()) return std::string(prefix);
    return std::string(prefix) + "." + std::string(suffix);
}

std::string ProjectGenericModel::sanitize(std::string_view text)
{
    std::string result;
    for (const char raw : text)
    {
        const unsigned char ch = static_cast<unsigned char>(raw);
        if (std::isalnum(ch) != 0) result.push_back(static_cast<char>(ch));
        else result.push_back('_');
    }
    while (!result.empty() && result.back() == '_') result.pop_back();
    if (result.empty()) result = "type";
    return result;
}

std::string ProjectGenericModel::methodCalleeName(const ast::AstNode* call)
{
    if (call == nullptr || call->first() == nullptr) return {};
    const ast::AstNode* callee = call->first();
    if (callee->kind() == ast::AstNodeKind::IdentifierExpression ||
        callee->kind() == ast::AstNodeKind::MemberExpression)
        return callee->text();
    return {};
}

ast::AstNode* ProjectGenericModel::methodCalleeSyntax(ast::AstNode* call)
{
    if (call == nullptr) return nullptr;
    return call->first();
}
void ProjectGenericModel::buildVisibility(const compilation::ProjectGraph& graph)
{
    const auto projects = graph.projects();
    visibleProjects_.assign(projects.size(), {});
    std::unordered_map<std::string, std::size_t> projectByPath;
    const auto pathKey = [](const std::filesystem::path& path)
    {
        std::error_code errorCode;
        std::filesystem::path canonical = std::filesystem::weakly_canonical(path, errorCode);
        if (errorCode) canonical = std::filesystem::absolute(path).lexically_normal();
        std::string key = canonical.generic_string();
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return key;
    };
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
        projectByPath.emplace(pathKey(projects[ordinal].projectFile()), ordinal);

    for (std::size_t start = 0; start < projects.size(); ++start)
    {
        std::vector<bool> reachable(projects.size(), false);
        std::function<void(std::size_t)> visit = [&](std::size_t ordinal)
        {
            for (const std::filesystem::path& reference : projects[ordinal].projectReferences())
            {
                const auto found = projectByPath.find(pathKey(reference));
                if (found == projectByPath.end() || reachable[found->second]) continue;
                reachable[found->second] = true;
                visit(found->second);
            }
        };
        visit(start);
        for (std::size_t ordinal = 0; ordinal < reachable.size(); ++ordinal)
            if (reachable[ordinal]) visibleProjects_[start].push_back(ordinal);
    }
}

bool ProjectGenericModel::projectVisible(std::size_t from, std::size_t target) const noexcept
{
    if (from == target) return true;
    if (from >= visibleProjects_.size()) return false;
    return std::find(visibleProjects_[from].begin(), visibleProjects_[from].end(), target) !=
        visibleProjects_[from].end();
}

}
