#include "nativesharp/Frontend/ProjectGenericModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kGenericRawNewAuxiliary = "stage52-generic-raw-new";

bool isGenericTypeTemplateNode(const ast::AstNode* node) noexcept
{
    if (node == nullptr || node->genericParameters().empty()) return false;
    return node->kind() == ast::AstNodeKind::ClassDeclaration ||
        node->kind() == ast::AstNodeKind::StructDeclaration ||
        node->kind() == ast::AstNodeKind::InterfaceDeclaration ||
        node->kind() == ast::AstNodeKind::DelegateDeclaration;
}

bool isNamedTypeDeclaration(ast::AstNodeKind kind) noexcept
{
    return kind == ast::AstNodeKind::ClassDeclaration ||
        kind == ast::AstNodeKind::InterfaceDeclaration ||
        kind == ast::AstNodeKind::StructDeclaration ||
        kind == ast::AstNodeKind::EnumDeclaration ||
        kind == ast::AstNodeKind::DelegateDeclaration;
}

GenericTemplateKind genericTypeTemplateKind(const ast::AstNode* node) noexcept
{
    if (node->kind() == ast::AstNodeKind::ClassDeclaration) return GenericTemplateKind::Class;
    if (node->kind() == ast::AstNodeKind::InterfaceDeclaration) return GenericTemplateKind::Interface;
    if (node->kind() == ast::AstNodeKind::DelegateDeclaration) return GenericTemplateKind::Delegate;
    return GenericTemplateKind::Struct;
}

// ID 132. Cuantos parametros DE VALOR declara un metodo. La identidad de una plantilla generica
// era `nombre + numero de parametros de TIPO` y nada mas, asi que dos metodos genericos con el
// mismo nombre y la misma aridad generica chocaban aunque tomaran cosas distintas:
//
//     duplicate generic template identity 'NativeSharp.Core.Spans.Of``1'
//
// Eso obligo a `Spans.OfRange` en vez de `Spans.Of` y a `Algorithm.SortWith` en vez de una
// sobrecarga de `Sort` -- dos nombres peores, escritos a proposito y anotados en el codigo.
std::size_t valueParameterCount(const ast::AstNode* node) noexcept
{
    if (node == nullptr) return 0;
    std::size_t count = 0;
    for (const ast::AstNode* child : node->children())
    {
        if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration) ++count;
    }
    return count;
}

bool isGenericMethodTemplateNode(const ast::AstNode* node) noexcept
{
    return node != nullptr && node->kind() == ast::AstNodeKind::MethodDeclaration &&
        !node->genericParameters().empty();
}

GenericTemplateAccessibility genericTemplateAccessibility(
    const ast::AstNode* node,
    bool isMethod) noexcept
{
    if (node == nullptr)
        return isMethod ? GenericTemplateAccessibility::Private
                        : GenericTemplateAccessibility::Internal;
    switch (node->tokenKind())
    {
        case lexer::TokenKind::KwPublic:
            return GenericTemplateAccessibility::Public;
        case lexer::TokenKind::KwProtected:
            return GenericTemplateAccessibility::Protected;
        case lexer::TokenKind::KwPrivate:
            return GenericTemplateAccessibility::Private;
        case lexer::TokenKind::KwInternal:
        case lexer::TokenKind::Invalid:
        default:
            return isMethod ? GenericTemplateAccessibility::Private
                            : GenericTemplateAccessibility::Internal;
    }
}

std::string qualifiedName(std::string_view ns, std::string_view name)
{
    if (ns.empty()) return std::string(name);
    return std::string(ns) + "." + std::string(name);
}

// NSC-LIMIT-009. La cadena de tipos contenedores extendida con uno mas, unida con `+` igual que
// `TypeIdentity::fullyQualifiedName`.
//
// Este modelo llevaba UN nombre de dueno --el del contenedor mas interno-- y lo REEMPLAZABA al
// bajar, de modo que `A.Inner` y `B.Inner` eran los dos «Inner». Con eso, dos plantillas sin
// ninguna relacion compartian clave canonica, se agrupaban como sobrecargas la una de la otra y
// podian compartir especializacion. La cadena entera es lo unico que las separa.
std::string appendOwner(std::string_view chain, std::string_view name)
{
    if (chain.empty()) return std::string(name);
    return std::string(chain) + "+" + std::string(name);
}

// El receptor de `N1.C.F<int>(x)` TAL COMO SE ESCRIBIO: `N1.C`. Con puntos, sin interpretar.
//
// LA VERSION ANTERIOR LO CONVERTIA A `N1+C`, y eso era un error de principio, no de detalle: esta
// fase corre ANTES de la resolucion de nombres, de modo que no puede saber que en `N1.C` el primero
// es un namespace y el segundo un tipo, ni que en `A.Inner` los dos son tipos. Poner `+` es afirmar
// una de las dos lecturas sin datos para elegir.
//
// Medido con la version anterior: `N1.C.F<int>(1)` producia `N1+C`, que no coincide con NINGUN
// registro --el de N1 tiene owner `C`-- de modo que el filtro de dueno descartaba las dos
// plantillas y solo el fallback rescataba la llamada. Con dos `F<T>` visibles de la misma aridad,
// ese fallback no es ambiguo a veces: lo es siempre, y dos llamadas completamente calificadas no
// compilaban.
//
// Devuelve vacio si el receptor no es una cadena de identificadores --una expresion, un indice, una
// llamada--; entonces se conserva el comportamiento anterior.
std::string receiverSourceSpelling(const ast::AstNode* receiver)
{
    if (receiver == nullptr) return {};
    if (receiver->kind() == ast::AstNodeKind::IdentifierExpression) return receiver->text();
    if (receiver->kind() != ast::AstNodeKind::MemberExpression) return {};
    const std::string prefix = receiverSourceSpelling(receiver->first());
    if (prefix.empty()) return {};
    return prefix + "." + receiver->text();
}

// La escritura fuente COMPLETA de una plantilla: `N1.C`, `a3.A.Inner`. El registro guarda el
// namespace y la cadena de contenedores por separado, y la cadena usa `+` porque es la identidad
// canonica; aqui se vuelve a puntos, que es lo que el programador escribe.
std::string templateSourceSpelling(std::string_view namespaceName, std::string_view ownerChain)
{
    std::string owner(ownerChain);
    for (char& character : owner)
    {
        if (character == '+') character = '.';
    }
    return qualifiedName(namespaceName, owner);
}

// LOS SEGMENTOS DE UN NOMBRE PUNTEADO. `A.B` -> {"A","B"}; el vacio no da segmentos.
std::vector<std::string> splitDotted(std::string_view text)
{
    std::vector<std::string> segments;
    std::size_t begin = 0;
    while (begin <= text.size())
    {
        const std::size_t dot = text.find('.', begin);
        const std::size_t stop = dot == std::string_view::npos ? text.size() : dot;
        if (stop > begin) segments.emplace_back(text.substr(begin, stop - begin));
        if (dot == std::string_view::npos) break;
        begin = dot + 1;
    }
    return segments;
}

// LA ESCRITURA COMPLETA DE CADA TIPO CONTENEDOR DEL SITIO, de fuera adentro. El modelo guarda la
// cadena de duenos con `+` --es la identidad canonica-- y aqui hace falta con puntos y ACUMULADA:
// dentro de `A.Outer+Medio` los contenedores son `A.Outer` y `A.Outer.Medio`, en ese orden, que es
// lo que el escalon de contenedores necesita para probar de dentro afuera.
std::vector<std::string> enclosingTypeSpellings(
    std::string_view namespaceName, std::string_view ownerChain)
{
    std::vector<std::string> spellings;
    std::string current(namespaceName);
    std::size_t begin = 0;
    while (begin <= ownerChain.size())
    {
        const std::size_t plus = ownerChain.find('+', begin);
        const std::size_t stop = plus == std::string_view::npos ? ownerChain.size() : plus;
        if (stop > begin)
        {
            if (!current.empty()) current.push_back('.');
            current.append(ownerChain.substr(begin, stop - begin));
            spellings.push_back(current);
        }
        if (plus == std::string_view::npos) break;
        begin = plus + 1;
    }
    return spellings;
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
        case GenericTemplateKind::Interface: return "interface";
    }
    return "generic";
}

