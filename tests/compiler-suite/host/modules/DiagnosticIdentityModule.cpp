#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::diagnostics::diagnosticIdName;
using nativesharp::diagnostics::isNativeSharp3Diagnostic;

using DiagnosticEntry = std::pair<DiagnosticId, std::string_view>;

constexpr std::array kOrdinaryEntries{
    DiagnosticEntry{DiagnosticId::CannotOpenSourceFile, "CannotOpenSourceFile"},
    DiagnosticEntry{DiagnosticId::SourceFileTooLarge, "SourceFileTooLarge"},
    DiagnosticEntry{DiagnosticId::UnexpectedCharacter, "UnexpectedCharacter"},
    DiagnosticEntry{DiagnosticId::UnterminatedBlockComment, "UnterminatedBlockComment"},
    DiagnosticEntry{DiagnosticId::InvalidNumericLiteral, "InvalidNumericLiteral"},
    DiagnosticEntry{DiagnosticId::InvalidCharacterLiteral, "InvalidCharacterLiteral"},
    DiagnosticEntry{DiagnosticId::InvalidStringLiteral, "InvalidStringLiteral"},
    DiagnosticEntry{DiagnosticId::InvalidEscapeSequence, "InvalidEscapeSequence"},
    DiagnosticEntry{DiagnosticId::ExpectedToken, "ExpectedToken"},
    DiagnosticEntry{DiagnosticId::UnexpectedToken, "UnexpectedToken"},
    DiagnosticEntry{DiagnosticId::ExpectedDeclaration, "ExpectedDeclaration"},
    DiagnosticEntry{DiagnosticId::ExpectedExpression, "ExpectedExpression"},
    DiagnosticEntry{DiagnosticId::ExpectedType, "ExpectedType"},
    DiagnosticEntry{DiagnosticId::UnknownTypeName, "UnknownTypeName"},
    DiagnosticEntry{DiagnosticId::UnknownNamespace, "UnknownNamespace"},
    DiagnosticEntry{DiagnosticId::InvalidUsingTarget, "InvalidUsingTarget"},
    DiagnosticEntry{DiagnosticId::DuplicateUsingAlias, "DuplicateUsingAlias"},
    DiagnosticEntry{DiagnosticId::AmbiguousTypeName, "AmbiguousTypeName"},
    DiagnosticEntry{DiagnosticId::PartialTypeConflict, "PartialTypeConflict"},
    DiagnosticEntry{DiagnosticId::InvalidBaseType, "InvalidBaseType"},
    DiagnosticEntry{DiagnosticId::InheritanceCycle, "InheritanceCycle"},
    DiagnosticEntry{DiagnosticId::InaccessibleMember, "InaccessibleMember"},
    DiagnosticEntry{DiagnosticId::InvalidOverride, "InvalidOverride"},
    DiagnosticEntry{DiagnosticId::UnknownIdentifier, "UnknownIdentifier"},
    DiagnosticEntry{DiagnosticId::DuplicateSymbol, "DuplicateSymbol"},
    DiagnosticEntry{DiagnosticId::PreviousDeclaration, "PreviousDeclaration"},
    DiagnosticEntry{DiagnosticId::ConflictingFunctionDeclaration, "ConflictingFunctionDeclaration"},
    DiagnosticEntry{DiagnosticId::DuplicateFunctionDefinition, "DuplicateFunctionDefinition"},
    DiagnosticEntry{DiagnosticId::MissingFunctionDefinition, "MissingFunctionDefinition"},
    DiagnosticEntry{DiagnosticId::InvalidType, "InvalidType"},
    DiagnosticEntry{DiagnosticId::IncompleteType, "IncompleteType"},
    DiagnosticEntry{DiagnosticId::TypeMismatch, "TypeMismatch"},
    DiagnosticEntry{DiagnosticId::InvalidConversion, "InvalidConversion"},
    DiagnosticEntry{DiagnosticId::InvalidOperand, "InvalidOperand"},
    DiagnosticEntry{DiagnosticId::InvalidAssignmentTarget, "InvalidAssignmentTarget"},
    DiagnosticEntry{DiagnosticId::InvalidAddressOf, "InvalidAddressOf"},
    DiagnosticEntry{DiagnosticId::InvalidDereference, "InvalidDereference"},
    DiagnosticEntry{DiagnosticId::InvalidCall, "InvalidCall"},
    DiagnosticEntry{DiagnosticId::InvalidArgumentCount, "InvalidArgumentCount"},
    DiagnosticEntry{DiagnosticId::InvalidMemberAccess, "InvalidMemberAccess"},
    DiagnosticEntry{DiagnosticId::UnknownMember, "UnknownMember"},
    DiagnosticEntry{DiagnosticId::InvalidIndex, "InvalidIndex"},
    DiagnosticEntry{DiagnosticId::ConstantIndexOutOfBounds, "ConstantIndexOutOfBounds"},
    DiagnosticEntry{DiagnosticId::InvalidCondition, "InvalidCondition"},
    DiagnosticEntry{DiagnosticId::InvalidReturn, "InvalidReturn"},
    DiagnosticEntry{DiagnosticId::NotAllPathsReturn, "NotAllPathsReturn"},
    DiagnosticEntry{DiagnosticId::BreakOutsideLoop, "BreakOutsideLoop"},
    DiagnosticEntry{DiagnosticId::ContinueOutsideLoop, "ContinueOutsideLoop"},
    DiagnosticEntry{DiagnosticId::ConstRequiresConstant, "ConstRequiresConstant"},
    DiagnosticEntry{DiagnosticId::StaticAssertionFailed, "StaticAssertionFailed"},
    DiagnosticEntry{DiagnosticId::GlobalRequiresConstant, "GlobalRequiresConstant"},
    DiagnosticEntry{DiagnosticId::InvalidArrayCount, "InvalidArrayCount"},
    DiagnosticEntry{DiagnosticId::InvalidEnumInitializer, "InvalidEnumInitializer"},
    DiagnosticEntry{DiagnosticId::InvalidSizeof, "InvalidSizeof"},
    DiagnosticEntry{DiagnosticId::InvalidOffsetof, "InvalidOffsetof"},
    DiagnosticEntry{DiagnosticId::InvalidAllocationType, "InvalidAllocationType"},
    DiagnosticEntry{DiagnosticId::InvalidDeleteOperand, "InvalidDeleteOperand"},
    DiagnosticEntry{DiagnosticId::InvalidStackalloc, "InvalidStackalloc"},
    DiagnosticEntry{DiagnosticId::InvalidInlineAssemblyConstraint, "InvalidInlineAssemblyConstraint"},
    DiagnosticEntry{DiagnosticId::InvalidInlineAssemblyClobber, "InvalidInlineAssemblyClobber"},
    DiagnosticEntry{DiagnosticId::InvalidInlineAssemblyControlFlow, "InvalidInlineAssemblyControlFlow"},
    DiagnosticEntry{DiagnosticId::InvalidInlineAssemblyTarget, "InvalidInlineAssemblyTarget"},
    DiagnosticEntry{DiagnosticId::InvalidInlineAssemblyPlaceholder, "InvalidInlineAssemblyPlaceholder"},
    DiagnosticEntry{DiagnosticId::InlineAssemblyNotAvailable, "InlineAssemblyNotAvailable"},
    DiagnosticEntry{DiagnosticId::PlacementConstructionNotAvailable, "PlacementConstructionNotAvailable"},
    DiagnosticEntry{DiagnosticId::ConstantEvaluationTrap, "ConstantEvaluationTrap"},
    DiagnosticEntry{DiagnosticId::MissingMain, "MissingMain"},
    DiagnosticEntry{DiagnosticId::InvalidMainSignature, "InvalidMainSignature"},
    DiagnosticEntry{DiagnosticId::InvalidExternCSignature, "InvalidExternCSignature"},
    // EN: Freeze 3.0 diagnostics, appended after NS3099 with the values 74, 75 and 76.
    // ES: Diagnosticos del congelamiento 3.0, agregados despues de NS3099 con los valores 74, 75 y 76.
    DiagnosticEntry{DiagnosticId::LostOwnership, "LostOwnership"},
    DiagnosticEntry{DiagnosticId::SynchronousAwaitCanSuspend, "SynchronousAwaitCanSuspend"},
    DiagnosticEntry{DiagnosticId::UnhandledDeclaredError, "UnhandledDeclaredError"},
};

