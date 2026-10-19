#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "../Internal/LookupEnvironment.h"
#include "../Internal/ProjectNameLookupFrame.h"

#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Compilation/PartialTypeAggregator.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <deque>
#include <filesystem>
#include <functional>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
bool ProjectNameLookup::isBuiltinTypeSurface(
    const ast::TypeSyntax* syntax,
    bool portableSimd)
{
    // NS304-T05-F09: `Future<T>` is a structural opaque surface (future:<T>), not a nominal type.
    const bool futureSurface =
        syntax->baseName() == "Future" && syntax->typeArguments().size() == 1;
    // NS304-T06-F02: `Iterator<T>` is a structural opaque surface (iterator:<T>).
    const bool iteratorSurface =
        syntax->baseName() == "Iterator" && syntax->typeArguments().size() == 1;
    // NS304-T06-F07: `Iterator<T,E>` is also a structural opaque surface (iterator:<T>|<E>).
    const bool iteratorErrorSurface =
        syntax->baseName() == "Iterator" && syntax->typeArguments().size() == 2;
    // NS304-T07-F01: `AsyncIterator<T,E>` is a structural opaque surface (async-iterator:<T>|<E>).
    const bool asyncIteratorSurface =
        syntax->baseName() == "AsyncIterator" && syntax->typeArguments().size() == 2;
    // NS304-T15-F01: `decimal` is a builtin 128-bit value type (builtin:decimal).
    const bool decimalSurface = syntax->baseName() == "decimal" && syntax->typeArguments().empty();
    return syntax->isFunction() || syntax->isTuple() || portableSimd || futureSurface ||
        iteratorSurface || iteratorErrorSurface || asyncIteratorSurface || decimalSurface ||
        lexer::isBuiltinTypeKeyword(syntax->baseTokenKind());
}

void ProjectNameLookup::validatePortableSimdType(
    const ast::TypeSyntax* syntax,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::size_t expected = syntax->baseName() == "simd" ? 2U : 2U;
    if (syntax->typeArguments().size() != expected)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "'" + syntax->baseName() + "' requires a lane type and a constant lane count");
        return;
    }
    const ast::TypeSyntax* lane = syntax->typeArguments()[0];
    const ast::TypeSyntax* count = syntax->typeArguments()[1];
    std::string markerType;
    std::string markerValue;
    std::size_t laneCount = 0;
    const bool marker = language::parseConstGenericArgumentMarker(
        count->baseName(), markerType, markerValue);
    const auto parsed = std::from_chars(
        markerValue.data(), markerValue.data() + markerValue.size(), laneCount);
    std::string simdError;
    const std::string laneKey = lexer::isBuiltinTypeKeyword(lane->baseTokenKind())
        ? "builtin:" + lane->baseName() : "unresolved:" + lane->baseName();
    if (marker && parsed.ec == std::errc{} &&
        parsed.ptr == markerValue.data() + markerValue.size() &&
        language::portableSimdType(
            laneKey, laneCount, syntax->baseName() == "mask", simdError).has_value())
        return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        syntax->range(),
        simdError.empty() ? "portable SIMD lane count must be a constant integer" : simdError);
}

void ProjectNameLookup::validateBuiltinTypeBinding(
    const ast::TypeSyntax* syntax,
    bool portableSimd,
    TypeNameBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    binding.kind = TypeNameBindingKind::Builtin;
    if (portableSimd) validatePortableSimdType(syntax, diagnostics);
    if (!syntax->isDynamicTrait()) return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        syntax->range(),
        "'dyn' requires a trait or interface type, not a builtin, tuple, or function type");
}

void ProjectNameLookup::appendObjectTypeRoot(
    TypeNameBinding& binding,
    const ast::TypeSyntax* syntax) const
{
    // C8 "type root": `object` is the lowercase spelling of NativeSharp.Core.Object, the way
    // `string` is of the native string type. Deliberately NOT a lexer keyword and resolved
    // only as a LAST resort, after ordinary lookup has failed: a project that does not have
    // the root in view keeps `object` as an ordinary identifier, and one that declares its own
    // type called `object` still wins. Same shape as the alias expansion just above -- the
    // binding's writtenName stays "object", so diagnostics say what the programmer wrote.
    if (!binding.candidateTypeOrdinals.empty() || syntax->baseName() != "object") return;
    for (std::size_t ordinal = 0; ordinal < namedTypes_.size(); ++ordinal)
    {
        if (namedTypes_[ordinal].kind == NamedTypeDeclarationKind::Class &&
            namedTypes_[ordinal].identity.fullyQualifiedName() == "NativeSharp.Core.Object")
        {
            binding.candidateTypeOrdinals.push_back(ordinal);
            break;
        }
    }
}

}