bool ProjectGenericModel::build(
    const compilation::ProjectGraph& graph,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    std::span<const PrecompiledGenericSpecialization> precompiledSpecializations,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_) return !diagnostics.hasErrors();
    buildVisibility(graph);

    // Item 276. Generic aliases are transparent source spellings, so erase them before collecting
    // templates or dependency edges.  Doing it later would let `Alias<int>` acquire a bogus
    // specialization identity and would leave aliases inside template bodies until after cloning.
    for (const std::unique_ptr<ProjectSyntaxUnit>& unitPtr : units)
    {
        if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
        expandAliasesInNode(
            *unitPtr, unitPtr->syntaxTree(), "", GenericLookupScope{}, diagnostics);
    }
    if (diagnostics.hasErrors())
    {
        built_ = true;
        return false;
    }

    for (const std::unique_ptr<ProjectSyntaxUnit>& unitPtr : units)
    {
        if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
        ProjectSyntaxUnit& unit = *unitPtr;
        collectTemplates(unit, unit.syntaxTree(), nullptr, "", "", false, diagnostics);
    }

    // ID 132. LA DESAMBIGUACION SE APLICA SOLO DONDE HAY COLISION, y esto no es una optimizacion:
    // es lo que evita cambiar una cadena que YA ES PUBLICA.
    //
    // La primera version de este arreglo metia la cuenta de parametros en la clave SIEMPRE, y esa
    // clave sale del compilador por tres sitios: el volcado de HIR, el de MIR y los METADATOS DE
    // BIBLIOTECA que lee un host. Cuatro pruebas se pusieron rojas y tenian razon: cambiar el
    // formato de una identidad publica por un problema interno rompe a quien la lea.
    //
    // Asi que la clave de un metodo generico se queda EXACTAMENTE como estaba mientras sea unica.
    // Solo cuando dos plantillas comparten `nombre``aridad` se les anade `#<parametros de valor>`
    // a las dos, que es el unico caso donde antes habia un error y por tanto el unico donde no
    // puede haber nada escrito que dependa del formato anterior.
    {
        std::unordered_map<std::string, std::size_t> baseCounts;
        for (const TemplateState& state : templateStates_)
        {
            if (state.record.kind != GenericTemplateKind::Method) continue;
            ++baseCounts[state.record.canonicalKey];
        }
        // EL DESEMPATE ES LA POSICION DE DECLARACION, no la cuenta de parametros.
        //
        // La cuenta separaba `F<T>(T[])` de `F<T>(T[], int, int)`, pero NO separa `F<T>(T)` de
        // `F<T>(T[])` -- las dos toman uno. Y si dos plantillas comparten clave, comparten la
        // clave de especializacion, asi que la segunda daria en la cache de la primera y una de
        // las dos sobrecargas DESAPARECERIA en silencio. Ese es exactamente el fallo que este
        // proyecto persigue: no un rojo, sino un verde que mide otra cosa.
        //
        // El indice de declaracion es unico por construccion y determinista (orden de fuente).
        std::size_t index = 0;
        for (TemplateState& state : templateStates_)
        {
            const std::size_t position = index++;
            if (state.record.kind != GenericTemplateKind::Method) continue;
            const auto found = baseCounts.find(state.record.canonicalKey);
            if (found == baseCounts.end() || found->second < 2) continue;
            state.record.canonicalKey += "#" + std::to_string(position);
        }
    }

    // Record the declaration-level dependency graph before any template is cloned. A producer
    // can then publish exactly the fixed-point closure of its public roots, and the NSMETA
    // verifier can prove that a same-module edge was not silently dropped.
    for (const std::unique_ptr<ProjectSyntaxUnit>& unitPtr : units)
    {
        if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
        discoverTemplateDependencies(
            *unitPtr, unitPtr->syntaxTree(), "", "", GenericLookupScope{});
    }
    for (TemplateState& state : templateStates_)
    {
        std::sort(state.record.dependencies.begin(), state.record.dependencies.end());
        state.record.dependencies.erase(
            std::unique(state.record.dependencies.begin(), state.record.dependencies.end()),
            state.record.dependencies.end());
        std::sort(
            state.record.unresolvedDependencies.begin(),
            state.record.unresolvedDependencies.end());
        state.record.unresolvedDependencies.erase(
            std::unique(
                state.record.unresolvedDependencies.begin(),
                state.record.unresolvedDependencies.end()),
            state.record.unresolvedDependencies.end());
    }

    // LA DETECCION DE DUPLICADOS ES SOLO PARA TIPOS, desde el 2026-09-03.
    //
    // Dos plantillas de METODO con el mismo nombre y la misma aridad generica YA NO son un error:
    // son sobrecargas, y se materializan todas con un nombre compartido para que la resolucion
    // ordinaria elija por tipos (ver processGenericCall). Si despues de sustituir acaban con la
    // MISMA lista de parametros --un duplicado de verdad-- lo detecta la deteccion de metodos
    // duplicados del modelo de miembros, que es donde se comparan listas de parametros.
    //
    // Para clases, structs y delegados no cambia nada: dos con el mismo nombre y aridad siguen
    // siendo un choque, porque ahi no hay lista de parametros que los distinga.
    std::unordered_set<std::string> templateKeys;
    for (const TemplateState& state : templateStates_)
    {
        if (state.record.kind == GenericTemplateKind::Method) continue;
        if (!templateKeys.insert(state.record.canonicalKey).second)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                state.syntax != nullptr ? state.syntax->range() : SourceRange{},
                // Este bucle salta los METODOS --la linea de arriba-- asi que aqui solo llegan
                // TIPOS genericos, y el mensaje tiene que hablar de tipos. Decia «dos metodos
                // genericos», que era el texto del caso vecino: un tipo generico no tiene
                // parametros por los que desambiguar, de modo que dos con el mismo nombre, el
                // mismo namespace y la misma aridad son sencillamente el mismo tipo dos veces.
                "duplicate generic type identity '" + state.record.canonicalKey +
                    "'; two generic types in the same namespace cannot share a name and an "
                    "arity -- rename one, or declare it 'partial' at namespace scope");
        }
    }

    templateRecords_.reserve(templateStates_.size());
    for (const TemplateState& state : templateStates_) templateRecords_.push_back(state.record);

    // Seed only identities whose public template was actually hydrated. An old metadata file may
    // list historical specializations without carrying a reusable template body; such a record
    // must never make an unavailable generic appear usable.
    for (const PrecompiledGenericSpecialization& imported : precompiledSpecializations)
    {
        if (!imported.hasConcreteDeclaration) continue;
        const auto templateState = std::ranges::find_if(templateStates_, [&](const auto& state) {
            return state.record.canonicalKey == imported.templateKey;
        });
        if (templateState == templateStates_.end() ||
            specializationByKey_.contains(imported.specializationKey))
        {
            continue;
        }
        const std::size_t ordinal = specializations_.size();
        specializationByKey_.emplace(imported.specializationKey, ordinal);
        specializations_.push_back(GenericSpecializationRecord{
            imported.kind,
            templateState->syntax,
            nullptr,
            imported.templateKey,
            imported.specializationKey,
            imported.specializedName,
            imported.typeArguments,
            ordinal,
            templateState->ordinal,
            nullptr,
            SourceRange{},
            0});
    }

    for (const std::unique_ptr<ProjectSyntaxUnit>& unitPtr : units)
    {
        if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
        ProjectSyntaxUnit& unit = *unitPtr;
        processNode(unit, unit.syntaxTree(), "", "", GenericLookupScope{}, false, diagnostics);
    }

    built_ = true;
    return !diagnostics.hasErrors();
}

ast::TypeSyntax* ProjectGenericModel::cloneAliasType(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    bool& ownershipConflict)
{
    if (source == nullptr) return nullptr;

    ast::TypeSyntax* result = nullptr;
    const auto substitution = substitutions.find(source->baseName());
    if (substitution != substitutions.end() && source->typeArguments().empty())
    {
        result = cloneAliasType(arena, substitution->second, {}, ownershipConflict);
    }
    else
    {
        result = arena.make<ast::TypeSyntax>(
            source->range(), source->baseTokenKind(), source->baseName());
        for (const ast::TypeSyntax* argument : source->typeArguments())
            result->addTypeArgument(
                cloneAliasType(arena, argument, substitutions, ownershipConflict));
        if (source->originProjectOrdinal().has_value())
            result->setOriginProjectOrdinal(source->originProjectOrdinal().value());
        if (source->originNamespace().has_value())
            result->setOriginNamespace(source->originNamespace().value());
    }

    if (source->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None)
    {
        if (result->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None &&
            result->ownershipQualifier() != source->ownershipQualifier())
            ownershipConflict = true;
        else
            result->setOwnershipQualifier(source->ownershipQualifier());
    }
    for (const ast::TypeModifierSyntax& modifier : source->modifiers())
    {
        // Array extents are immutable expression syntax owned by the same source unit.  Alias
        // substitution changes types, never value parameters, so sharing this pointer is exact.
        result->addModifier(ast::TypeModifierSyntax{
            modifier.kind, modifier.arrayCount, modifier.range});
    }
    return result;
}

bool ProjectGenericModel::expandTypeAliases(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    std::vector<std::string>& expansionStack)
{
    if (syntax == nullptr) return true;
    bool ok = true;
    for (ast::TypeSyntax* argument : syntax->typeArguments())
        ok = expandTypeAliases(unit, argument, scope, diagnostics, expansionStack) && ok;

    if (lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) ||
        scope.typeParameters.contains(syntax->baseName()))
        return ok;

    const auto found = scope.typeAliases.find(syntax->baseName());
    if (found == scope.typeAliases.end() || found->second == nullptr) return ok;
    const ast::AstNode* declaration = found->second;

    if (std::find(expansionStack.begin(), expansionStack.end(), syntax->baseName()) !=
        expansionStack.end())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidUsingTarget,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "using alias '" + syntax->baseName() + "' expands cyclically");
        return false;
    }

    if (syntax->typeArguments().size() != declaration->genericParameters().size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "type alias '" + declaration->text() + "' expects " +
                std::to_string(declaration->genericParameters().size()) +
                " type argument(s) but received " +
                std::to_string(syntax->typeArguments().size()));
        return false;
    }

    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    for (std::size_t index = 0; index < declaration->genericParameters().size(); ++index)
        substitutions.emplace(declaration->genericParameters()[index], syntax->typeArguments()[index]);

    expansionStack.push_back(syntax->baseName());
    bool targetOwnershipConflict = false;
    ast::TypeSyntax* expanded =
        cloneAliasType(
            unit.arena(), declaration->typeSyntax(), substitutions, targetOwnershipConflict);
    if (targetOwnershipConflict)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "type alias '" + declaration->text() +
                "' applies a conflicting ownership qualifier to a type argument");
        expansionStack.pop_back();
        return false;
    }
    ok = expandTypeAliases(unit, expanded, scope, diagnostics, expansionStack) && ok;
    expansionStack.pop_back();
    if (!ok || expanded == nullptr) return false;

    const ast::OwnershipQualifierSyntaxKind writtenOwnership = syntax->ownershipQualifier();
    const ast::OwnershipQualifierSyntaxKind targetOwnership = expanded->ownershipQualifier();
    if (writtenOwnership != ast::OwnershipQualifierSyntaxKind::None &&
        targetOwnership != ast::OwnershipQualifierSyntaxKind::None &&
        writtenOwnership != targetOwnership)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "type alias '" + declaration->text() +
                "' conflicts with the ownership qualifier written at its use site");
        return false;
    }

    syntax->setBaseTokenKind(expanded->baseTokenKind());
    syntax->setBaseName(expanded->baseName());
    if (writtenOwnership == ast::OwnershipQualifierSyntaxKind::None)
        syntax->setOwnershipQualifier(targetOwnership);
    const std::vector<ast::TypeSyntax*> expandedArguments(
        expanded->typeArguments().begin(), expanded->typeArguments().end());
    syntax->clearTypeArguments();
    for (ast::TypeSyntax* argument : expandedArguments) syntax->addTypeArgument(argument);
    // Target/substituted modifiers are inside modifiers written on the alias use.  For example,
    // `using P<T> = T*; P<int>[]` becomes `int*[]`.
    syntax->prependModifiers(expanded->modifiers());
    if (expanded->originProjectOrdinal().has_value())
        syntax->setOriginProjectOrdinal(expanded->originProjectOrdinal().value());
    if (expanded->originNamespace().has_value())
        syntax->setOriginNamespace(expanded->originNamespace().value());
    return true;
}

