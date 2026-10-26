#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "Internal/PendingDiagnostic.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
// El nombre corto de una clave de tipo, para el diagnostico. `type:App::App.Pieza` -> `Pieza`.
// El mensaje ya lleva el sitio; repetir la identidad canonica entera lo haria ilegible.
std::string shortName(std::string_view typeKey)
{
    std::string_view text = typeKey;
    if (text.starts_with("type:")) text.remove_prefix(5);
    if (const std::size_t module = text.find("::"); module != std::string_view::npos)
        text.remove_prefix(module + 2);
    if (const std::size_t dot = text.rfind('.'); dot != std::string_view::npos)
        text.remove_prefix(dot + 1);
    return std::string(text);
}

// Lo ESCRITO, para cuando el argumento no es un tipo declarado y no hay identidad que nombrar:
// `string`, `int[]`, `Pieza*`. Sin esto, un `Caja<string>` rechazado hablaria de una clave
// canonica que el autor no escribio en ninguna parte.
std::string writtenSpelling(const ast::TypeSyntax* syntax)
{
    if (syntax == nullptr) return "?";
    std::string text(syntax->baseName());
    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        switch (modifier.kind)
        {
            case ast::TypeModifierSyntaxKind::Pointer: text += "*"; break;
            case ast::TypeModifierSyntaxKind::Array: text += "[N]"; break;
            case ast::TypeModifierSyntaxKind::DynamicArray: text += "[]"; break;
            case ast::TypeModifierSyntaxKind::RangeArray:
                text += '[' + std::string(
                    modifier.arrayRank > 0 ? modifier.arrayRank - 1 : 0, ',') + ']';
                break;
            case ast::TypeModifierSyntaxKind::FlexibleArray: text += "[*]"; break;
        }
    }
    return text;
}

// La declaracion que una clave de tipo nombra, o null. Se busca por la clave y no por un indice
// prestado de otro modelo: los ordinales de `namedTypes()` son la identidad que comparten la
// herencia y las interfaces, asi que resolver aqui una vez evita tener que decidir a cual de los
// dos modelos preguntar segun lo que el tipo resulte ser.
const NamedTypeDeclaration* declarationForTypeKey(
    const ProjectNameLookup& nameLookup,
    std::string_view typeKey,
    std::size_t& ordinalOut)
{
    if (!typeKey.starts_with("type:")) return nullptr;
    const std::optional<std::size_t> ordinal =
        nameLookup.typeOrdinalForIdentity(typeKey.substr(5));
    if (!ordinal.has_value()) return nullptr;
    ordinalOut = *ordinal;
    return nameLookup.typeForOrdinal(*ordinal);
}

// QUE ES UN TIPO REFERENCIA en este lenguaje, que es lo que `where T : class` promete.
//
// La primera version de esta comprobacion aceptaba clase e interfaz y nada mas, y era mas
// estrecha que el nombre que lleva: un DELEGADO es una referencia --tiene su propia
// NamedTypeDeclarationKind--, `string` lo es, y un ARREGLO lo es. Rechazarlos convertia
// `where T : class` en «where T : class or interface», que no es lo que la clausula dice.
//
// Un PUNTERO no entra, y no por descuido: `Pieza*` no participa del modelo de objetos --no tiene
// descriptor, no admite `is`, no lo recorre la destruccion-- y aceptarlo bajo `class` prometeria
// justamente eso. Se mira el modificador MAS EXTERNO, que es el que decide que es el valor:
// `Pieza*[]` es un arreglo (referencia) y `Pieza[]*` es un puntero.
bool isReferenceTypeArgument(
    const ast::TypeSyntax* syntax,
    const NamedTypeDeclaration* declaration)
{
    if (syntax == nullptr) return false;
    if (syntax->isFunction())
        return syntax->functionCarrier() == ast::FunctionTypeCarrierSyntaxKind::Reference;
    if (!syntax->modifiers().empty())
    {
        // SOLO EL ARREGLO DINAMICO. `T[]` es una referencia --un objeto con cabecera, longitud y
        // vida propia--; `T[N]` es ALMACENAMIENTO INLINE, tantos bytes seguidos dentro de quien lo
        // declara, sin identidad ni indireccion. Este mismo compilador los separa asi en
        // ProjectMethodBodyModel, y meter los dos bajo `class` prometia sobre `int[4]` lo que solo
        // es cierto de `int[]`.
        const ast::TypeModifierSyntaxKind outermost = syntax->modifiers().back().kind;
        return outermost == ast::TypeModifierSyntaxKind::DynamicArray ||
            outermost == ast::TypeModifierSyntaxKind::RangeArray;
    }
    if (syntax->baseTokenKind() == lexer::TokenKind::KwString) return true;
    if (declaration == nullptr) return false;
    return declaration->kind == NamedTypeDeclarationKind::Class ||
        declaration->kind == NamedTypeDeclarationKind::Interface ||
        declaration->kind == NamedTypeDeclarationKind::Delegate;
}

bool isValueTypeArgument(
    const ast::TypeSyntax* syntax,
    const NamedTypeDeclaration* declaration)
{
    if (syntax == nullptr) return false;
    if (syntax->isFunction())
        return syntax->functionCarrier() == ast::FunctionTypeCarrierSyntaxKind::Pointer;
    if (!syntax->modifiers().empty())
    {
        const ast::TypeModifierSyntaxKind outermost = syntax->modifiers().back().kind;
        return outermost != ast::TypeModifierSyntaxKind::DynamicArray &&
            outermost != ast::TypeModifierSyntaxKind::RangeArray;
    }
    if (syntax->baseTokenKind() == lexer::TokenKind::KwVoid ||
        syntax->baseTokenKind() == lexer::TokenKind::KwString)
        return false;
    if (lexer::isBuiltinTypeKeyword(syntax->baseTokenKind())) return true;
    return declaration != nullptr &&
        (declaration->kind == NamedTypeDeclarationKind::Struct ||
         declaration->kind == NamedTypeDeclarationKind::Enum);
}

const NamedTypeDeclaration* declarationForKey(
    const ProjectNameLookup& nameLookup,
    std::string_view typeKey,
    std::size_t& ordinalOut)
{
    return declarationForTypeKey(nameLookup, typeKey, ordinalOut);
}

bool typeKeyIsUnmanaged(
    std::string_view typeKey,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    std::unordered_set<std::size_t>& visiting)
{
    if (typeKey.empty() || typeKey == "builtin:void" || typeKey == "builtin:string" ||
        typeKey.ends_with("[]"))
        return false;
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(typeKey))
        return functionType->carrier == language::FunctionCarrier::Pointer;
    if (typeKey.starts_with("builtin:") || typeKey.ends_with("*")) return true;
    if (const auto element = ProjectMemberModel::fixedArrayElementTypeKey(typeKey); element.has_value())
        return typeKeyIsUnmanaged(*element, nameLookup, memberModel, visiting);
    std::size_t ordinal = 0;
    const NamedTypeDeclaration* declaration = declarationForKey(nameLookup, typeKey, ordinal);
    if (declaration == nullptr) return false;
    if (declaration->kind == NamedTypeDeclarationKind::Enum) return true;
    if (declaration->kind != NamedTypeDeclarationKind::Struct) return false;
    if (!visiting.insert(ordinal).second) return true;
    for (const std::size_t memberOrdinal : memberModel.membersOfType(ordinal))
    {
        const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Field || member.isStatic || member.isConst) continue;
        if (member.declaredOwnership != sema::OwnershipKind::None ||
            !typeKeyIsUnmanaged(member.declaredTypeKey, nameLookup, memberModel, visiting))
        {
            visiting.erase(ordinal);
            return false;
        }
    }
    visiting.erase(ordinal);
    return true;
}