// EN: Count of the historical ordinary IDs (values 0..68); the short codes follow with 69..73 and the
//     freeze diagnostics are appended after them.
// ES: Cantidad de IDs ordinarios historicos (valores 0..68); los codigos cortos siguen con 69..73 y los
//     diagnosticos del congelamiento se agregan despues.
constexpr std::size_t kHistoricalOrdinaryCount = 69;
constexpr std::size_t kFirstShortCodeValue = 69;
constexpr std::size_t kFirstFreezeValue = 74;

// EN: Returns the frozen numeric value of the ordinary entry at `index`. Complexity is O(1).
// ES: Devuelve el valor numerico congelado de la entrada ordinaria en `index`. Complejidad O(1).
constexpr std::size_t ordinaryValue(std::size_t index) noexcept
{
    return index < kHistoricalOrdinaryCount ? index : kFirstFreezeValue + (index - kHistoricalOrdinaryCount);
}

constexpr std::array kNativeSharp3Entries{
    DiagnosticEntry{DiagnosticId::NS3010InvalidCompilationStage, "NS3010"},
    DiagnosticEntry{DiagnosticId::NS3020InvalidValueCategory, "NS3020"},
    DiagnosticEntry{DiagnosticId::NS3030MissingEffectContext, "NS3030"},
    DiagnosticEntry{DiagnosticId::NS3049DeferredLanguageSurface, "NS3049"},
    DiagnosticEntry{DiagnosticId::NS3099InvalidConformanceManifest, "NS3099"},
};