bool ProjectGenericModel::expandAliasesInNode(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& namespaceName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (node == nullptr) return true;
    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(namespaceName, node->text());

    GenericLookupScope currentScope = scope;
    for (const std::string& parameter : node->genericParameters())
        currentScope.typeParameters.insert(parameter);
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr || child->kind() != ast::AstNodeKind::UsingAliasDirective) continue;
        if (!child->genericParameters().empty() || child->auxiliaryText().empty())
            currentScope.typeAliases[child->text()] = child;
    }

    bool ok = true;
    // Normalize every alias target in its declaration scope first.  This makes a chained alias
    // immune to a same-named alias that may shadow it only at a later use site.
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr || child->kind() != ast::AstNodeKind::UsingAliasDirective ||
            child->typeSyntax() == nullptr ||
            (child->genericParameters().empty() && !child->auxiliaryText().empty()))
            continue;
        GenericLookupScope aliasScope = currentScope;
        for (const std::string& parameter : child->genericParameters())
            aliasScope.typeParameters.insert(parameter);
        std::vector<std::string> stack{child->text()};
        ok = expandTypeAliases(
                 unit, child->typeSyntax(), aliasScope, diagnostics, stack) && ok;
    }

    if (node->kind() != ast::AstNodeKind::UsingAliasDirective)
    {
        std::vector<std::string> stack;
        ok = expandTypeAliases(unit, node->typeSyntax(), currentScope, diagnostics, stack) && ok;
        for (ast::TypeSyntax* syntax : node->typeSyntaxes())
        {
            stack.clear();
            ok = expandTypeAliases(unit, syntax, currentScope, diagnostics, stack) && ok;
        }
        for (const ast::GenericConstraintClause& clause : node->genericConstraints())
        {
            for (ast::TypeSyntax* constraint : clause.typeConstraints)
            {
                stack.clear();
                ok = expandTypeAliases(unit, constraint, currentScope, diagnostics, stack) && ok;
            }
        }
    }

    ok = expandAliasesInNode(unit, node->first(), currentNamespace, currentScope, diagnostics) && ok;
    ok = expandAliasesInNode(unit, node->second(), currentNamespace, currentScope, diagnostics) && ok;
    ok = expandAliasesInNode(unit, node->third(), currentNamespace, currentScope, diagnostics) && ok;
    for (ast::AstNode* child : node->children())
        ok = expandAliasesInNode(unit, child, currentNamespace, currentScope, diagnostics) && ok;
    return ok;
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

    // NSC-LIMIT-022. Generic specialization happens before ProjectNameLookup exists, but its
    // cache key still has to identify the DECLARATION of an argument, not the project in which
    // that argument happened to be written. Record ordinary named types now, from the same
    // already-parsed graph. Generic declarations are represented by TemplateState and nested
    // declarations inside a generic type are not predeclared by the later name model either.
    if (!insideGenericType && isNamedTypeDeclaration(node->kind()) &&
        node->genericParameters().empty())
    {
        const std::string qualifiedTypeName = appendOwner(ownerName, node->text());
        std::string sourceName = qualifiedName(currentNamespace, qualifiedTypeName);
        std::replace(sourceName.begin(), sourceName.end(), '+', '.');
        declaredTypeIdentities_.push_back(DeclaredTypeIdentity{
            unit.projectOrdinal(),
            currentNamespace,
            std::move(sourceName),
            std::string(unit.moduleName()) + "::" +
                qualifiedName(currentNamespace, qualifiedTypeName),
            node->tokenKind() == lexer::TokenKind::KwPublic});
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
        record.accessibility = genericTemplateAccessibility(node, false);
        record.syntax = node;
        record.projectOrdinal = unit.projectOrdinal();
        record.sourceOrdinal = unit.sourceOrdinal();
        record.moduleName = std::string(unit.moduleName());
        record.namespaceName = currentNamespace;
        record.name = node->text();
        record.ownerName = ownerName;
        record.parameters = node->genericParameters();
        record.constraints = node->genericConstraints();
        // NSC-LIMIT-009: `ns.A+Inner`1`, no `ns.Inner`1`. Hoy el parser rechaza un tipo anidado
        // generico, de modo que esta cadena esta siempre vacia; se escribe igualmente porque el
        // dia que deje de rechazarlo la clave tiene que salir completa, no ambigua.
        record.qualifiedTypeName = appendOwner(ownerName, record.name);
        record.canonicalKey = record.moduleName + "::" +
            qualifiedName(currentNamespace, record.qualifiedTypeName) +
            "`" + std::to_string(record.parameters.size());
        templateStates_.push_back(TemplateState{record, &unit, node, parent, templateStates_.size()});
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
        record.accessibility = genericTemplateAccessibility(node, true);
        record.syntax = node;
        record.projectOrdinal = unit.projectOrdinal();
        record.sourceOrdinal = unit.sourceOrdinal();
        record.moduleName = std::string(unit.moduleName());
        record.namespaceName = currentNamespace;
        record.ownerName = ownerName;
        record.name = node->text();
        record.parameters = node->genericParameters();
        record.constraints = node->genericConstraints();
        record.qualifiedTypeName = ownerName;
        record.canonicalKey = record.moduleName + "::" + qualifiedName(currentNamespace, ownerName) +
            "." + record.name + "``" + std::to_string(record.parameters.size());
        templateStates_.push_back(TemplateState{record, &unit, node, parent, templateStates_.size()});
    }

    // NSC-LIMIT-009: se APILA. La version anterior reemplazaba, de modo que al entrar en
    // `A { Inner { ... } }` el dueno pasaba a ser «Inner» y `A` desaparecia.
    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration || node->kind() == ast::AstNodeKind::StructDeclaration)
            ? appendOwner(ownerName, node->text()) : ownerName;
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
    const GenericLookupScope& scope,
    bool insideTemplate,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (node == nullptr) return;

    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        currentNamespace = namespaceAppend(namespaceName, node->text());
    }

    // LOS `using` DE ESTE AMBITO, recogidos como los recoge ProjectNameLookup::bindScope: del
    // mismo AST, en el mismo sitio, y heredando los de fuera.
    GenericLookupScope currentScope = scope;
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr) continue;
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
        {
            // LAS DOS FORMAS. `using Otro;` dentro de `namespace A` puede significar `Otro` o
            // `A.Otro`, y aqui no hay tabla de namespaces con la que decidirlo --el modelo de
            // nombres todavia no existe--. Se guardan las dos: el escalon de imports las prueba
            // ambas, y la que no exista sencillamente no acierta.
            const std::string written = child->text();
            if (written.empty()) continue;
            const auto add = [&](std::string candidate)
            {
                if (std::find(
                        currentScope.importedNamespaces.begin(),
                        currentScope.importedNamespaces.end(),
                        candidate) == currentScope.importedNamespaces.end())
                {
                    currentScope.importedNamespaces.push_back(std::move(candidate));
                }
            };
            const std::vector<std::string> segments = splitDotted(currentNamespace);
            for (std::size_t depth = segments.size(); depth > 0; --depth)
            {
                std::string prefixed;
                for (std::size_t index = 0; index < depth; ++index)
                {
                    if (!prefixed.empty()) prefixed.push_back('.');
                    prefixed += segments[index];
                }
                prefixed.push_back('.');
                prefixed += written;
                add(std::move(prefixed));
            }
            add(written);
        }
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective)
        {
            if (!child->auxiliaryText().empty())
            {
                currentScope.aliases[child->text()] = child->auxiliaryText();
            }
        }
    }

    // Parameters and fields are visible throughout their containing method/type regardless of
    // textual order.  Snapshot their source spelling before processTypeSyntax replaces closed
    // generic uses with generated physical names; inference needs the original structure.
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr || child->typeSyntax() == nullptr ||
            (child->kind() != ast::AstNodeKind::ParameterDeclaration &&
             child->kind() != ast::AstNodeKind::FieldDeclaration))
            continue;
        bool conflict = false;
        currentScope.valueTypes[child->text()] =
            cloneAliasType(unit.arena(), child->typeSyntax(), {}, conflict);
    }

    // The declaration is a compile-time rewrite rule, not a use of its still-open target.  Its
    // normalized target was handled by expandAliasesInNode; materializing it here would create
    // e.g. Box<T> and leak the alias parameter T into the concrete program.
    if (node->kind() == ast::AstNodeKind::UsingAliasDirective) return;

    const bool templateNode = isGenericTypeTemplateNode(node) || isGenericMethodTemplateNode(node);
    // Constraint types are declaration dependencies, not executable template-body syntax. Closed
    // constraints such as `where T : IValue<int>` must be materialized even while the remaining
    // body is deferred until a concrete specialization exists.
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            processTypeSyntax(
                unit, const_cast<ast::TypeSyntax*>(constraint), currentNamespace, currentScope,
                diagnostics);
        processTypeSyntax(
            unit, const_cast<ast::TypeSyntax*>(clause.substitutedArgument), currentNamespace,
            currentScope, diagnostics);
    }
    if (templateNode || insideTemplate) return;

    if (node->typeSyntax() != nullptr)
    {
        processTypeSyntax(unit, node->typeSyntax(), currentNamespace, currentScope, diagnostics);
    }

    if (node->kind() == ast::AstNodeKind::CallExpression && node->typeSyntaxes().empty())
    {
        inferGenericCall(
            unit, node, currentNamespace, ownerName, currentScope, diagnostics);
    }
    if (node->kind() == ast::AstNodeKind::CallExpression && !node->typeSyntaxes().empty())
    {
        processGenericCall(unit, node, currentNamespace, ownerName, currentScope, diagnostics);
    }
    else
    {
        for (ast::TypeSyntax* syntax : node->typeSyntaxes())
        {
            processTypeSyntax(unit, syntax, currentNamespace, currentScope, diagnostics);
        }
    }
    const auto processChild = [&](ast::AstNode* child)
    {
        processNode(unit, child, currentNamespace, ownerName, currentScope, false, diagnostics);
    };
    processChild(node->first());
    processChild(node->second());
    processChild(node->third());

    // NSC-LIMIT-009: se APILA. La version anterior reemplazaba, de modo que al entrar en
    // `A { Inner { ... } }` el dueno pasaba a ser «Inner» y `A` desaparecia.
    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration || node->kind() == ast::AstNodeKind::StructDeclaration)
            ? appendOwner(ownerName, node->text()) : ownerName;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    GenericLookupScope sequentialScope = currentScope;
    for (ast::AstNode* child : children)
    {
        ast::TypeSyntax* declaredLocalType = nullptr;
        if (child != nullptr && child->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
            child->typeSyntax() != nullptr &&
            child->typeSyntax()->baseTokenKind() != lexer::TokenKind::KwVar)
        {
            bool conflict = false;
            declaredLocalType =
                cloneAliasType(unit.arena(), child->typeSyntax(), {}, conflict);
        }
        else if (child != nullptr &&
                 child->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
                 child->first() != nullptr)
        {
            // Capture `var` from the initializer before its closed generic syntax is rewritten to
            // a physical specialization name; structural inference still needs Box<int>, not the
            // generated Box__g1__... spelling.
            declaredLocalType = inferExpressionType(unit, child->first(), sequentialScope);
        }
        processNode(unit, child, currentNamespace, childOwner, sequentialScope, false, diagnostics);
        if (child == nullptr || child->kind() != ast::AstNodeKind::VariableDeclarationStatement)
            continue;
        if (declaredLocalType == nullptr && child->first() != nullptr)
            declaredLocalType = inferExpressionType(unit, child->first(), sequentialScope);
        if (declaredLocalType != nullptr)
            sequentialScope.valueTypes[child->text()] = declaredLocalType;
    }
}