bool hasPublicDefaultConstructor(
    const ast::TypeSyntax* syntax,
    const NamedTypeDeclaration* declaration,
    std::size_t declarationOrdinal,
    const ProjectMemberModel& memberModel)
{
    if (isValueTypeArgument(syntax, declaration)) return true;
    if (declaration == nullptr || declaration->kind != NamedTypeDeclarationKind::Class) return false;
    bool hasExplicitConstructor = false;
    for (const std::size_t memberOrdinal : memberModel.membersOfType(declarationOrdinal))
    {
        const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Constructor || member.isStatic) continue;
        hasExplicitConstructor = true;
        if (member.accessibility == ClassMemberAccessibility::Public && member.parameters.empty())
            return true;
    }
    return !hasExplicitConstructor;
}

bool hasTrackedLifetime(
    const ast::TypeSyntax* syntax,
    const NamedTypeDeclaration* declaration)
{
    if (syntax == nullptr) return false;
    if (syntax->isFunction()) return true;
    if (syntax->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None) return true;
    if (!syntax->modifiers().empty())
    {
        const ast::TypeModifierSyntaxKind outermost = syntax->modifiers().back().kind;
        return outermost == ast::TypeModifierSyntaxKind::Pointer ||
            outermost == ast::TypeModifierSyntaxKind::DynamicArray ||
            outermost == ast::TypeModifierSyntaxKind::RangeArray;
    }
    if (syntax->baseTokenKind() == lexer::TokenKind::KwString) return true;
    if (declaration == nullptr) return false;
    if (declaration->kind == NamedTypeDeclarationKind::Class ||
        declaration->kind == NamedTypeDeclarationKind::Interface ||
        declaration->kind == NamedTypeDeclarationKind::Delegate)
        return true;
    return declaration->syntax != nullptr &&
        declaration->syntax->auxiliaryText().find("borrowedview") != std::string::npos;
}

bool isDefinitelyNonNull(
    const ast::TypeSyntax* syntax,
    const NamedTypeDeclaration* declaration)
{
    if (syntax == nullptr) return false;
    if (syntax->isFunction()) return !syntax->functionIsNullable();
    if (!syntax->modifiers().empty())
    {
        const ast::TypeModifierSyntaxKind outermost = syntax->modifiers().back().kind;
        return outermost == ast::TypeModifierSyntaxKind::Array ||
            outermost == ast::TypeModifierSyntaxKind::FlexibleArray;
    }
    if (syntax->baseTokenKind() == lexer::TokenKind::KwString) return false;
    if (lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()))
        return syntax->baseTokenKind() != lexer::TokenKind::KwVoid;
    return declaration != nullptr &&
        (declaration->kind == NamedTypeDeclarationKind::Struct ||
         declaration->kind == NamedTypeDeclarationKind::Enum);
}

// NS304-T11-F05: a ref-like (stack-only) argument is a borrowed struct view (Span/ReadOnlySpan).
bool isRefLikeArgument(
    const ast::TypeSyntax* syntax,
    const NamedTypeDeclaration* declaration)
{
    if (syntax == nullptr) return false;
    if (syntax->isFunction() || !syntax->modifiers().empty()) return false;
    return declaration != nullptr &&
        declaration->kind == NamedTypeDeclarationKind::Struct &&
        declaration->syntax != nullptr &&
        declaration->syntax->auxiliaryText().find("borrowedview") != std::string::npos;
}

std::string normalizedArgumentIdentity(
    const ast::TypeSyntax* syntax,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel)
{
    if (syntax == nullptr) return "<missing>";
    std::string constantType;
    std::string constantValue;
    if (language::parseConstGenericArgumentMarker(
            syntax->baseName(), constantType, constantValue))
        return "const:" + constantType + "=" + constantValue;
    return memberModel.canonicalTypeKey(syntax, nameLookup);
}

// Un diagnostico ya redactado que TODAVIA no se emitio. Existe por las sobrecargas: cuando varias
// plantillas comparten nombre y aridad se materializan todas, y que una incumpla no dice nada del
// programa hasta saber que ninguna otra servia.
using internal::PendingDiagnostic;
}

std::size_t ProjectGenericConstraintModel::constraintParameterOrdinal(
    const GenericTemplateRecord& templateRecord,
    const ast::GenericConstraintClause& clause)
{
    const std::size_t projectionSeparator = clause.parameterName.find('.');
    const std::string_view constrainedParameter = projectionSeparator == std::string::npos
        ? std::string_view(clause.parameterName)
        : std::string_view(clause.parameterName).substr(0, projectionSeparator);
    const auto parameter = std::ranges::find(
        templateRecord.parameters, constrainedParameter);
    return parameter == templateRecord.parameters.end()
        ? templateRecord.parameters.size()
        : static_cast<std::size_t>(parameter - templateRecord.parameters.begin());
}

void ProjectGenericConstraintModel::appendTemplateClauseAtoms(
    const ast::GenericConstraintClause& clause,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    std::vector<std::string>& clauseAtoms)
{
    if (clause.requiresClass) clauseAtoms.emplace_back("class");
    if (clause.requiresValueType) clauseAtoms.emplace_back("struct");
    if (clause.requiresDefaultConstructor) clauseAtoms.emplace_back("new()");
    if (clause.requiresCopy) clauseAtoms.emplace_back("copy");
    if (clause.requiresMove) clauseAtoms.emplace_back("move");
    if (clause.requiresTrackedLifetime) clauseAtoms.emplace_back("lifetime");
    if (clause.requiresUnmanaged) clauseAtoms.emplace_back("unmanaged");
    if (clause.requiresNotNull) clauseAtoms.emplace_back("notnull");
    if (clause.requiresRefLike) clauseAtoms.emplace_back("ref-like");
    if (clause.equalityConstraint != nullptr)
        clauseAtoms.push_back(
            "eq:" + normalizedArgumentIdentity(
                clause.equalityConstraint, nameLookup, memberModel));
}