// Registers one case under the fixed diagnostic identity contract. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-002", "Host.Diagnostics", "unit",
        "PR", {"diagnostics", "identity"}, std::move(function)});
}

// Verifies every stable name and name uniqueness using an independent table. Complexity is O(N).
void registerCatalogCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-003", [](TestContext& context) {
        std::unordered_set<std::string_view> names;
        names.reserve(kOrdinaryEntries.size() + kNativeSharp3Entries.size());
        for (const DiagnosticEntry& entry : kOrdinaryEntries)
        {
            context.expect(diagnosticIdName(entry.first) == entry.second,
                "TD-DIA-003 every ordinary diagnostic ID must preserve its exact name");
            context.expect(names.insert(entry.second).second,
                "TD-DIA-003 every diagnostic name must be unique");
        }
        for (const DiagnosticEntry& entry : kNativeSharp3Entries)
        {
            context.expect(diagnosticIdName(entry.first) == entry.second,
                "TD-DIA-003 every short diagnostic code must preserve its exact name");
            context.expect(names.insert(entry.second).second,
                "TD-DIA-003 every diagnostic name must be unique");
        }
        context.expect(names.size() == 77, "TD-DIA-003 the independent catalog must contain 77 names");
    });
}

// Verifies the ordinary and short-code classification partitions. Complexity is O(N).
void registerClassificationCases(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-004", [](TestContext& context) {
        context.expect(kOrdinaryEntries.size() == 72,
            "TD-DIA-004 the ordinary diagnostic partition must contain exactly 72 IDs");
        for (std::size_t index = 0; index < kOrdinaryEntries.size(); ++index)
        {
            const DiagnosticEntry& entry = kOrdinaryEntries[index];
            context.expect(!isNativeSharp3Diagnostic(entry.first),
                "TD-DIA-004 every ordinary diagnostic ID must remain outside the short-code subset");
            context.expect(static_cast<std::size_t>(entry.first) == ordinaryValue(index),
                "TD-DIA-004 every ordinary diagnostic ID must keep its frozen numeric value");
        }
    });
    addCase(registry, "TD-DIA-005", [](TestContext& context) {
        context.expect(kNativeSharp3Entries.size() == 5,
            "TD-DIA-005 the short-code diagnostic partition must contain exactly five IDs");
        for (std::size_t index = 0; index < kNativeSharp3Entries.size(); ++index)
        {
            const DiagnosticEntry& entry = kNativeSharp3Entries[index];
            context.expect(isNativeSharp3Diagnostic(entry.first),
                "TD-DIA-005 every short-code diagnostic ID must remain classified");
            context.expect(diagnosticIdName(entry.first) == entry.second,
                "TD-DIA-005 every classified ID must preserve its short code");
            context.expect(static_cast<std::size_t>(entry.first) == kFirstShortCodeValue + index,
                "TD-DIA-005 every short-code diagnostic ID must keep its historical value 69..73");
        }
    });
}

// Verifies the frozen unknown-value fallback without weakening analyzer policy. Complexity is O(1).
void registerUnknownCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-006", [](TestContext& context) {
        // The frozen API intentionally defines behavior outside the enumerator set.
        // EN: Freeze 3.0: the largest int is outside the enumerator domain no matter how many IDs are
        //     appended later; 74 stopped being unknown once the freeze diagnostics were added.
        // ES: Congelamiento 3.0: el int mas grande queda fuera del dominio de enumeradores sin importar
        //     cuantos IDs se agreguen despues; 74 dejo de ser desconocido al agregar los del congelamiento.
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
        constexpr auto unknown = static_cast<DiagnosticId>((std::numeric_limits<int>::max)());
        context.expect(diagnosticIdName(unknown) == "UnknownDiagnostic",
            "TD-DIA-006 an unknown diagnostic ID must use the stable fallback name");
        context.expect(!isNativeSharp3Diagnostic(unknown),
            "TD-DIA-006 an unknown diagnostic ID must not be classified as a short code");
    });
}
}

// Registers the complete CD-DIA-002 case set. The registry owns each callable for the process lifetime.
void registerDiagnosticIdentityModule(TestRegistry& registry)
{
    registerCatalogCase(registry);
    registerClassificationCases(registry);
    registerUnknownCase(registry);
}
}