bool ProjectGenericModel::processTypeSyntax(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    const std::string& namespaceName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (syntax == nullptr) return true;
    for (ast::TypeSyntax* argument : syntax->typeArguments())
    {
        if (!processTypeSyntax(unit, argument, namespaceName, scope, diagnostics)) return false;
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
                "type '" + syntax->baseName() +
                    "' does not name a unique visible generic class/struct template");
            return false;
        }
        if (const DeclaredTypeIdentity* declaration = resolveDeclaredTypeIdentity(
                syntax, unit.projectOrdinal(), namespaceName, scope);
            declaration != nullptr)
        {
            std::string canonical = "type:" + declaration->identityKey;
            appendTypeModifiers(canonical, syntax);
            resolvedTypeIdentityBySyntax_[syntax] = std::move(canonical);
            resolvedTypeOriginBySyntax_[syntax] = DeclaredTypeOrigin{
                declaration->projectOrdinal, declaration->namespaceName};
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
    // NSC-LIMIT-007: el SITIO. Un `Caja<int>` invalido tiene que sonar donde alguien lo escribio,
    // no sobre la declaracion de `Caja<T>`, que es correcta.
    currentInstantiationRange_ = syntax->range();
    const std::string specialized = ensureTypeSpecialization(
        *state, syntax->typeArguments(), diagnostics);
    if (specialized.empty()) return false;

    // Keep the closed template's canonical key on this exact syntax node before erasing its
    // arguments. An outer specialization can then use that declaration-derived identity instead
    // of reinterpreting the generated name under a caller project.
    for (auto iterator = specializations_.rbegin(); iterator != specializations_.rend(); ++iterator)
    {
        if (iterator->specializedName != specialized) continue;
        std::string canonical = "generic:" + iterator->specializationKey;
        appendTypeModifiers(canonical, syntax);
        resolvedTypeIdentityBySyntax_[syntax] = std::move(canonical);
        // The generated declaration belongs beside its template, not beside the source site that
        // requested the closed type. Preserve that declaration origin before the readable
        // `Box<int>` spelling is replaced with `Box__g1__...`. A later substitution (for example
        // `Reader<T> where T : IValue<int>` instantiated as `Reader<Box<int>>`) can then clone the
        // already-closed argument without retrying its physical name in the consumer module.
        resolvedTypeOriginBySyntax_[syntax] = DeclaredTypeOrigin{
            state->record.projectOrdinal, state->record.namespaceName};
        syntax->setOriginProjectOrdinal(state->record.projectOrdinal);
        syntax->setOriginNamespace(state->record.namespaceName);
        break;
    }
    // Preserve an explicitly written qualifier. Replacing `Lib.Box<int>` with only the generated
    // terminal name made name lookup retry it in the caller module, even though the specialization
    // itself was correctly materialized in Lib. The monomorphized name replaces `Box`, not `Lib`.
    const std::string originalBaseName = syntax->baseName();
    if (const std::size_t separator = originalBaseName.find_last_of('.');
        separator != std::string::npos)
    {
        syntax->setBaseName(originalBaseName.substr(0, separator + 1) + specialized);
    }
    else
    {
        syntax->setBaseName(specialized);
    }
    syntax->clearTypeArguments();
    return true;
}

ast::TypeSyntax* ProjectGenericModel::inferExpressionType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression,
    const GenericLookupScope& scope)
{
    if (expression == nullptr) return nullptr;
    const auto builtin = [&](lexer::TokenKind kind, std::string name)
    {
        return unit.arena().make<ast::TypeSyntax>(expression->range(), kind, std::move(name));
    };
    const auto clone = [&](const ast::TypeSyntax* source)
    {
        bool conflict = false;
        return cloneAliasType(unit.arena(), source, {}, conflict);
    };

    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
        {
            const auto found = scope.valueTypes.find(expression->text());
            return found == scope.valueTypes.end() ? nullptr : clone(found->second);
        }
        case ast::AstNodeKind::MemberExpression:
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::ThisExpression)
            {
                const auto found = scope.valueTypes.find(expression->text());
                return found == scope.valueTypes.end() ? nullptr : clone(found->second);
            }
            return nullptr;
        case ast::AstNodeKind::IntegerLiteralExpression:
        {
            const std::string key(builtins::integerLiteralTypeKey(
                expression->text(), expression->integerValue(), false));
            if (key == "builtin:uint") return builtin(lexer::TokenKind::KwUInt, "uint");
            if (key == "builtin:long") return builtin(lexer::TokenKind::KwLong, "long");
            if (key == "builtin:ulong") return builtin(lexer::TokenKind::KwULong, "ulong");
            return builtin(lexer::TokenKind::KwInt, "int");
        }
        case ast::AstNodeKind::FloatingLiteralExpression:
            return !expression->text().empty() &&
                    (expression->text().back() == 'f' || expression->text().back() == 'F')
                ? builtin(lexer::TokenKind::KwFloat, "float")
                : builtin(lexer::TokenKind::KwDouble, "double");
        case ast::AstNodeKind::CharacterLiteralExpression:
            return builtin(lexer::TokenKind::KwChar, "char");
        case ast::AstNodeKind::StringLiteralExpression:
            return builtin(lexer::TokenKind::KwString, "string");
        case ast::AstNodeKind::BooleanLiteralExpression:
            return builtin(lexer::TokenKind::KwBool, "bool");
        case ast::AstNodeKind::CastExpression:
        case ast::AstNodeKind::SafeCastExpression:
        case ast::AstNodeKind::DefaultValueExpression:
        case ast::AstNodeKind::ObjectCreationExpression:
            return clone(expression->typeSyntax());
        case ast::AstNodeKind::NewExpression:
        {
            ast::TypeSyntax* result = clone(expression->typeSyntax());
            if (result == nullptr) return nullptr;
            result->addModifier(ast::TypeModifierSyntax{
                expression->flag() ? ast::TypeModifierSyntaxKind::DynamicArray
                                   : ast::TypeModifierSyntaxKind::Pointer,
                nullptr,
                expression->range()});
            return result;
        }
        case ast::AstNodeKind::StackallocExpression:
        {
            ast::TypeSyntax* result = clone(expression->typeSyntax());
            if (result != nullptr)
                result->addModifier(ast::TypeModifierSyntax{
                    ast::TypeModifierSyntaxKind::Pointer, nullptr, expression->range()});
            return result;
        }
        case ast::AstNodeKind::MoveExpression:
            return inferExpressionType(unit, expression->first(), scope);
        case ast::AstNodeKind::UnaryExpression:
        {
            ast::TypeSyntax* operand = inferExpressionType(unit, expression->first(), scope);
            if (operand == nullptr) return nullptr;
            if (expression->tokenKind() == lexer::TokenKind::Bang)
                return builtin(lexer::TokenKind::KwBool, "bool");
            if (expression->tokenKind() == lexer::TokenKind::Amp)
            {
                operand->addModifier(ast::TypeModifierSyntax{
                    ast::TypeModifierSyntaxKind::Pointer, nullptr, expression->range()});
                return operand;
            }
            if (expression->tokenKind() != lexer::TokenKind::Star ||
                operand->modifiers().empty() ||
                operand->modifiers().back().kind != ast::TypeModifierSyntaxKind::Pointer)
                return operand;

            ast::TypeSyntax* result = unit.arena().make<ast::TypeSyntax>(
                operand->range(), operand->baseTokenKind(), operand->baseName());
            result->setOwnershipQualifier(operand->ownershipQualifier());
            for (ast::TypeSyntax* argument : operand->typeArguments())
                result->addTypeArgument(clone(argument));
            for (std::size_t index = 0; index + 1 < operand->modifiers().size(); ++index)
                result->addModifier(operand->modifiers()[index]);
            return result;
        }
        case ast::AstNodeKind::AssignmentExpression:
            return inferExpressionType(unit, expression->second(), scope);
        case ast::AstNodeKind::ConditionalExpression:
        {
            ast::TypeSyntax* whenTrue = inferExpressionType(unit, expression->second(), scope);
            ast::TypeSyntax* whenFalse = inferExpressionType(unit, expression->third(), scope);
            if (whenTrue == nullptr || whenFalse == nullptr ||
                typeSpelling(whenTrue) != typeSpelling(whenFalse)) return nullptr;
            return whenTrue;
        }
        case ast::AstNodeKind::BinaryExpression:
        {
            switch (expression->tokenKind())
            {
                case lexer::TokenKind::EqualEqual:
                case lexer::TokenKind::BangEqual:
                case lexer::TokenKind::Less:
                case lexer::TokenKind::LessEqual:
                case lexer::TokenKind::Greater:
                case lexer::TokenKind::GreaterEqual:
                case lexer::TokenKind::AmpAmp:
                case lexer::TokenKind::PipePipe:
                    return builtin(lexer::TokenKind::KwBool, "bool");
                default: break;
            }
            ast::TypeSyntax* left = inferExpressionType(unit, expression->first(), scope);
            ast::TypeSyntax* right = inferExpressionType(unit, expression->second(), scope);
            if (left == nullptr || right == nullptr || typeSpelling(left) != typeSpelling(right))
                return nullptr;
            return left;
        }
        case ast::AstNodeKind::CallExpression:
        {
            const auto found = inferredCallReturnTypes_.find(expression);
            return found == inferredCallReturnTypes_.end() ? nullptr : clone(found->second);
        }
        default:
            return nullptr;
    }
}

bool ProjectGenericModel::inferTypeArguments(
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* pattern,
    const ast::TypeSyntax* actual,
    const std::unordered_set<std::string>& parameters,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred)
{
    if (pattern == nullptr || actual == nullptr) return false;
    if (actual->modifiers().size() < pattern->modifiers().size()) return false;
    const std::size_t modifierPrefix = actual->modifiers().size() - pattern->modifiers().size();
    for (std::size_t index = 0; index < pattern->modifiers().size(); ++index)
    {
        if (pattern->modifiers()[index].kind != actual->modifiers()[modifierPrefix + index].kind)
            return false;
    }

    if (parameters.contains(pattern->baseName()) && pattern->typeArguments().empty())
    {
        ast::TypeSyntax* value = unit.arena().make<ast::TypeSyntax>(
            actual->range(), actual->baseTokenKind(), actual->baseName());
        if (pattern->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::None)
            value->setOwnershipQualifier(actual->ownershipQualifier());
        bool conflict = false;
        for (const ast::TypeSyntax* argument : actual->typeArguments())
            value->addTypeArgument(cloneAliasType(unit.arena(), argument, {}, conflict));
        for (std::size_t index = 0; index < modifierPrefix; ++index)
            value->addModifier(actual->modifiers()[index]);
        const auto prior = inferred.find(pattern->baseName());
        if (prior == inferred.end())
        {
            inferred.emplace(pattern->baseName(), value);
            return true;
        }
        return typeSpelling(prior->second) == typeSpelling(value);
    }

    if (modifierPrefix != 0 || pattern->baseTokenKind() != actual->baseTokenKind() ||
        pattern->baseName() != actual->baseName() ||
        pattern->typeArguments().size() != actual->typeArguments().size())
        return false;
    for (std::size_t index = 0; index < pattern->typeArguments().size(); ++index)
    {
        if (!inferTypeArguments(
                unit, pattern->typeArguments()[index], actual->typeArguments()[index],
                parameters, inferred)) return false;
    }
    return true;
}