bool ProjectGenericConstraintModel::validateTemplateTypeConstraint(
    std::size_t templateOrdinal,
    const ast::GenericConstraintClause& clause,
    const ast::TypeSyntax* constraintSyntax,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    std::unordered_map<std::string, std::string>& seenByKey,
    std::vector<std::pair<const ast::TypeSyntax*, std::size_t>>& baseClasses,
    std::size_t& acceptedConstraints,
    std::vector<std::string>& clauseAtoms,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::string constraintKey =
        memberModel.canonicalTypeKey(constraintSyntax, nameLookup);
    clauseAtoms.push_back("type:" + constraintKey);
    std::size_t constraintOrdinal = 0;
    const NamedTypeDeclaration* constraint =
        declarationForTypeKey(nameLookup, constraintKey, constraintOrdinal);
    if (constraint == nullptr ||
        (constraint->kind != NamedTypeDeclarationKind::Class &&
         constraint->kind != NamedTypeDeclarationKind::Interface))
    {
        violations_.push_back(GenericConstraintViolation{
            GenericConstraintViolationKind::ConstraintIsNotAClassOrInterface,
            templateOrdinal, clause.parameterName, std::string(), constraintKey, true});
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            constraintSyntax->range(),
            "'" + std::string(constraintSyntax->baseName()) +
                "' cannot constrain '" + clause.parameterName +
                "': a generic constraint names a base class or an interface");
        return false;
    }
    const auto previous = seenByKey.find(constraintKey);
    if (previous != seenByKey.end())
    {
        violations_.push_back(GenericConstraintViolation{
            GenericConstraintViolationKind::DuplicateConstraint,
            templateOrdinal, clause.parameterName, std::string(), constraintKey, true});
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            constraintSyntax->range(),
            "'" + std::string(constraintSyntax->baseName()) + "' and '" +
                previous->second + "' name the same type '" + shortName(constraintKey) +
                "'; '" + clause.parameterName + "' is already constrained by it");
        return false;
    }
    seenByKey.emplace(constraintKey, std::string(constraintSyntax->baseName()));
    if (constraint->kind == NamedTypeDeclarationKind::Class)
        baseClasses.emplace_back(constraintSyntax, acceptedConstraints);
    ++acceptedConstraints;
    return true;
}

void ProjectGenericConstraintModel::normalizeTemplateClause(
    std::size_t templateOrdinal,
    std::size_t parameterOrdinal,
    const ast::GenericConstraintClause& clause,
    const std::vector<std::string>& clauseAtoms,
    std::vector<std::string>& templateAtoms,
    diagnostics::DiagnosticEngine& diagnostics)
{
    language::ConstraintSet normalized;
    std::string normalizationError;
    if (!language::normalizeConstraints(clauseAtoms, normalized, normalizationError))
    {
        violations_.push_back(GenericConstraintViolation{
            GenericConstraintViolationKind::InvalidConstraintCombination,
            templateOrdinal, clause.parameterName, std::string(), normalized.canonical, true});
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            clause.range,
            "invalid constraints for generic parameter '" + clause.parameterName +
                "': " + normalizationError);
    }
    for (const std::string& atom : normalized.atoms)
        templateAtoms.push_back("p" + std::to_string(parameterOrdinal) + ":" + atom);
}

void ProjectGenericConstraintModel::validatePrimaryTemplateConstraint(
    std::size_t templateOrdinal,
    const ast::GenericConstraintClause& clause,
    const std::vector<std::pair<const ast::TypeSyntax*, std::size_t>>& baseClasses,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // LA REGLA DE CONSTRAINT PRIMARIO, que es la de C# y no es cosmetica.
    //
    // Una clausula dice DOS cosas de distinta naturaleza: QUE ES el parametro --a lo sumo
    // una: `class`, o una clase base concreta-- y QUE IMPLEMENTA, que pueden ser varias.
    // La primera es unica porque no hay herencia multiple: dos clases base no describen
    // ningun tipo posible, y `class` mas una clase base es redundante en el mejor caso y
    // contradictorio en cuanto la base sea un struct. Aceptarlas dejaba escribir clausulas
    // que no restringen nada y que el lector interpreta como si restringieran.
    //
    // Y va PRIMERA por la misma razon que `class`: la lista se lee de izquierda a derecha
    // como «esto es un X que implementa Y y Z». Con la base en medio deja de leerse.
    if (clause.requiresClass && !baseClasses.empty())
        reportPrimaryConstraint(
            diagnostics, templateOrdinal, clause, baseClasses.front().first,
            "'class' and the base class '" +
                std::string(baseClasses.front().first->baseName()) +
                "' are both primary constraints of '" + clause.parameterName +
                "'; a clause carries at most one");
    else if (baseClasses.size() > 1)
        reportPrimaryConstraint(
            diagnostics, templateOrdinal, clause, baseClasses[1].first,
            "'" + clause.parameterName + "' cannot be constrained by two base classes ('" +
                std::string(baseClasses.front().first->baseName()) + "' and '" +
                std::string(baseClasses[1].first->baseName()) +
                "'); this language has no multiple inheritance");
    else if (!baseClasses.empty() && baseClasses.front().second != 0)
        reportPrimaryConstraint(
            diagnostics, templateOrdinal, clause, baseClasses.front().first,
            "the base class '" + std::string(baseClasses.front().first->baseName()) +
                "' must be the first constraint of the 'where' clause for '" +
                clause.parameterName + "', before any interface");
}

void ProjectGenericConstraintModel::validateTemplateClause(
    std::size_t templateOrdinal,
    const GenericTemplateRecord& templateRecord,
    const ast::GenericConstraintClause& clause,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    std::vector<std::string>& templateAtoms,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::size_t parameterOrdinal =
        constraintParameterOrdinal(templateRecord, clause);
    std::vector<std::string> clauseAtoms;
    appendTemplateClauseAtoms(clause, nameLookup, memberModel, clauseAtoms);
    // Identidad -> primera escritura que la nombro. El parser ya rechaza dos veces el
    // MISMO texto; esto coge las dos escrituras DISTINTAS del mismo tipo --un alias, un
    // nombre corto frente a uno calificado-- que alli eran indistinguibles, porque
    // comparar nombres sin resolver es lo unico que el parser puede hacer.
    std::unordered_map<std::string, std::string> seenByKey;
    // Las clases base encontradas, EN ORDEN DE ESCRITURA, y cuantos constraints validos
    // las precedieron. Con eso se responden las tres preguntas de la regla de constraint
    // primario, que es la que faltaba entera.
    std::vector<std::pair<const ast::TypeSyntax*, std::size_t>> baseClasses;
    std::size_t acceptedConstraints = 0;
    for (const ast::TypeSyntax* constraintSyntax : clause.typeConstraints)
    {
        if (constraintSyntax == nullptr) continue;
        validateTemplateTypeConstraint(
            templateOrdinal, clause, constraintSyntax, nameLookup, memberModel,
            seenByKey, baseClasses, acceptedConstraints, clauseAtoms, diagnostics);
    }
    normalizeTemplateClause(
        templateOrdinal, parameterOrdinal, clause, clauseAtoms, templateAtoms, diagnostics);
    validatePrimaryTemplateConstraint(templateOrdinal, clause, baseClasses, diagnostics);
}

void ProjectGenericConstraintModel::finalizeTemplateConstraintIdentity(
    std::size_t templateOrdinal,
    const std::vector<std::string>& templateAtoms)
{
    language::ConstraintSet normalizedTemplate;
    std::string ignoredError;
    language::normalizeConstraints(templateAtoms, normalizedTemplate, ignoredError);
    if (templateOrdinal < normalizedConstraintIdentities_.size())
        normalizedConstraintIdentities_[templateOrdinal] =
            std::move(normalizedTemplate.canonical);
    if (templateOrdinal < normalizedConstraintAtoms_.size())
        normalizedConstraintAtoms_[templateOrdinal] = std::move(normalizedTemplate.atoms);
}