bool ProjectGenericModel::inferGenericCall(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (call == nullptr || call->kind() != ast::AstNodeKind::CallExpression ||
        !call->typeSyntaxes().empty()) return true;
    ast::AstNode* callee = methodCalleeSyntax(call);
    const std::string methodName = methodCalleeName(call);
    if (callee == nullptr || methodName.empty()) return true;

    std::string requestedOwner = templateSourceSpelling(namespaceName, ownerName);
    if (callee->kind() == ast::AstNodeKind::MemberExpression)
    {
        const std::string written = receiverSourceSpelling(callee->first());
        if (!written.empty()) requestedOwner = written;
    }
    const std::vector<std::string> namespaceSegments = splitDotted(namespaceName);
    const std::vector<std::string> enclosingTypes =
        enclosingTypeSpellings(namespaceName, ownerName);
    QualifiedNameSite site;
    site.namespaceSegments = namespaceSegments;
    site.enclosingTypeNames = enclosingTypes;
    site.aliases = &scope.aliases;
    site.importedNamespaces = scope.importedNamespaces;
    constexpr std::size_t kAny = std::numeric_limits<std::size_t>::max();
    const std::vector<TemplateState*> named = templatesNamedBy(
        unit, site, requestedOwner, methodName, kAny, kAny);
    if (named.empty()) return true;

    // Infer inner calls first so `Identity(Identity(42))` has a concrete argument type at the
    // outer site.  The later ordinary AST walk may visit the inner node again, but its callee has
    // already been rewritten to a non-template specialization and the cache prevents duplication.
    for (ast::AstNode* argument : call->children())
    {
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::CallExpression ||
            !argument->typeSyntaxes().empty()) continue;
        inferGenericCall(unit, argument, namespaceName, ownerName, scope, diagnostics);
        if (!argument->typeSyntaxes().empty())
            processGenericCall(unit, argument, namespaceName, ownerName, scope, diagnostics);
    }

    // A non-generic overload in the same owner remains an ordinary overload-resolution problem.
    // This early phase cannot compare its conversions with a synthesized generic candidate, so it
    // leaves the call untouched; explicit `<...>` remains available when the generic is intended.
    for (const TemplateState* candidate : named)
    {
        if (candidate == nullptr || candidate->parent == nullptr) continue;
        for (const ast::AstNode* sibling : candidate->parent->children())
        {
            if (sibling != nullptr && sibling->kind() == ast::AstNodeKind::MethodDeclaration &&
                sibling->genericParameters().empty() && sibling->text() == methodName &&
                valueParameterCount(sibling) == call->children().size())
                return true;
        }
    }

    struct Inference final
    {
        TemplateState* candidate = nullptr;
        std::vector<ast::TypeSyntax*> arguments;
        std::size_t specificity = 0;
    };
    const auto patternSpecificity = [&](auto&& self, const ast::TypeSyntax* pattern,
                                        const std::unordered_set<std::string>& parameters)
        -> std::size_t
    {
        if (pattern == nullptr) return 0;
        std::size_t score = parameters.contains(pattern->baseName()) &&
                pattern->typeArguments().empty()
            ? 0U : 4U;
        score += pattern->modifiers().size() * 2U;
        for (const ast::TypeSyntax* argument : pattern->typeArguments())
            score += self(self, argument, parameters);
        return score;
    };
    std::vector<Inference> successes;
    for (TemplateState* candidate : named)
    {
        if (candidate == nullptr || candidate->syntax == nullptr) continue;
        std::vector<const ast::AstNode*> parameters;
        for (const ast::AstNode* child : candidate->syntax->children())
            if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration)
                parameters.push_back(child);

        std::vector<std::optional<std::size_t>> mapping(call->children().size());
        std::vector<bool> used(parameters.size(), false);
        std::size_t nextPositional = 0;
        bool viable = true;
        for (std::size_t argumentIndex = 0; argumentIndex < call->children().size(); ++argumentIndex)
        {
            const ast::AstNode* argument = call->children()[argumentIndex];
            std::optional<std::size_t> parameterIndex;
            if (argument != nullptr && !argument->argumentName().empty())
            {
                for (std::size_t index = 0; index < parameters.size(); ++index)
                    if (parameters[index]->text() == argument->argumentName())
                    {
                        parameterIndex = index;
                        break;
                    }
            }
            else
            {
                while (nextPositional < used.size() && used[nextPositional]) ++nextPositional;
                if (nextPositional < used.size()) parameterIndex = nextPositional++;
            }
            if (!parameterIndex.has_value() || used[*parameterIndex])
            {
                viable = false;
                break;
            }
            used[*parameterIndex] = true;
            mapping[argumentIndex] = parameterIndex;
        }
        for (std::size_t index = 0; viable && index < parameters.size(); ++index)
            if (!used[index] && parameters[index]->first() == nullptr) viable = false;
        if (!viable) continue;

        const std::unordered_set<std::string> genericParameters(
            candidate->record.parameters.begin(), candidate->record.parameters.end());
        std::unordered_map<std::string, ast::TypeSyntax*> inferred;
        for (std::size_t index = 0; viable && index < call->children().size(); ++index)
        {
            ast::TypeSyntax* actual = inferExpressionType(unit, call->children()[index], scope);
            const ast::TypeSyntax* pattern = parameters[*mapping[index]]->typeSyntax();
            viable = inferTypeArguments(
                unit, pattern, actual, genericParameters, inferred);
        }
        if (!viable || inferred.size() != candidate->record.parameters.size()) continue;
        Inference success;
        success.candidate = candidate;
        for (const ast::AstNode* parameter : parameters)
            success.specificity +=
                patternSpecificity(patternSpecificity, parameter->typeSyntax(), genericParameters);
        for (const std::string& parameter : candidate->record.parameters)
            success.arguments.push_back(inferred.at(parameter));
        successes.push_back(std::move(success));
    }

    if (successes.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            "type arguments for generic method '" + methodName +
                "' cannot be inferred from the local call arguments; write them explicitly");
        return false;
    }
    const std::size_t bestSpecificity = std::ranges::max_element(
        successes, {}, &Inference::specificity)->specificity;
    std::erase_if(successes, [&](const Inference& inference)
    {
        return inference.specificity != bestSpecificity;
    });
    const auto sameArguments = [&](const Inference& candidate)
    {
        if (candidate.arguments.size() != successes.front().arguments.size()) return false;
        for (std::size_t index = 0; index < candidate.arguments.size(); ++index)
            if (typeSpelling(candidate.arguments[index]) !=
                typeSpelling(successes.front().arguments[index])) return false;
        return true;
    };
    std::string returnSpelling;
    ast::TypeSyntax* inferredReturn = nullptr;
    bool commonReturn = true;
    for (const Inference& success : successes)
    {
        std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
        for (std::size_t index = 0; index < success.arguments.size(); ++index)
            substitutions.emplace(success.candidate->record.parameters[index], success.arguments[index]);
        bool conflict = false;
        ast::TypeSyntax* candidateReturn = cloneAliasType(
            unit.arena(), success.candidate->syntax->typeSyntax(), substitutions, conflict);
        const std::string spelling = typeSpelling(candidateReturn);
        if (inferredReturn == nullptr)
        {
            inferredReturn = candidateReturn;
            returnSpelling = spelling;
        }
        else if (spelling != returnSpelling)
        {
            commonReturn = false;
        }
    }
    if (commonReturn && inferredReturn != nullptr)
        inferredCallReturnTypes_[call] = inferredReturn;

    if (!std::ranges::all_of(successes, sameArguments))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            "generic method call '" + methodName +
                "' has equally specific but incompatible type inferences; write type arguments explicitly");
        return false;
    }

    for (ast::TypeSyntax* argument : successes.front().arguments)
    {
        bool conflict = false;
        call->addTypeSyntax(cloneAliasType(unit.arena(), argument, {}, conflict));
    }
    return true;
}

bool ProjectGenericModel::processGenericCall(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (call == nullptr || call->kind() != ast::AstNodeKind::CallExpression) return false;
    for (ast::TypeSyntax* argument : call->typeSyntaxes())
    {
        if (!processTypeSyntax(unit, argument, namespaceName, scope, diagnostics)) return false;
    }

    ast::AstNode* callee = methodCalleeSyntax(call);
    const std::string methodName = methodCalleeName(call);
    // La escritura del receptor, o --si no hay receptor escrito-- la del tipo desde el que se
    // llama, que es como se nombra a un metodo estatico del propio tipo.
    std::string requestedOwner = templateSourceSpelling(namespaceName, ownerName);
    if (callee != nullptr && callee->kind() == ast::AstNodeKind::MemberExpression)
    {
        const std::string written = receiverSourceSpelling(callee->first());
        if (!written.empty()) requestedOwner = written;
    }

    // ID 132. Los argumentos de VALOR de la llamada. Los hijos de un nodo de llamada son sus
    // argumentos; el receptor viaja en el callee, no aqui.
    const std::size_t argumentCount = call->children().size();

    // ID 132, SEGUNDA MITAD -- las que comparten cuenta de parametros y difieren en los TIPOS.
    //
    // Filtrar por cuenta de argumentos separa `Cuenta<T>(T[])` de `Cuenta<T>(T[], int, int)`. Lo
    // que NO puede separar es `F<T>(T)` de `F<T>(T[])`: los dos toman uno, y elegir necesita el
    // TIPO del argumento -- que esta pasada no conoce, porque corre antes del modelo semantico.
    //
    // ANTES SE RENDIA: "duplicate generic template identity", y habia que renombrar.
    // AHORA SE MATERIALIZAN TODAS con el MISMO nombre especializado, y a partir de ese momento son
    // sobrecargas ORDINARIAS -- metodos no genericos con el mismo nombre y listas de parametros
    // distintas, que este compilador resuelve por tipos desde siempre.
    //
    // No se adivina nada: se le pasa el problema a la capa que tiene los datos. Y si dos
    // especializaciones acaban con la MISMA lista de parametros --`F<int>(int)` y `F<T>(int)`--
    // el choque lo detecta la deteccion de metodos duplicados de siempre, que es su sitio.
    // EL SITIO DE LA LLAMADA, en la forma que entiende la escala compartida.
    const std::vector<std::string> namespaceSegments = splitDotted(namespaceName);
    const std::vector<std::string> enclosingTypes =
        enclosingTypeSpellings(namespaceName, ownerName);
    QualifiedNameSite site;
    site.namespaceSegments = namespaceSegments;
    site.enclosingTypeNames = enclosingTypes;
    site.aliases = &scope.aliases;
    site.importedNamespaces = scope.importedNamespaces;

    const std::vector<TemplateState*> namedAtWinningRung = templatesNamedBy(
        unit, site, requestedOwner, methodName, call->typeSyntaxes().size(),
        argumentCount);
    std::vector<TemplateState*> shared = collectMethodTemplates(
        unit, site, requestedOwner, methodName, call->typeSyntaxes().size(),
        argumentCount);
    if (shared.size() > 1U)
    {
        std::string sharedName = methodName + "__g" +
            std::to_string(call->typeSyntaxes().size());
        for (const ast::TypeSyntax* argument : call->typeSyntaxes())
            sharedName += "__" + sanitize(typeSpelling(argument));

        currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
        currentInstantiationNamespace_ = namespaceName;
        currentInstantiationRange_ = call->range();
        // NSC-LIMIT-007: las que salen de aqui son ALTERNATIVAS entre si, no obligaciones
        // simultaneas. Se marcan con un grupo comun para que la validacion de constraints exija
        // que fallen todas antes de decir nada.
        currentOverloadGroup_ = ++nextOverloadGroup_;
        for (TemplateState* candidate : shared)
        {
            if (call->typeSyntaxes().size() != candidate->record.parameters.size()) continue;
            if (ensureMethodSpecialization(
                    *candidate, call->typeSyntaxes(), diagnostics, sharedName).empty())
            {
                currentOverloadGroup_ = 0;
                return false;
            }
        }
        currentOverloadGroup_ = 0;
        if (callee != nullptr) callee->setText(sharedName);
        call->clearTypeSyntaxes();
        return true;
    }

    TemplateState* state = resolveMethodTemplate(
        unit, site, requestedOwner, methodName, call->typeSyntaxes().size(), argumentCount);
    if (state == nullptr)
        state = resolveMethodTemplate(
            unit, site, requestedOwner, methodName,
            std::numeric_limits<std::size_t>::max(), argumentCount);
    if (state == nullptr)
    {
        const bool ambiguous = namedAtWinningRung.size() > 1U;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            ambiguous
                ? "generic call '" + methodName + "<...>' is ambiguous between " +
                    std::to_string(namedAtWinningRung.size()) +
                    " visible Stage 44 generic method templates"
                : "generic call '" + methodName +
                    "<...>' has no visible Stage 44 generic method template");
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
    currentInstantiationRange_ = call->range();
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
    const std::size_t moduleSeparator = writtenName.find("::");
    if (moduleSeparator != std::string_view::npos)
    {
        const std::string_view requestedModule = writtenName.substr(0, moduleSeparator);
        const std::string_view requestedType = writtenName.substr(moduleSeparator + 2U);
        if (requestedModule.empty() || requestedType.empty()) return nullptr;

        TemplateState* exact = nullptr;
        for (TemplateState& state : templateStates_)
        {
            if (state.record.kind == GenericTemplateKind::Method ||
                state.record.moduleName != requestedModule ||
                (arity != std::numeric_limits<std::size_t>::max() &&
                 state.record.parameters.size() != arity))
                continue;
            const std::string candidate =
                qualifiedName(state.record.namespaceName, state.record.qualifiedTypeName);
            if (candidate != requestedType) continue;
            if (exact != nullptr) return nullptr;
            exact = &state;
        }
        return exact;
    }

    TemplateState* fallback = nullptr;
    const std::string qualified = qualifiedName(namespaceName, writtenName);
    for (TemplateState& state : templateStates_)
    {
        if (state.record.kind == GenericTemplateKind::Method ||
            !templateVisible(unit.projectOrdinal(), state.record)) continue;
        if (arity != std::numeric_limits<std::size_t>::max() && state.record.parameters.size() != arity) continue;
        const std::string candidate =
            qualifiedName(state.record.namespaceName, state.record.qualifiedTypeName);
        if (writtenName.find('.') != std::string_view::npos)
        {
            if (candidate == writtenName)
            {
                if (fallback != nullptr) return nullptr;
                fallback = &state;
            }
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

// NSC-LIMIT-007. QUE PLANTILLAS NOMBRA UNA LLAMADA -- con la escala del modelo de nombres, no
// con una propia.
//
// La version anterior tenia tres escalones inventados aqui: exacto, relativo al namespace del que
// llama, y sufijo de segmentos. Eso no es lo que hace este compilador. Dentro de `A.B`, el nombre
// `C` significa `A.B.C` si existe, si no `A.C`, si no lo que traiga un `using`, si no el global --y
// antes que todo eso, un tipo contenedor del sitio: dentro de `Outer`, `Helper` es `Outer.Helper`--.
// Con los tres escalones inventados, `A.C` y `Z.C` eran los DOS sufijos de `.C` y empataban, de
// modo que una llamada perfectamente legal se declaraba ambigua o clonaba la plantilla ajena --con
// su cuerpo, que habla de SU `T`--.
//
// Ahora la escala viene de `qualifiedNameLadder`, que es la MISMA funcion que recorre
// ProjectNameLookup. Lo que cambia entre las dos fases es que se sondea: alli la tabla de tipos,
// aqui la de plantillas. Ver QualifiedNameLadder.h.
std::vector<ProjectGenericModel::TemplateState*> ProjectGenericModel::templatesNamedBy(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view writtenOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount)
{
    const auto sinFiltro = std::numeric_limits<std::size_t>::max();
    const auto aplicable = [&](const TemplateState& state)
    {
        if (state.record.kind != GenericTemplateKind::Method) return false;
        if (!templateVisible(unit.projectOrdinal(), state.record)) return false;
        if (state.record.name != methodName) return false;
        if (arity != sinFiltro && state.record.parameters.size() != arity) return false;
        if (argumentCount != sinFiltro && valueParameterCount(state.syntax) != argumentCount)
            return false;
        return true;
    };

    // EL PRIMER ESCALON QUE PRODUCE ALGO DECIDE, y los de abajo ya no se miran. Es la regla de
    // sombreado: que exista un `Z.C` en otro namespace no puede cambiar lo que `C` significa aqui.
    for (const QualifiedNameRung& rung : qualifiedNameLadder(site, writtenOwner))
    {
        std::vector<TemplateState*> found;
        for (TemplateState& state : templateStates_)
        {
            if (!aplicable(state)) continue;
            const std::string owner =
                templateSourceSpelling(state.record.namespaceName, state.record.ownerName);
            for (const std::string& candidate : rung.spellings)
            {
                if (owner == candidate)
                {
                    found.push_back(&state);
                    break;
                }
            }
        }
        if (!found.empty()) return found;
    }
    return {};
}

std::vector<ProjectGenericModel::TemplateState*> ProjectGenericModel::collectMethodTemplates(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view writtenOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount)
{
    std::vector<TemplateState*> result =
        templatesNamedBy(unit, site, writtenOwner, methodName, arity, argumentCount);

    // UN SOLO DUENO. Un escalon puede empatar --dos `using` que traen dos `Caja`, por ejemplo--.
    // Materializar las dos significa clonar y analizar un cuerpo que este codigo no nombra, y ese
    // cuerpo habla de SU `T`. Si el empate cruza duenos la llamada es realmente ambigua y no se
    // materializa nada: `resolveMethodTemplate` da entonces su diagnostico, que es su sitio.
    std::string owner;
    std::string ownerModule;
    for (const TemplateState* state : result)
    {
        const std::string candidate =
            templateSourceSpelling(state->record.namespaceName, state->record.ownerName);
        if (owner.empty())
        {
            owner = candidate;
            ownerModule = state->record.moduleName;
        }
        else if (owner != candidate || ownerModule != state->record.moduleName) return {};
    }
    return result;
}

ProjectGenericModel::TemplateState* ProjectGenericModel::resolveMethodTemplate(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view writtenOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount)
{
    const auto sinFiltro = std::numeric_limits<std::size_t>::max();

    // CON DUENO: la misma funcion que usa `collectMethodTemplates`, no una copia de sus reglas.
    // Primero exigiendo la cuenta de argumentos y despues sin ella, que es el aflojamiento que
    // este metodo ya tenia.
    for (const std::size_t cuenta : {argumentCount, sinFiltro})
    {
        const std::vector<TemplateState*> named =
            templatesNamedBy(unit, site, writtenOwner, methodName, arity, cuenta);
        if (named.size() == 1) return named.front();
        // Mas de una en el escalon ganador y del mismo nombre: ambigua de verdad. Aflojar la
        // cuenta de argumentos no la desambigua, asi que se sigue al siguiente intento solo
        // cuando no hubo NINGUNA.
        if (!named.empty()) break;
        if (cuenta == sinFiltro) break;
    }

    // SIN DUENO -- y solo aqui, cuando la escala no encontro ninguno.
    //
    // Es lo unico que esta fase tiene para sospechar que el receptor escrito no era un tipo sino un
    // VALOR: `objeto.F<int>()` produce la escritura `objeto`, que no nombra ninguna plantilla
    // porque no nombra ningun tipo. Tomar este camino cuando el dueno SI se resolvio seria
    // deshacer el filtro que se acaba de aplicar, y era justamente lo que hacia antes.
    const auto coincideSinDueno = [&](const TemplateState& state, bool exigirArgumentos)
    {
        if (state.record.kind != GenericTemplateKind::Method) return false;
        if (!templateVisible(unit.projectOrdinal(), state.record)) return false;
        if (arity != sinFiltro && state.record.parameters.size() != arity) return false;
        if (state.record.name != methodName) return false;
        if (exigirArgumentos && argumentCount != sinFiltro &&
            valueParameterCount(state.syntax) != argumentCount)
            return false;
        return true;
    };
    const auto unico = [&](bool exigirArgumentos) -> TemplateState*
    {
        TemplateState* result = nullptr;
        for (TemplateState& state : templateStates_)
        {
            if (!coincideSinDueno(state, exigirArgumentos)) continue;
            if (result != nullptr) return nullptr;
            result = &state;
        }
        return result;
    };
    if (TemplateState* result = unico(true); result != nullptr) return result;
    return unico(false);
}

std::string ProjectGenericModel::ensureTypeSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::size_t callerProjectOrdinal = currentInstantiationProjectOrdinal_;
    const std::string callerNamespace = currentInstantiationNamespace_;
    const SourceRange callerRange = currentInstantiationRange_;
    const std::size_t callerOverloadGroup = currentOverloadGroup_;

    std::vector<std::string> spellings;
    std::vector<std::string> identities;
    spellings.reserve(arguments.size());
    identities.reserve(arguments.size());
    for (const ast::TypeSyntax* argument : arguments)
    {
        spellings.push_back(typeSpelling(argument));
        identities.push_back(typeSpecializationIdentity(argument));
    }

    std::ostringstream keyBuilder;
    keyBuilder << state.record.canonicalKey << '<';
    for (std::size_t i = 0; i < spellings.size(); ++i)
    {
        if (i != 0) keyBuilder << ',';
        keyBuilder << identities[i];
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
        identities,
        ordinal,
        state.ordinal,
        nullptr,
        currentInstantiationRange_,
        currentOverloadGroup_});

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

    processNode(
        *state.unit, clone, state.record.namespaceName, specializedName,
        GenericLookupScope{}, false, diagnostics);
    // processNode may recursively materialize another generic and therefore replace all four
    // current-instantiation fields. They describe a call stack, not global "last call" state:
    // restore the caller before returning so a sibling specialization (notably the next overload
    // candidate) cannot inherit the nested library context.
    currentInstantiationProjectOrdinal_ = callerProjectOrdinal;
    currentInstantiationNamespace_ = callerNamespace;
    currentInstantiationRange_ = callerRange;
    currentOverloadGroup_ = callerOverloadGroup;
    return specializedName;
}

std::string ProjectGenericModel::ensureMethodSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string_view sharedName)
{
    const std::size_t callerProjectOrdinal = currentInstantiationProjectOrdinal_;
    const std::string callerNamespace = currentInstantiationNamespace_;
    const SourceRange callerRange = currentInstantiationRange_;
    const std::size_t callerOverloadGroup = currentOverloadGroup_;

    std::vector<std::string> spellings;
    std::vector<std::string> identities;
    spellings.reserve(arguments.size());
    identities.reserve(arguments.size());
    for (const ast::TypeSyntax* argument : arguments)
    {
        spellings.push_back(typeSpelling(argument));
        identities.push_back(typeSpecializationIdentity(argument));
    }

    std::ostringstream keyBuilder;
    keyBuilder << state.record.canonicalKey << '<';
    for (std::size_t i = 0; i < spellings.size(); ++i)
    {
        if (i != 0) keyBuilder << ',';
        keyBuilder << identities[i];
    }
    keyBuilder << '>';
    const std::string key = keyBuilder.str();
    if (const auto found = specializationByKey_.find(key); found != specializationByKey_.end())
        return specializations_[found->second].specializedName;

    // ID 132, segunda mitad. `sharedName` no esta vacio cuando VARIAS plantillas comparten nombre
    // y aridad generica: todas se especializan con EL MISMO nombre, y a partir de ahi dejan de ser
    // un problema de genericos y pasan a ser SOBRECARGAS ORDINARIAS, que este compilador ya
    // resuelve por lista de parametros desde siempre.
    //
    // Es la unica forma honesta de hacerlo en esta pasada: elegir aqui exigiria los TIPOS de los
    // argumentos, y el modelo semantico --el unico que los conoce-- corre despues. En vez de
    // adivinar, se materializan todas y se deja elegir a quien sabe.
    //
    // La CLAVE sigue llevando la identidad de cada plantilla (con su `#N`), asi que la cache
    // sigue distinguiendolas; lo unico compartido es el nombre emitido.
    std::string specializedName;
    if (!sharedName.empty())
    {
        specializedName = std::string(sharedName);
    }
    else
    {
        specializedName = state.record.name + "__g" + std::to_string(arguments.size());
        for (const std::string& spelling : spellings) specializedName += "__" + sanitize(spelling);
        specializedName += "__h" + stableHashSuffix(key);
    }

    const std::size_t ordinal = specializations_.size();
    specializationByKey_.emplace(key, ordinal);
    specializations_.push_back(GenericSpecializationRecord{
        GenericTemplateKind::Method,
        state.syntax,
        nullptr,
        state.record.canonicalKey,
        key,
        specializedName,
        identities,
        ordinal,
        state.ordinal,
        nullptr,
        currentInstantiationRange_,
        currentOverloadGroup_});

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
    processNode(
        *state.unit, clone, state.record.namespaceName, state.record.ownerName,
        GenericLookupScope{}, false, diagnostics);
    currentInstantiationProjectOrdinal_ = callerProjectOrdinal;
    currentInstantiationNamespace_ = callerNamespace;
    currentInstantiationRange_ = callerRange;
    currentOverloadGroup_ = callerOverloadGroup;
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
        preserveOrAttachInstantiationOrigin(result);
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
        if (const auto origin = resolvedTypeOriginBySyntax_.find(source);
            origin != resolvedTypeOriginBySyntax_.end())
        {
            resolvedTypeOriginBySyntax_[result] = origin->second;
        }
    }

    for (const ast::TypeModifierSyntax& modifier : source->modifiers())
    {
        result->addModifier(ast::TypeModifierSyntax{
            modifier.kind,
            cloneNode(arena, modifier.arrayCount, substitutions, "", ""),
            modifier.range});
        // In the substitution branch `result` already carries the argument's canonical identity
        // (including modifiers written on that argument). Modifiers written on the template
        // parameter occurrence are outer modifiers and must be appended as well: substituting
        // Foo for T in T[] identifies Foo[], not Foo.
        if (substituted)
        {
            if (auto identity = resolvedTypeIdentityBySyntax_.find(result);
                identity != resolvedTypeIdentityBySyntax_.end())
            {
                if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer)
                    identity->second.push_back('*');
                else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray)
                    identity->second += "[]";
                else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray)
                    identity->second += "[*]";
                else
                    identity->second += "[fixed]";
            }
        }
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
        if (const auto identity = resolvedTypeIdentityBySyntax_.find(source);
            identity != resolvedTypeIdentityBySyntax_.end())
        {
            resolvedTypeIdentityBySyntax_[result] = identity->second;
        }
    }
    return result;
}