void ProjectGenericConstraintModel::validateTemplateClauses(
    const ProjectGenericModel& genericModel,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // LO QUE SE COMPRUEBA SOBRE LA PLANTILLA, y por que no sobre la especializacion.
    //
    // `where T : MiEnum` esta mal ESCRITO. No depende de con que se cierre `T`, no depende de que
    // alguien instancie, y el sitio que hay que corregir es la declaracion. Comprobarlo en la
    // especializacion --como hacia la primera version-- lo ataba a que la plantilla se usara: una
    // biblioteca con la clausula mal escrita y sin un solo uso salia verde.
    const std::span<const GenericTemplateRecord> templates = genericModel.templates();
    for (std::size_t templateOrdinal = 0; templateOrdinal < templates.size(); ++templateOrdinal)
    {
        const GenericTemplateRecord& templateRecord = templates[templateOrdinal];
        std::vector<std::string> templateAtoms;
        for (const ast::GenericConstraintClause& clause : templateRecord.constraints)
            validateTemplateClause(
                templateOrdinal, templateRecord, clause, nameLookup,
                memberModel, templateAtoms, diagnostics);
        finalizeTemplateConstraintIdentity(templateOrdinal, templateAtoms);
    }
}

void ProjectGenericConstraintModel::reportControlledSpecializationViolation(
    std::span<const GenericTemplateRecord> templates,
    std::size_t ordinal,
    std::string message,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const GenericTemplateRecord& record = templates[ordinal];
    violations_.push_back(GenericConstraintViolation{
        GenericConstraintViolationKind::InvalidConstraintCombination,
        ordinal,
        std::string(),
        std::string(),
        ordinal < normalizedConstraintIdentities_.size()
            ? normalizedConstraintIdentities_[ordinal] : std::string(),
        true});
    diagnostics.report(
        diagnostics::DiagnosticId::DuplicateSymbol,
        diagnostics::DiagnosticSeverity::Error,
        record.syntax != nullptr ? record.syntax->range() : SourceRange{},
        std::move(message));
}

void ProjectGenericConstraintModel::notePreviousControlledSpecialization(
    std::span<const GenericTemplateRecord> templates,
    std::size_t ordinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const GenericTemplateRecord& record = templates[ordinal];
    diagnostics.report(
        diagnostics::DiagnosticId::PreviousDeclaration,
        diagnostics::DiagnosticSeverity::Note,
        record.syntax != nullptr ? record.syntax->range() : SourceRange{},
        "previous implementation in this controlled-specialization family is here");
}

void ProjectGenericConstraintModel::validateControlledSpecializationPair(
    std::span<const GenericTemplateRecord> templates,
    std::size_t leftOrdinal,
    std::size_t rightOrdinal,
    std::unordered_set<std::size_t>& hasFallback,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::span<const std::string> left = normalizedConstraintAtoms(leftOrdinal);
    const std::span<const std::string> right = normalizedConstraintAtoms(rightOrdinal);
    const bool leftDominates = language::specializationStrictlyDominates(left, right);
    const bool rightDominates = language::specializationStrictlyDominates(right, left);
    if (!leftDominates && !rightDominates)
    {
        const bool equivalent = left.size() == right.size() && std::ranges::equal(left, right);
        reportControlledSpecializationViolation(
            templates,
            rightOrdinal,
            equivalent
                ? "generic implementation '" + templates[rightOrdinal].name +
                    "' has constraints equivalent to an existing implementation; "
                    "controlled specialization requires a strict constraint order"
                : "generic implementations of '" + templates[rightOrdinal].name +
                    "' overlap with incomparable constraints; add a single strictly "
                    "more-constrained specialize implementation or separate their parameter patterns",
            diagnostics);
        notePreviousControlledSpecialization(templates, leftOrdinal, diagnostics);
        return;
    }

    const std::size_t moreSpecific = leftDominates ? leftOrdinal : rightOrdinal;
    const std::size_t lessSpecific = leftDominates ? rightOrdinal : leftOrdinal;
    if (templates[moreSpecific].isControlledSpecialization)
        hasFallback.insert(moreSpecific);
    else
    {
        reportControlledSpecializationViolation(
            templates,
            moreSpecific,
            "more-constrained generic implementation '" + templates[moreSpecific].name +
                "' must opt in with the specialize modifier",
            diagnostics);
        notePreviousControlledSpecialization(templates, lessSpecific, diagnostics);
    }
}

void ProjectGenericConstraintModel::validateControlledSpecializationFamily(
    std::span<const GenericTemplateRecord> templates,
    const std::vector<std::size_t>& ordinals,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (ordinals.size() == 1U)
    {
        const std::size_t ordinal = ordinals.front();
        if (templates[ordinal].isControlledSpecialization)
            reportControlledSpecializationViolation(
                templates,
                ordinal,
                "controlled specialization '" + templates[ordinal].name +
                    "' has no less-constrained fallback implementation",
                diagnostics);
        return;
    }

    std::unordered_set<std::size_t> hasFallback;
    for (std::size_t leftIndex = 0; leftIndex < ordinals.size(); ++leftIndex)
    {
        const std::size_t leftOrdinal = ordinals[leftIndex];
        for (std::size_t rightIndex = leftIndex + 1U;
             rightIndex < ordinals.size(); ++rightIndex)
        {
            const std::size_t rightOrdinal = ordinals[rightIndex];
            validateControlledSpecializationPair(
                templates, leftOrdinal, rightOrdinal, hasFallback, diagnostics);
        }
    }

    for (const std::size_t ordinal : ordinals)
    {
        if (templates[ordinal].isControlledSpecialization &&
            !hasFallback.contains(ordinal))
            reportControlledSpecializationViolation(
                templates,
                ordinal,
                "controlled specialization '" + templates[ordinal].name +
                    "' does not strictly dominate any fallback implementation",
                diagnostics);
    }
}

void ProjectGenericConstraintModel::validateControlledSpecializations(
    const ProjectGenericModel& genericModel,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::span<const GenericTemplateRecord> templates = genericModel.templates();
    std::unordered_map<std::string, std::vector<std::size_t>> families;
    for (std::size_t ordinal = 0; ordinal < templates.size(); ++ordinal)
    {
        const GenericTemplateRecord& record = templates[ordinal];
        if (record.kind == GenericTemplateKind::Method &&
            !record.specializationFamilyKey.empty())
            families[record.specializationFamilyKey].push_back(ordinal);
    }
    for (const auto& [familyKey, ordinals] : families)
    {
        (void)familyKey;
        validateControlledSpecializationFamily(templates, ordinals, diagnostics);
    }
}