void ProjectGenericModel::preserveOrAttachInstantiationOrigin(ast::TypeSyntax* syntax) const
{
    if (syntax == nullptr || lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) ||
        syntax->originProjectOrdinal().has_value())
    {
        return;
    }

    // This node names a substituted argument, not a type written in the template that now owns
    // the clone. Prefer the declaration selected while processing the request site: the project
    // that writes Dictionary<string, Texture2D> may be a third project, distinct from both the
    // Dictionary template and the project that declares Texture2D. If the early pass could not
    // select a declaration, retain the historical request-site fallback. Keep the spelling
    // untouched either way: origin is lookup context, never specialization identity.
    if (const auto origin = resolvedTypeOriginBySyntax_.find(syntax);
        origin != resolvedTypeOriginBySyntax_.end())
    {
        syntax->setOriginProjectOrdinal(origin->second.projectOrdinal);
        syntax->setOriginNamespace(origin->second.namespaceName);
    }
    else
    {
        syntax->setOriginProjectOrdinal(currentInstantiationProjectOrdinal_);
        syntax->setOriginNamespace(currentInstantiationNamespace_);
    }
}

ast::AstNode* ProjectGenericModel::cloneNode(
    ast::AstArena& arena,
    const ast::AstNode* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::string_view rewrittenContainingTypeName,
    std::string_view originalContainingTypeName)
{
    if (source == nullptr) return nullptr;
    const bool layoutQueryOfSubstitutedParameter =
        (source->kind() == ast::AstNodeKind::SizeofExpression ||
         source->kind() == ast::AstNodeKind::AlignofExpression) &&
        source->first() != nullptr &&
        source->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
        substitutions.contains(source->first()->text());
    ast::AstNodeKind clonedKind = source->kind();
    if (layoutQueryOfSubstitutedParameter)
    {
        clonedKind = source->kind() == ast::AstNodeKind::SizeofExpression
            ? ast::AstNodeKind::SizeofTypeExpression
            : ast::AstNodeKind::AlignofTypeExpression;
    }
    ast::AstNode* result = arena.make<ast::AstNode>(clonedKind, source->range());
    result->setText(source->text());
    result->setAuxiliaryText(source->auxiliaryText());
    result->setTokenKind(source->tokenKind());
    result->setFlag(source->flag());
    result->setIntegerValue(source->integerValue());
    result->setFloatingValue(source->floatingValue());
    // A scalar `new T` inside a generic template allocates one raw storage slot whose element
    // type is T.  After monomorphization the cloned TypeSyntax names the concrete argument; for a
    // class argument that made method-body analysis mistake the original raw allocation for the
    // distinct source form `new ConcreteClass`, which must use constructor syntax.  Preserve the
    // source-level intent on the expression before substituting its type.
    if (source->kind() == ast::AstNodeKind::NewExpression && !source->flag() &&
        source->typeSyntax() != nullptr && source->typeSyntax()->typeArguments().empty() &&
        substitutions.contains(source->typeSyntax()->baseName()))
    {
        result->setAuxiliaryText(std::string(kGenericRawNewAuxiliary));
    }
    // Modifiers are semantic template state, not decoration. In particular, the mutually
    // exclusive inline/forceinline/noinline policy must reach every explicit or inferred clone;
    // ProjectMemberModel will consume the copied token into the single InliningPolicy field.
    for (const lexer::TokenKind modifier : source->modifiers()) result->addModifier(modifier);
    for (const std::string& parameter : source->genericParameters()) result->addGenericParameter(parameter);
    // NSC-LIMIT-007. Las clausulas se clonan con la sustitucion aplicada a sus tipos, igual que
    // cualquier otro TypeSyntax del nodo. Copiar los punteros originales dejaria la
    // especializacion apuntando a sintaxis de la PLANTILLA -- viva, pero de otro arbol, y con `T`
    // sin sustituir dentro. El nodo especializado ya no es generico, asi que estas clausulas no
    // se vuelven a validar; viajan para que un volcado del AST especializado diga la verdad.
    for (const ast::GenericConstraintClause& clause : source->genericConstraints())
    {
        ast::GenericConstraintClause copy;
        copy.parameterName = clause.parameterName;
        copy.parameterRange = clause.parameterRange;
        copy.requiresClass = clause.requiresClass;
        copy.range = clause.range;
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            copy.typeConstraints.push_back(cloneType(arena, constraint, substitutions));
        // El argumento con el que se cerro este parametro, colgado de la clausula para que la
        // resolucion de nombres lo alcance. Ver GenericConstraintClause::substitutedArgument.
        if (const auto found = substitutions.find(clause.parameterName);
            found != substitutions.end())
        {
            ast::TypeSyntax* substitutedArgument = cloneType(arena, found->second, substitutions);
            preserveOrAttachInstantiationOrigin(substitutedArgument);
            copy.substitutedArgument = substitutedArgument;
        }
        result->addGenericConstraint(std::move(copy));
    }

    if (!rewrittenContainingTypeName.empty() && !originalContainingTypeName.empty() &&
        (source->kind() == ast::AstNodeKind::ConstructorDeclaration ||
         source->kind() == ast::AstNodeKind::DestructorDeclaration) &&
        source->text() == originalContainingTypeName)
    {
        result->setText(std::string(rewrittenContainingTypeName));
    }

    if (layoutQueryOfSubstitutedParameter)
    {
        ast::TypeSyntax* measuredType = cloneType(
            arena, substitutions.at(source->first()->text()), substitutions);
        preserveOrAttachInstantiationOrigin(measuredType);
        result->setTypeSyntax(measuredType);
    }
    else
    {
        result->setTypeSyntax(cloneType(arena, source->typeSyntax(), substitutions));
    }
    for (const ast::TypeSyntax* syntax : source->typeSyntaxes())
        result->addTypeSyntax(cloneType(arena, syntax, substitutions));
    if (!layoutQueryOfSubstitutedParameter)
    {
        result->setFirst(cloneNode(
            arena, source->first(), substitutions,
            rewrittenContainingTypeName, originalContainingTypeName));
    }
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
        else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray) result += "[*]";
        else result += "[fixed]";
    }
    return result;
}

std::string ProjectGenericModel::typeSpecializationIdentity(const ast::TypeSyntax* syntax) const
{
    if (syntax != nullptr)
    {
        if (const auto resolved = resolvedTypeIdentityBySyntax_.find(syntax);
            resolved != resolvedTypeIdentityBySyntax_.end())
        {
            return resolved->second;
        }
    }

    const std::string spelling = typeSpelling(syntax);
    if (syntax == nullptr || lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()))
    {
        return spelling;
    }

    // The spelling remains the public/readable type argument. The cache identity additionally
    // records where an unresolved source name came from, so AppA.Payload and AppB.Payload cannot
    // reuse one clone merely because both were written as `Payload`. A substituted argument
    // carries its original site explicitly; a direct call uses the current site. The persistent
    // identity uses the module name, never the graph-order-dependent ordinal (the ordinal remains
    // lookup-only). Length-prefix module and namespace so punctuation cannot make identities
    // concatenate alike.
    const std::size_t projectOrdinal =
        syntax->originProjectOrdinal().value_or(currentInstantiationProjectOrdinal_);
    const std::string_view moduleName = projectOrdinal < projectModuleNames_.size()
        ? std::string_view(projectModuleNames_[projectOrdinal])
        : std::string_view("<invalid-project>");
    const std::string namespaceName = syntax->originNamespace().has_value()
        ? syntax->originNamespace().value()
        : (syntax->originProjectOrdinal().has_value() ? std::string{} : currentInstantiationNamespace_);

    return spelling + "@origin-module[" + std::to_string(moduleName.size()) + "]=" +
        std::string(moduleName) +
        ";namespace[" + std::to_string(namespaceName.size()) + "]=" + namespaceName;
}