void ProjectGenericConstraintModel::reportPrimaryConstraint(
    diagnostics::DiagnosticEngine& diagnostics,
    std::size_t templateOrdinal,
    const ast::GenericConstraintClause& clause,
    const ast::TypeSyntax* offending,
    const std::string& message)
{
    violations_.push_back(GenericConstraintViolation{
        GenericConstraintViolationKind::InvalidPrimaryConstraint,
        templateOrdinal,
        clause.parameterName,
        std::string(),
        offending != nullptr ? std::string(offending->baseName()) : std::string(),
        true});
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        offending != nullptr ? offending->range() : clause.range,
        message);
}

void ProjectGenericConstraintModel::initializeBuildState(std::size_t templateCount)
{
    violations_.clear();
    deferredBySpecialization_.clear();
    invalidSpecializations_.clear();
    invalidTemplateOrdinals_.clear();
    normalizedConstraintIdentities_.assign(templateCount, {});
    normalizedConstraintAtoms_.assign(templateCount, {});
    built_ = true;
}

void ProjectGenericConstraintModel::collectInvalidTemplateOrdinals()
{
    // UNA PLANTILLA MAL DECLARADA INVALIDA A TODAS SUS ESPECIALIZACIONES, y eso se sabe AQUI, no
    // en la segunda pasada: no depende de ningun argumento. Se recolecta de `violations_` porque
    // toda violacion de declaracion guarda ahi el ordinal de la PLANTILLA -- ver el comentario
    // de `invalidTemplateOrdinals_`.
    for (const GenericConstraintViolation& violation : violations_)
    {
        if (violation.isDeclarationViolation)
        {
            invalidTemplateOrdinals_.insert(violation.specializationOrdinal);
        }
    }
}

void ProjectGenericConstraintModel::appendCapabilityViolation(
    bool required,
    bool satisfied,
    GenericConstraintViolationKind kind,
    std::string_view spelling,
    std::string_view expectation,
    const GenericSpecializationRecord& specialization,
    const GenericTemplateRecord& templateRecord,
    const ast::GenericConstraintClause& clause,
    const std::string& argumentKey,
    const std::string& argumentName,
    const std::string& templateModuleSuffix,
    std::vector<internal::PendingDiagnostic>& pending)
{
    if (!required || satisfied) return;
    pending.push_back(PendingDiagnostic{
        diagnostics::DiagnosticId::InvalidType,
        specialization.instantiationRange,
        "generic parameter '" + clause.parameterName + "' of '" +
            templateRecord.name + "' requires " + std::string(expectation) +
            " ('where " + clause.parameterName + " : " +
            std::string(spelling) + "'), but the argument '" + argumentName +
            "' does not satisfy it" + templateModuleSuffix,
        GenericConstraintViolation{
            kind,
            specialization.cacheOrdinal,
            clause.parameterName,
            argumentKey,
            std::string(spelling),
            false}});
}

void ProjectGenericConstraintModel::appendCapabilityViolations(
    const GenericSpecializationRecord& specialization,
    const GenericTemplateRecord& templateRecord,
    const ast::GenericConstraintClause& clause,
    const ast::TypeSyntax* argumentSyntax,
    const NamedTypeDeclaration* argument,
    std::size_t argumentOrdinal,
    const std::string& argumentKey,
    const std::string& argumentName,
    const std::string& templateModuleSuffix,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    std::vector<internal::PendingDiagnostic>& pending)
{
    std::unordered_set<std::size_t> unmanagedVisiting;
    appendCapabilityViolation(
        clause.requiresValueType, isValueTypeArgument(argumentSyntax, argument),
        GenericConstraintViolationKind::RequiresValueType, "struct", "a value type",
        specialization, templateRecord, clause, argumentKey, argumentName,
        templateModuleSuffix, pending);
    appendCapabilityViolation(
        clause.requiresDefaultConstructor,
        hasPublicDefaultConstructor(argumentSyntax, argument, argumentOrdinal, memberModel),
        GenericConstraintViolationKind::RequiresDefaultConstructor, "new()",
        "a public parameterless or implicit default constructor", specialization,
        templateRecord, clause, argumentKey, argumentName, templateModuleSuffix, pending);
    appendCapabilityViolation(
        clause.requiresCopy, memberModel.typeIsImplicitlyCopyable(argumentKey),
        GenericConstraintViolationKind::RequiresCopy, "copy", "copyable storage",
        specialization, templateRecord, clause, argumentKey, argumentName,
        templateModuleSuffix, pending);
    appendCapabilityViolation(
        clause.requiresMove, argumentKey != "builtin:void" && !argumentKey.empty(),
        GenericConstraintViolationKind::RequiresMove, "move", "movable storage",
        specialization, templateRecord, clause, argumentKey, argumentName,
        templateModuleSuffix, pending);
    appendCapabilityViolation(
        clause.requiresTrackedLifetime, hasTrackedLifetime(argumentSyntax, argument),
        GenericConstraintViolationKind::RequiresTrackedLifetime, "lifetime",
        "a type with an explicit tracked lifetime", specialization, templateRecord,
        clause, argumentKey, argumentName, templateModuleSuffix, pending);
    appendCapabilityViolation(
        clause.requiresUnmanaged,
        typeKeyIsUnmanaged(argumentKey, nameLookup, memberModel, unmanagedVisiting),
        GenericConstraintViolationKind::RequiresUnmanaged, "unmanaged",
        "recursively unmanaged storage", specialization, templateRecord, clause,
        argumentKey, argumentName, templateModuleSuffix, pending);
    appendCapabilityViolation(
        clause.requiresNotNull, isDefinitelyNonNull(argumentSyntax, argument),
        GenericConstraintViolationKind::RequiresNotNull, "notnull",
        "a representation that cannot be null", specialization, templateRecord,
        clause, argumentKey, argumentName, templateModuleSuffix, pending);
    appendCapabilityViolation(
        clause.requiresRefLike, isRefLikeArgument(argumentSyntax, argument),
        GenericConstraintViolationKind::RequiresRefLike, "ref-like",
        "a stack-only borrowed view (Span/ReadOnlySpan)", specialization,
        templateRecord, clause, argumentKey, argumentName, templateModuleSuffix, pending);
}

void ProjectGenericConstraintModel::appendEqualityViolation(
    const GenericSpecializationRecord& specialization,
    const GenericTemplateRecord& templateRecord,
    const ast::GenericConstraintClause& clause,
    const ast::TypeSyntax* argumentSyntax,
    const std::string& argumentName,
    const std::string& templateModuleSuffix,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    std::vector<internal::PendingDiagnostic>& pending)
{
    if (clause.equalityConstraint == nullptr) return;
    const std::string requiredIdentity = normalizedArgumentIdentity(
        clause.equalityConstraint, nameLookup, memberModel);
    const std::string argumentIdentity = normalizedArgumentIdentity(
        argumentSyntax, nameLookup, memberModel);
    if (requiredIdentity == argumentIdentity) return;
    pending.push_back(PendingDiagnostic{
        diagnostics::DiagnosticId::InvalidType,
        specialization.instantiationRange,
        "generic parameter '" + clause.parameterName + "' of '" + templateRecord.name +
            "' requires equality with '" + shortName(requiredIdentity) +
            "', but the argument '" + argumentName + "' has identity '" +
            shortName(argumentIdentity) + "'" + templateModuleSuffix,
        GenericConstraintViolation{
            GenericConstraintViolationKind::EqualityMismatch,
            specialization.cacheOrdinal,
            clause.parameterName,
            argumentIdentity,
            requiredIdentity,
            false}});
}

void ProjectGenericConstraintModel::appendReferenceTypeViolation(
    const GenericSpecializationRecord& specialization,
    const GenericTemplateRecord& templateRecord,
    const ast::GenericConstraintClause& clause,
    const ast::TypeSyntax* argumentSyntax,
    const NamedTypeDeclaration* argument,
    const std::string& argumentKey,
    const std::string& argumentName,
    const std::string& templateModuleSuffix,
    std::vector<internal::PendingDiagnostic>& pending)
{
    // `class`: el argumento tiene que ser un tipo REFERENCIA -- clase, interfaz, delegado,
    // `string` o arreglo. El calificador de propiedad del uso no entra aqui y no se toca:
    // `where T : class` habla de que ES T, no de quien lo posee.
    if (!clause.requiresClass || isReferenceTypeArgument(argumentSyntax, argument)) return;
    pending.push_back(PendingDiagnostic{
        diagnostics::DiagnosticId::InvalidType,
        specialization.instantiationRange,
        "generic parameter '" + clause.parameterName + "' of '" + templateRecord.name +
            "' requires a reference type ('where " + clause.parameterName +
            " : class'), but the argument '" + argumentName +
            "' is not a class, interface, delegate, string or dynamic array" +
            templateModuleSuffix,
        GenericConstraintViolation{
            GenericConstraintViolationKind::RequiresReferenceType,
            specialization.cacheOrdinal,
            clause.parameterName,
            argumentKey,
            "class",
            false}});
}

bool ProjectGenericConstraintModel::satisfiesThreadSafetyConstraint(
    const NamedTypeDeclaration& argument,
    std::size_t argumentOrdinal,
    bool sendMarker,
    const ProjectMemberModel& memberModel)
{
    // NS304-T07-F07: Send/Sync derivan estructuralmente — un tipo es Send/Sync
    // cuando sus campos lo son y no contiene raw pointers. No se exige `: Send`.
    language::ThreadSafetyTypeFacts facts;
    facts.typeName = argument.identity.name();
    std::vector<std::size_t> fallbackMemberOrdinals;
    std::span<const std::size_t> memberOrdinals =
        memberModel.membersDeclaredByType(argumentOrdinal);
    if (!memberModel.hasMembersDeclaredByTypeIndex())
    {
        for (std::size_t ordinal = 0; ordinal < memberModel.members().size(); ++ordinal)
        {
            if (memberModel.members()[ordinal].declaringTypeOrdinal == argumentOrdinal)
                fallbackMemberOrdinals.push_back(ordinal);
        }
        memberOrdinals = fallbackMemberOrdinals;
    }
    for (const std::size_t memberOrdinal : memberOrdinals)
    {
        if (memberOrdinal >= memberModel.members().size()) continue;
        const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Field) continue;
        language::ThreadSafetyMemberFacts memberFacts;
        memberFacts.name = member.name;
        if (member.declaredTypeKey.starts_with("refmut:"))
        {
            // NS304-T07-F07: un `ref mut T` es mutable -> Send del referido, no Sync.
            memberFacts.kind = language::ThreadSafetyMemberKind::MutableReference;
            memberFacts.send = true;
            memberFacts.sync = false;
        }
        else if (member.declaredTypeKey.starts_with("ref:"))
        {
            memberFacts.kind = language::ThreadSafetyMemberKind::SharedReference;
            memberFacts.send = true;
            memberFacts.sync = true;
        }
        else if (member.declaredTypeKey.ends_with("*"))
        {
            memberFacts.kind = language::ThreadSafetyMemberKind::RawPointer;
            memberFacts.send = false;
            memberFacts.sync = false;
        }
        else
        {
            memberFacts.kind = language::ThreadSafetyMemberKind::Value;
            memberFacts.send = true;
            memberFacts.sync = true;
        }
        facts.members.push_back(std::move(memberFacts));
    }
    const language::ThreadSafetyDecision decision = language::evaluateThreadSafety(facts);
    return sendMarker ? decision.send : decision.sync;
}

bool ProjectGenericConstraintModel::satisfiesTypeConstraint(
    const NamedTypeDeclaration& argument,
    std::size_t argumentOrdinal,
    const NamedTypeDeclaration& constraint,
    std::size_t constraintOrdinal,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel)
{
    const bool sendMarker = constraint.identity.name() == "Send";
    const bool syncMarker = constraint.identity.name() == "Sync";
    if ((sendMarker || syncMarker) &&
        (argument.kind == NamedTypeDeclarationKind::Class ||
         argument.kind == NamedTypeDeclarationKind::Struct))
        return satisfiesThreadSafetyConstraint(
            argument, argumentOrdinal, sendMarker, memberModel);
    if (constraint.kind == NamedTypeDeclarationKind::Interface)
    {
        // Dos caminos, y hacen falta los dos: una CLASE la implementa, y una INTERFAZ
        // la extiende. `includeIdentity` es true en el segundo porque `where T : IFoo`
        // lo satisface `IFoo` misma.
        return ((argument.kind == NamedTypeDeclarationKind::Class ||
                 argument.kind == NamedTypeDeclarationKind::Struct) &&
                interfaceModel.classImplementsInterface(argumentOrdinal, constraintOrdinal)) ||
            (argument.kind == NamedTypeDeclarationKind::Interface &&
             interfaceModel.interfaceDerivesFrom(argumentOrdinal, constraintOrdinal, true));
    }
    // Base class. La identidad cuenta: `where T : Base` lo satisface `Base`.
    return inheritanceModel.isDerivedFrom(argumentOrdinal, constraintOrdinal, true);
}