const ProjectGenericModel::DeclaredTypeIdentity* ProjectGenericModel::resolveDeclaredTypeIdentity(
    const ast::TypeSyntax* syntax,
    std::size_t projectOrdinal,
    std::string_view namespaceName,
    const GenericLookupScope& scope) const
{
    if (syntax == nullptr || lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) ||
        !syntax->typeArguments().empty())
    {
        return nullptr;
    }

    projectOrdinal = syntax->originProjectOrdinal().value_or(projectOrdinal);
    const std::string effectiveNamespace = syntax->originNamespace().has_value()
        ? syntax->originNamespace().value()
        : (syntax->originProjectOrdinal().has_value() ? std::string{} : std::string(namespaceName));
    const std::vector<std::string> namespaceSegments = splitDotted(effectiveNamespace);
    const std::vector<std::string> noContainingTypes;

    // A substituted node carries the original project/namespace, but not its using directives.
    // Its declaration identity was normally copied before substitution. If an older/unannotated
    // node reaches this fallback, do not accidentally apply the template's imports to the
    // consumer's source name.
    const std::vector<std::string> noImports;
    const std::unordered_map<std::string, std::string> noAliases;
    QualifiedNameSite site;
    site.namespaceSegments = namespaceSegments;
    site.enclosingTypeNames = noContainingTypes;
    site.aliases = syntax->originProjectOrdinal().has_value() ? &noAliases : &scope.aliases;
    site.importedNamespaces = syntax->originProjectOrdinal().has_value()
        ? std::span<const std::string>(noImports)
        : std::span<const std::string>(scope.importedNamespaces);

    for (const QualifiedNameRung& rung : qualifiedNameLadder(site, syntax->baseName()))
    {
        std::vector<const DeclaredTypeIdentity*> identities;
        for (const std::string& spelling : rung.spellings)
        {
            for (const DeclaredTypeIdentity& declaration : declaredTypeIdentities_)
            {
                if (declaration.sourceName != spelling) continue;

                const bool sameProject = declaration.projectOrdinal == projectOrdinal;
                bool visible = false;
                switch (rung.tier)
                {
                    case QualifiedNameTier::EnclosingType:
                    case QualifiedNameTier::LexicalNamespace:
                    case QualifiedNameTier::Global:
                        visible = sameProject;
                        break;
                    case QualifiedNameTier::ImportedNamespace:
                        visible = sameProject ||
                            (declaration.isPublic &&
                             projectVisible(projectOrdinal, declaration.projectOrdinal));
                        break;
                    case QualifiedNameTier::ReferencedModule:
                        visible = !sameProject && declaration.isPublic &&
                            projectVisible(projectOrdinal, declaration.projectOrdinal);
                        break;
                }
                if (!visible || std::find_if(
                        identities.begin(), identities.end(), [&](const DeclaredTypeIdentity* identity) {
                            return identity != nullptr && identity->identityKey == declaration.identityKey;
                        }) != identities.end())
                {
                    continue;
                }
                identities.push_back(&declaration);
            }
        }
        if (identities.size() == 1) return identities.front();
        if (identities.size() > 1) return nullptr;
    }
    return nullptr;
}

void ProjectGenericModel::appendTypeModifiers(
    std::string& identity,
    const ast::TypeSyntax* syntax)
{
    if (syntax == nullptr) return;
    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer) identity.push_back('*');
        else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray) identity += "[]";
        else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray) identity += "[*]";
        else identity += "[fixed]";
    }
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
    projectModuleNames_.clear();
    projectModuleNames_.reserve(projects.size());
    for (const compilation::Project& project : projects)
    {
        projectModuleNames_.push_back(project.moduleName());
    }
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

ProjectGenericModel::TemplateState* ProjectGenericModel::stateForSyntax(
    const ast::AstNode* syntax) noexcept
{
    const auto found = std::ranges::find_if(templateStates_, [&](const TemplateState& state) {
        return state.syntax == syntax;
    });
    return found == templateStates_.end() ? nullptr : &*found;
}

void ProjectGenericModel::discoverTemplateDependencies(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope)
{
    if (node == nullptr) return;

    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(namespaceName, node->text());

    GenericLookupScope currentScope = scope;
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr) continue;
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
        {
            const std::string written = child->text();
            if (written.empty()) continue;
            const auto add = [&](std::string candidate)
            {
                if (std::find(
                        currentScope.importedNamespaces.begin(),
                        currentScope.importedNamespaces.end(),
                        candidate) == currentScope.importedNamespaces.end())
                    currentScope.importedNamespaces.push_back(std::move(candidate));
            };
            const std::vector<std::string> segments = splitDotted(currentNamespace);
            for (std::size_t depth = segments.size(); depth > 0; --depth)
            {
                std::string prefixed;
                for (std::size_t index = 0; index < depth; ++index)
                {
                    if (!prefixed.empty()) prefixed.push_back('.');
                    prefixed += segments[index];
                }
                prefixed.push_back('.');
                prefixed += written;
                add(std::move(prefixed));
            }
            add(written);
        }
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective &&
                 !child->auxiliaryText().empty())
        {
            currentScope.aliases[child->text()] = child->auxiliaryText();
        }
    }

    if (TemplateState* root = stateForSyntax(node); root != nullptr)
    {
        const std::string templateOwner = root->record.kind == GenericTemplateKind::Method
            ? ownerName
            : appendOwner(ownerName, node->text());
        collectTemplateDependencies(
            *root, unit, node, currentNamespace, templateOwner, currentScope, node);
        return;
    }

    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration ||
         node->kind() == ast::AstNodeKind::StructDeclaration)
            ? appendOwner(ownerName, node->text())
            : ownerName;
    for (ast::AstNode* child : node->children())
        discoverTemplateDependencies(
            unit, child, currentNamespace, childOwner, currentScope);
}

void ProjectGenericModel::collectTypeDependency(
    TemplateState& root,
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::string& namespaceName,
    const GenericLookupScope& scope)
{
    if (syntax == nullptr) return;
    static_cast<void>(scope);
    for (const ast::TypeSyntax* argument : syntax->typeArguments())
        collectTypeDependency(root, unit, argument, namespaceName, scope);
    if (syntax->typeArguments().empty()) return;

    TemplateState* target = resolveTypeTemplate(
        unit, namespaceName, syntax->baseName(), syntax->typeArguments().size());
    if (target == nullptr)
        target = resolveTypeTemplate(
            unit, namespaceName, syntax->baseName(),
            std::numeric_limits<std::size_t>::max());
    if (target == nullptr)
    {
        root.record.unresolvedDependencies.push_back(
            "generic type '" + syntax->baseName() + "<...>'");
        return;
    }
    root.record.dependencies.push_back(target->record.canonicalKey);
}

void ProjectGenericModel::collectCallDependency(
    TemplateState& root,
    ProjectSyntaxUnit& unit,
    const ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope)
{
    if (call == nullptr || call->kind() != ast::AstNodeKind::CallExpression ||
        call->typeSyntaxes().empty()) return;

    const ast::AstNode* callee = call->first();
    const std::string methodName = methodCalleeName(call);
    std::string requestedOwner = templateSourceSpelling(namespaceName, ownerName);
    if (callee != nullptr && callee->kind() == ast::AstNodeKind::MemberExpression)
    {
        const std::string written = receiverSourceSpelling(callee->first());
        if (!written.empty()) requestedOwner = written;
    }

    const std::vector<std::string> namespaceSegments = splitDotted(namespaceName);
    const std::vector<std::string> enclosingTypes =
        enclosingTypeSpellings(namespaceName, ownerName);
    QualifiedNameSite site;
    site.namespaceSegments = namespaceSegments;
    site.enclosingTypeNames = enclosingTypes;
    site.aliases = &scope.aliases;
    site.importedNamespaces = scope.importedNamespaces;

    const std::size_t arity = call->typeSyntaxes().size();
    const std::size_t argumentCount = call->children().size();
    const std::vector<TemplateState*> named = templatesNamedBy(
        unit, site, requestedOwner, methodName, arity, argumentCount);
    const std::vector<TemplateState*> targets = collectMethodTemplates(
        unit, site, requestedOwner, methodName, arity, argumentCount);
    if (!targets.empty())
    {
        for (const TemplateState* target : targets)
            root.record.dependencies.push_back(target->record.canonicalKey);
        return;
    }

    TemplateState* fallback = resolveMethodTemplate(
        unit, site, requestedOwner, methodName, arity, argumentCount);
    if (fallback == nullptr)
        fallback = resolveMethodTemplate(
            unit, site, requestedOwner, methodName,
            std::numeric_limits<std::size_t>::max(), argumentCount);
    if (fallback != nullptr)
    {
        root.record.dependencies.push_back(fallback->record.canonicalKey);
        return;
    }

    root.record.unresolvedDependencies.push_back(
        std::string(named.size() > 1U ? "ambiguous generic method '" : "missing generic method '") +
        methodName + "<...>'");
}

void ProjectGenericModel::collectTemplateDependencies(
    TemplateState& root,
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    const ast::AstNode* rootSyntax)
{
    if (node == nullptr) return;
    if (node != rootSyntax && stateForSyntax(node) != nullptr) return;

    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(namespaceName, node->text());

    GenericLookupScope currentScope = scope;
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr) continue;
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
            currentScope.importedNamespaces.push_back(child->text());
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective &&
                 !child->auxiliaryText().empty())
            currentScope.aliases[child->text()] = child->auxiliaryText();
    }

    collectTypeDependency(root, unit, node->typeSyntax(), currentNamespace, currentScope);
    for (const ast::TypeSyntax* syntax : node->typeSyntaxes())
        collectTypeDependency(root, unit, syntax, currentNamespace, currentScope);
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            collectTypeDependency(root, unit, constraint, currentNamespace, currentScope);
        collectTypeDependency(
            root, unit, clause.substitutedArgument, currentNamespace, currentScope);
    }
    collectCallDependency(
        root, unit, node, currentNamespace, ownerName, currentScope);

    collectTemplateDependencies(
        root, unit, node->first(), currentNamespace, ownerName, currentScope, rootSyntax);
    collectTemplateDependencies(
        root, unit, node->second(), currentNamespace, ownerName, currentScope, rootSyntax);
    collectTemplateDependencies(
        root, unit, node->third(), currentNamespace, ownerName, currentScope, rootSyntax);

    const std::string childOwner = node == rootSyntax
        ? ownerName
        : ((node->kind() == ast::AstNodeKind::ClassDeclaration ||
            node->kind() == ast::AstNodeKind::StructDeclaration)
               ? appendOwner(ownerName, node->text())
               : ownerName);
    for (ast::AstNode* child : node->children())
        collectTemplateDependencies(
            root, unit, child, currentNamespace, childOwner, currentScope, rootSyntax);
}

bool ProjectGenericModel::templateVisible(
    std::size_t from,
    const GenericTemplateRecord& target) const noexcept
{
    // A materialization-only declaration remains usable while walking a clone owned by its
    // producer project. A different project may discover only the producer's public roots.
    return from == target.projectOrdinal ||
        (target.accessibility == GenericTemplateAccessibility::Public &&
         projectVisible(from, target.projectOrdinal));
}

}