void ProjectGenericConstraintModel::appendTypeConstraintViolation(
    const GenericSpecializationRecord& specialization,
    const GenericTemplateRecord& templateRecord,
    const ast::GenericConstraintClause& clause,
    const ast::TypeSyntax* constraintSyntax,
    const NamedTypeDeclaration* argument,
    std::size_t argumentOrdinal,
    const std::string& argumentKey,
    const std::string& argumentName,
    const std::string& templateModuleSuffix,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    std::vector<internal::PendingDiagnostic>& pending)
{
    const std::string constraintKey =
        memberModel.canonicalTypeKey(constraintSyntax, nameLookup);
    std::size_t constraintOrdinal = 0;
    const NamedTypeDeclaration* constraint =
        declarationForTypeKey(nameLookup, constraintKey, constraintOrdinal);

    // Un constraint que no nombra clase ni interfaz ya lo denuncio la primera pasada,
    // sobre la plantilla y una sola vez. Aqui se salta: repetirlo por cada `Caja<X>`
    // del programa llenaria la salida de copias del mismo error de declaracion.
    if (constraint == nullptr ||
        (constraint->kind != NamedTypeDeclarationKind::Class &&
         constraint->kind != NamedTypeDeclarationKind::Interface))
        return;
    if (argument == nullptr)
    {
        // El argumento no nombra un tipo declarado -- un `int`, un `string`, un
        // arreglo, un puntero. Ninguno deriva de nada ni implementa nada.
        pending.push_back(PendingDiagnostic{
            diagnostics::DiagnosticId::InvalidType,
            specialization.instantiationRange,
            "generic parameter '" + clause.parameterName + "' of '" +
                templateRecord.name + "' requires '" + shortName(constraintKey) +
                "', but the argument '" + argumentName + "' does not satisfy it" +
                templateModuleSuffix,
            GenericConstraintViolation{
                constraint->kind == NamedTypeDeclarationKind::Interface
                    ? GenericConstraintViolationKind::DoesNotImplementInterface
                    : GenericConstraintViolationKind::DoesNotDeriveFromBase,
                specialization.cacheOrdinal,
                clause.parameterName,
                argumentKey,
                constraintKey,
                false}});
        return;
    }
    if (satisfiesTypeConstraint(
            *argument, argumentOrdinal, *constraint, constraintOrdinal, memberModel,
            inheritanceModel, interfaceModel))
        return;
    pending.push_back(PendingDiagnostic{
        diagnostics::DiagnosticId::InvalidType,
        specialization.instantiationRange,
        "generic parameter '" + clause.parameterName + "' of '" +
            templateRecord.name + "' requires " +
            (constraint->kind == NamedTypeDeclarationKind::Interface
                 ? "an implementation of '"
                 : "a type derived from '") +
            shortName(constraintKey) + "', but the argument '" + argumentName + "' " +
             (constraint->kind == NamedTypeDeclarationKind::Interface
                  ? "does not implement it"
                  : "does not derive from it") + templateModuleSuffix,
        GenericConstraintViolation{
            constraint->kind == NamedTypeDeclarationKind::Interface
                ? GenericConstraintViolationKind::DoesNotImplementInterface
                : GenericConstraintViolationKind::DoesNotDeriveFromBase,
            specialization.cacheOrdinal,
            clause.parameterName,
            argumentKey,
            constraintKey,
            false}});
}

void ProjectGenericConstraintModel::appendTypeConstraintViolations(
    const GenericSpecializationRecord& specialization,
    const GenericTemplateRecord& templateRecord,
    const ast::GenericConstraintClause& clause,
    const NamedTypeDeclaration* argument,
    std::size_t argumentOrdinal,
    const std::string& argumentKey,
    const std::string& argumentName,
    const std::string& templateModuleSuffix,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    std::vector<internal::PendingDiagnostic>& pending)
{
    for (const ast::TypeSyntax* constraintSyntax : clause.typeConstraints)
    {
        if (constraintSyntax == nullptr) continue;
        appendTypeConstraintViolation(
            specialization, templateRecord, clause, constraintSyntax, argument,
            argumentOrdinal, argumentKey, argumentName, templateModuleSuffix, nameLookup,
            memberModel, inheritanceModel, interfaceModel, pending);
    }
}

void ProjectGenericConstraintModel::validateSpecializationClause(
    const GenericSpecializationRecord& specialization,
    const GenericTemplateRecord& templateRecord,
    const ast::GenericConstraintClause& clause,
    const std::string& templateModuleSuffix,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    std::vector<internal::PendingDiagnostic>& pending)
{
    // El argumento con el que se cerro este parametro. Falta solo si la sustitucion no lo
    // tenia, que el parser ya habria reportado como parametro desconocido: se sigue en
    // silencio en vez de encadenar un segundo diagnostico sobre la misma causa.
    const ast::TypeSyntax* argumentSyntax = clause.substitutedArgument;
    if (argumentSyntax == nullptr) return;
    const std::string argumentKey = memberModel.canonicalTypeKey(argumentSyntax, nameLookup);
    std::size_t argumentOrdinal = 0;
    const NamedTypeDeclaration* argument =
        declarationForTypeKey(nameLookup, argumentKey, argumentOrdinal);
    const std::string argumentName = argument != nullptr
        ? shortName(argumentKey)
        : writtenSpelling(argumentSyntax);
    appendCapabilityViolations(
        specialization, templateRecord, clause, argumentSyntax, argument, argumentOrdinal,
        argumentKey, argumentName, templateModuleSuffix, nameLookup, memberModel, pending);
    appendEqualityViolation(
        specialization, templateRecord, clause, argumentSyntax, argumentName,
        templateModuleSuffix, nameLookup, memberModel, pending);
    appendReferenceTypeViolation(
        specialization, templateRecord, clause, argumentSyntax, argument, argumentKey,
        argumentName, templateModuleSuffix, pending);
    appendTypeConstraintViolations(
        specialization, templateRecord, clause, argument, argumentOrdinal, argumentKey,
        argumentName, templateModuleSuffix, nameLookup, memberModel, inheritanceModel,
        interfaceModel, pending);
}

void ProjectGenericConstraintModel::storeSpecializationViolations(
    const GenericSpecializationRecord& specialization,
    const std::vector<internal::PendingDiagnostic>& pending,
    std::vector<internal::PendingDiagnostic>& ungrouped)
{
    if (pending.empty()) return;
    // Invalida es invalida: se marca SIEMPRE, antes de decidir si su diagnostico se aplaza.
    invalidSpecializations_.insert(specialization.specializedSyntax);
    if (specialization.overloadGroup == 0)
    {
        // No forma grupo: es la unica respuesta posible a su sitio, asi que su violacion es
        // el error del programa y se emite.
        ungrouped.insert(ungrouped.end(), pending.begin(), pending.end());
        return;
    }
    // Forma grupo: se GUARDA y no se dice nada. Quien decide si esta candidata es la que el
    // programa usa es la resolucion de sobrecargas, que conoce los tipos de los argumentos.
    std::vector<DeferredDiagnostic>& deferred =
        deferredBySpecialization_[specialization.specializedSyntax];
    for (const PendingDiagnostic& entry : pending)
        deferred.push_back(
            DeferredDiagnostic{entry.id, entry.range, entry.message, entry.violation});
}

// EN: Freeze 3.0 P1: the member clauses of a closed type (`ToArray() where T : copy` in
//     `List<Guard>`). A member whose clause fails is marked invalid and its diagnostics are only
//     deferred: they reach the program at the call that selects the member, where overload
//     resolution already reports deferred constraint failures. The type itself stays valid, and
//     the analysis and HIR guards skip that member's body. O(members of the closed type).
// ES: Congelamiento 3.0 P1: las clausulas de miembro de un tipo cerrado (`ToArray() where T : copy`
//     en `List<Guard>`). Un miembro cuya clausula falla se marca invalido y sus diagnosticos solo
//     se aplazan: llegan al programa en la llamada que lo elige, donde la resolucion de sobrecargas
//     ya informa los constraints aplazados. El tipo sigue valido, y las guardas de analisis y de
//     HIR saltan el cuerpo de ese miembro. O(miembros del tipo cerrado).
void ProjectGenericConstraintModel::validateSpecializationMemberClauses(
    const GenericSpecializationRecord& specialization,
    const GenericTemplateRecord& templateRecord,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel)
{
    for (const ast::AstNode* member : specialization.specializedSyntax->children())
    {
        if (member == nullptr || member->kind() != ast::AstNodeKind::MethodDeclaration) continue;
        std::vector<PendingDiagnostic> pending;
        GenericTemplateRecord memberRecord;
        memberRecord.name = templateRecord.name + "." + member->text();
        const std::string templateModuleSuffix =
            "; template is declared in module '" + templateRecord.moduleName + "'";
        for (const ast::GenericConstraintClause& clause : member->genericConstraints())
        {
            if (!clause.enclosingTypeParameter) continue;
            validateSpecializationClause(
                specialization, memberRecord, clause, templateModuleSuffix, nameLookup, memberModel,
                inheritanceModel, interfaceModel, pending);
        }
        if (pending.empty()) continue;
        invalidSpecializations_.insert(member);
        std::vector<DeferredDiagnostic>& deferred = deferredBySpecialization_[member];
        for (const PendingDiagnostic& entry : pending)
            deferred.push_back(DeferredDiagnostic{entry.id, entry.range, entry.message, entry.violation});
    }
}

void ProjectGenericConstraintModel::validateSpecialization(
    const GenericSpecializationRecord& specialization,
    std::span<const GenericTemplateRecord> templates,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    std::vector<internal::PendingDiagnostic>& ungrouped)
{
    if (specialization.templateOrdinal >= templates.size()) return;
    const GenericTemplateRecord& templateRecord = templates[specialization.templateOrdinal];
    if (specialization.specializedSyntax == nullptr) return;
    if (!invalidTemplateOrdinals_.contains(specialization.templateOrdinal))
        validateSpecializationMemberClauses(
            specialization, templateRecord, nameLookup, memberModel, inheritanceModel,
            interfaceModel);
    if (templateRecord.constraints.empty()) return;
    // LA PLANTILLA YA ERA INVALIDA ANTES DE QUE ESTA ESPECIALIZACION EXISTIERA. Se marca y se
    // sale SIN emitir nada: el error ya salio sobre el `where`, que es donde hay que
    // corregirlo. Repetirlo aqui, una vez por instanciacion, seria el muro de errores de
    // plantilla que esta fase existe para quitar.
    if (invalidTemplateOrdinals_.contains(specialization.templateOrdinal))
    {
        invalidSpecializations_.insert(specialization.specializedSyntax);
        return;
    }
    std::vector<PendingDiagnostic> pending;
    const std::string templateModuleSuffix =
        "; template is declared in module '" + templateRecord.moduleName + "'";
    // SE LEEN LAS CLAUSULAS DEL NODO ESPECIALIZADO, no las de la plantilla, y esa es la pieza
    // que hace innecesaria una segunda resolucion de nombres: el clon esta EN EL ARBOL, asi
    // que la resolucion ordinaria ya visito sus tipos de constraint y el argumento que cada
    // clausula lleva colgado. La plantilla solo aporta su nombre, para el mensaje.
    for (const ast::GenericConstraintClause& clause :
         specialization.specializedSyntax->genericConstraints())
        validateSpecializationClause(
            specialization, templateRecord, clause, templateModuleSuffix, nameLookup, memberModel,
            inheritanceModel, interfaceModel, pending);
    storeSpecializationViolations(specialization, pending, ungrouped);
}

void ProjectGenericConstraintModel::emitUngroupedViolations(
    const std::vector<internal::PendingDiagnostic>& ungrouped,
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (const PendingDiagnostic& entry : ungrouped)
    {
        violations_.push_back(entry.violation);
        diagnostics.report(
            entry.id,
            diagnostics::DiagnosticSeverity::Error,
            entry.range,
            entry.message);
    }
}

bool ProjectGenericConstraintModel::build(
    const ProjectGenericModel& genericModel,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    diagnostics::DiagnosticEngine& diagnostics)
{
    initializeBuildState(genericModel.templates().size());

    // PRIMERA PASADA: lo que esta mal escrito en la declaracion, se instancie o no.
    validateTemplateClauses(genericModel, nameLookup, memberModel, diagnostics);
    validateControlledSpecializations(genericModel, diagnostics);
    collectInvalidTemplateOrdinals();

    // SEGUNDA PASADA: lo que un ARGUMENTO concreto incumple. Se acumula sin emitir, porque una
    // especializacion puede ser una candidata descartada de un conjunto de sobrecargas.
    const std::span<const GenericTemplateRecord> templates = genericModel.templates();
    std::vector<PendingDiagnostic> ungrouped;
    for (const GenericSpecializationRecord& specialization : genericModel.specializations())
        validateSpecialization(
            specialization, templates, nameLookup, memberModel, inheritanceModel,
            interfaceModel, ungrouped);
    emitUngroupedViolations(ungrouped, diagnostics);

    // Lo agrupado NO se emite aqui. Vive en `deferredBySpecialization_` hasta que la resolucion
    // de sobrecargas pregunte por una candidata concreta. Por eso el valor de retorno mira solo
    // lo emitido: decir «hay violaciones» de algo que quiza ningun programa usa seria mentir.
    return violations_.empty();
}

bool ProjectGenericConstraintModel::specializationSatisfiesConstraints(
    const ast::AstNode* specializedSyntax) const noexcept
{
    if (specializedSyntax == nullptr) return true;
    return !invalidSpecializations_.contains(specializedSyntax);
}

void ProjectGenericConstraintModel::reportSpecializationConstraintViolations(
    const ast::AstNode* specializedSyntax,
    SourceRange callRange,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (specializedSyntax == nullptr) return;
    const auto entry = deferredBySpecialization_.find(specializedSyntax);
    if (entry == deferredBySpecialization_.end()) return;
    for (const DeferredDiagnostic& deferred : entry->second)
    {
        // El rango del sitio manda sobre el guardado, que es el de la PRIMERA instanciacion: la
        // especializacion se cachea por clave, asi que una segunda llamada con el mismo argumento
        // no lo actualiza. Reportar el guardado apuntaria a una linea que ya compilaba.
        diagnostics.report(
            deferred.id, diagnostics::DiagnosticSeverity::Error, callRange, deferred.message);
    }
}

std::span<const GenericConstraintViolation> ProjectGenericConstraintModel::violations() const noexcept
{
    return violations_;
}

bool ProjectGenericConstraintModel::isBuilt() const noexcept
{
    return built_;
}

std::string_view ProjectGenericConstraintModel::normalizedConstraintIdentity(
    std::size_t templateOrdinal) const noexcept
{
    return templateOrdinal < normalizedConstraintIdentities_.size()
        ? std::string_view(normalizedConstraintIdentities_[templateOrdinal])
        : std::string_view{};
}

std::span<const std::string> ProjectGenericConstraintModel::normalizedConstraintAtoms(
    std::size_t templateOrdinal) const noexcept
{
    return templateOrdinal < normalizedConstraintAtoms_.size()
        ? std::span<const std::string>(normalizedConstraintAtoms_[templateOrdinal])
        : std::span<const std::string>{};
}
}
