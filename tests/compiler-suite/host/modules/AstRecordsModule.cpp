#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"

#include <array>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::ast::FfiUnwindSyntaxKind;
using nativesharp::ast::FloatingPointModeSyntaxKind;
using nativesharp::ast::GenericConstraintClause;
using nativesharp::ast::GenericParameterSyntax;
using nativesharp::ast::GenericParameterSyntaxKind;
using nativesharp::ast::LifetimeOutlivesConstraint;
using nativesharp::ast::LifetimeParameterSyntax;
using nativesharp::ast::TypeSyntax;
using nativesharp::lexer::TokenKind;

// Builds a physical source range with explicit file and byte offsets. Complexity is O(1).
constexpr SourceRange makeRange(std::uint32_t file, std::uint32_t begin, std::uint32_t end)
{
    return SourceRange(SourceLocation(FileId{file}, begin), SourceLocation(FileId{file}, end));
}

// Compares every observable SourceRange coordinate without relying on hidden representation. Complexity is O(1).
constexpr bool sameRange(SourceRange left, SourceRange right)
{
    return left.begin() == right.begin() && left.end() == right.end();
}

// Registers one records contract case with the canonical Host.AST metadata. Complexity is O(1).
void addRecordCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{
        std::move(id), "CA-AST-004", "Host.AST", "unit", "PR", {"ast", "records"},
        std::move(function),
    });
}

// Registers default construction of GenericParameterSyntax. Complexity is O(1).
void registerGenericDefaultCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-027", [](TestContext& context) {
        const GenericParameterSyntax value;
        context.expect(value.name.empty(), "TA-AST-027 default name must be empty");
        context.expect(value.kind == GenericParameterSyntaxKind::Type, "TA-AST-027 default kind must be Type");
        context.expect(value.constantType == nullptr, "TA-AST-027 default constant type must be null");
        context.expect(!value.range.isValid(), "TA-AST-027 default range must be invalid");
    });
}

// Registers the exact declaration order of generic parameter kinds. Complexity is O(n).
void registerGenericKindsCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-028", [](TestContext& context) {
        constexpr std::array kinds{GenericParameterSyntaxKind::Type, GenericParameterSyntaxKind::Constant,
            GenericParameterSyntaxKind::TypePack, GenericParameterSyntaxKind::ConstantPack};
        for (std::size_t index = 0; index < kinds.size(); ++index)
        {
            context.expect(static_cast<std::size_t>(kinds[index]) == index,
                "TA-AST-028 generic parameter kinds must remain distinct and ordered");
        }
    });
}

// Registers copy and move preservation for a fully populated generic record. Complexity is O(1).
void registerGenericRecordCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-029", [](TestContext& context) {
        TypeSyntax type(makeRange(29, 1, 4), TokenKind::KwInt, "int");
        const SourceRange range = makeRange(29, 5, 12);
        GenericParameterSyntax original{"Count", GenericParameterSyntaxKind::Constant, &type, range};
        GenericParameterSyntax copied = original;
        GenericParameterSyntax moved = std::move(copied);
        context.expect(moved.name == "Count", "TA-AST-029 moved name must be preserved");
        context.expect(moved.kind == GenericParameterSyntaxKind::Constant, "TA-AST-029 moved kind must be preserved");
        context.expect(moved.constantType == &type, "TA-AST-029 moved pointer identity must be preserved");
        context.expect(sameRange(moved.range, range), "TA-AST-029 moved range must be preserved");
    });
}

// Registers the two owned fields of a lifetime parameter record. Complexity is O(1).
void registerLifetimeParameterCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-030", [](TestContext& context) {
        const SourceRange range = makeRange(30, 2, 7);
        const LifetimeParameterSyntax value{"scope", range};
        context.expect(value.name == "scope", "TA-AST-030 lifetime name must be exact");
        context.expect(sameRange(value.range, range), "TA-AST-030 lifetime range must be exact");
    });
}

// Registers all observable fields of an outlives record. Complexity is O(1).
void registerOutlivesCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-031", [](TestContext& context) {
        const SourceRange range = makeRange(31, 8, 17);
        const LifetimeOutlivesConstraint value{"longer", "shorter", range};
        context.expect(value.longer == "longer", "TA-AST-031 longer lifetime must be exact");
        context.expect(value.shorter == "shorter", "TA-AST-031 shorter lifetime must be exact");
        context.expect(sameRange(value.range, range), "TA-AST-031 outlives range must be exact");
    });
}

// Registers the exact declaration order of floating-point modes. Complexity is O(n).
void registerFloatingModesCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-032", [](TestContext& context) {
        constexpr std::array modes{FloatingPointModeSyntaxKind::Inherit, FloatingPointModeSyntaxKind::Strict,
            FloatingPointModeSyntaxKind::Precise, FloatingPointModeSyntaxKind::Fast};
        for (std::size_t index = 0; index < modes.size(); ++index)
        {
            context.expect(static_cast<std::size_t>(modes[index]) == index,
                "TA-AST-032 floating-point modes must remain distinct and ordered");
        }
    });
}

// Registers the exact declaration order of unwind policies. Complexity is O(n).
void registerUnwindPoliciesCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-033", [](TestContext& context) {
        constexpr std::array policies{
            FfiUnwindSyntaxKind::Default, FfiUnwindSyntaxKind::Forbidden, FfiUnwindSyntaxKind::Abort};
        for (std::size_t index = 0; index < policies.size(); ++index)
        {
            context.expect(static_cast<std::size_t>(policies[index]) == index,
                "TA-AST-033 unwind policies must remain distinct and ordered");
        }
    });
}

// Registers every default of GenericConstraintClause. Complexity is O(1).
void registerConstraintDefaultsCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-034", [](TestContext& context) {
        const GenericConstraintClause value;
        const bool flags = value.requiresClass || value.requiresValueType || value.requiresDefaultConstructor ||
            value.requiresCopy || value.requiresMove || value.requiresTrackedLifetime || value.requiresUnmanaged ||
            value.requiresNotNull;
        context.expect(value.parameterName.empty(), "TA-AST-034 parameter name must default empty");
        context.expect(!flags, "TA-AST-034 capability flags must default false");
        context.expect(value.equalityConstraint == nullptr, "TA-AST-034 equality pointer must default null");
        context.expect(value.associatedProjection == nullptr, "TA-AST-034 projection pointer must default null");
        context.expect(value.typeConstraints.empty(), "TA-AST-034 type constraints must default empty");
        context.expect(value.substitutedArgument == nullptr, "TA-AST-034 substituted pointer must default null");
    });
}

// Registers each constraint capability in isolation to prove orthogonality. Complexity is O(1).
void registerIsolatedCapabilitiesCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-035", [](TestContext& context) {
        using Member = bool GenericConstraintClause::*;
        constexpr std::array<Member, 8> members{&GenericConstraintClause::requiresClass,
            &GenericConstraintClause::requiresValueType, &GenericConstraintClause::requiresDefaultConstructor,
            &GenericConstraintClause::requiresCopy, &GenericConstraintClause::requiresMove,
            &GenericConstraintClause::requiresTrackedLifetime, &GenericConstraintClause::requiresUnmanaged,
            &GenericConstraintClause::requiresNotNull};
        for (std::size_t selected = 0; selected < members.size(); ++selected)
        {
            GenericConstraintClause value;
            value.*members[selected] = true;
            for (std::size_t index = 0; index < members.size(); ++index)
            {
                context.expect(value.*members[index] == (index == selected),
                    "TA-AST-035 only the selected capability may be true");
            }
        }
    });
}

// Registers simultaneous activation of all independent constraint capabilities. Complexity is O(1).
void registerAllCapabilitiesCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-036", [](TestContext& context) {
        GenericConstraintClause value;
        value.requiresClass = value.requiresValueType = value.requiresDefaultConstructor = true;
        value.requiresCopy = value.requiresMove = value.requiresTrackedLifetime = true;
        value.requiresUnmanaged = value.requiresNotNull = true;
        context.expect(value.requiresClass && value.requiresValueType && value.requiresDefaultConstructor,
            "TA-AST-036 first capability group must remain true");
        context.expect(value.requiresCopy && value.requiresMove && value.requiresTrackedLifetime,
            "TA-AST-036 second capability group must remain true");
        context.expect(value.requiresUnmanaged && value.requiresNotNull,
            "TA-AST-036 final capability group must remain true");
    });
}

// Registers pointer identity for equality, projection, and substituted argument channels. Complexity is O(1).
void registerConstraintPointersCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-037", [](TestContext& context) {
        TypeSyntax equality(makeRange(37, 1, 2), TokenKind::KwInt, "E");
        TypeSyntax projection(makeRange(37, 3, 4), TokenKind::Identifier, "P");
        TypeSyntax substituted(makeRange(37, 5, 6), TokenKind::Identifier, "S");
        GenericConstraintClause value;
        value.equalityConstraint = &equality;
        value.associatedProjection = &projection;
        value.substitutedArgument = &substituted;
        context.expect(value.equalityConstraint == &equality, "TA-AST-037 equality identity must be exact");
        context.expect(value.associatedProjection == &projection, "TA-AST-037 projection identity must be exact");
        context.expect(value.substitutedArgument == &substituted, "TA-AST-037 substitution identity must be exact");
    });
}

// Registers preservation of nulls and duplicates in the type-constraint vector. Complexity is O(1).
void registerTypeConstraintVectorCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-038", [](TestContext& context) {
        TypeSyntax type(makeRange(38, 1, 2), TokenKind::Identifier, "T");
        GenericConstraintClause value;
        value.typeConstraints = {nullptr, &type, &type, nullptr};
        context.expect(value.typeConstraints.size() == 4, "TA-AST-038 vector size must be exact");
        context.expect(value.typeConstraints[0] == nullptr && value.typeConstraints[3] == nullptr,
            "TA-AST-038 null entries must be preserved");
        context.expect(value.typeConstraints[1] == &type && value.typeConstraints[2] == &type,
            "TA-AST-038 duplicate pointer entries must be preserved");
    });
}

// Registers independence of parameter and whole-clause ranges. Complexity is O(1).
void registerIndependentRangesCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-039", [](TestContext& context) {
        const SourceRange parameterRange = makeRange(39, 3, 4);
        const SourceRange clauseRange = makeRange(39, 1, 20);
        GenericConstraintClause value;
        value.parameterRange = parameterRange;
        value.range = clauseRange;
        context.expect(sameRange(value.parameterRange, parameterRange), "TA-AST-039 parameter range must be exact");
        context.expect(sameRange(value.range, clauseRange), "TA-AST-039 clause range must be exact");
    });
}

// Registers that syntax records preserve semantically incoherent combinations without validation. Complexity O(1).
void registerIncoherentRecordCase(TestRegistry& registry)
{
    addRecordCase(registry, "TA-AST-040", [](TestContext& context) {
        TypeSyntax type(makeRange(40, 1, 2), TokenKind::KwInt, "int");
        GenericParameterSyntax parameter{"", GenericParameterSyntaxKind::Type, &type, makeRange(40, 9, 2)};
        GenericConstraintClause clause;
        clause.parameterName = "missing";
        clause.requiresClass = true;
        clause.requiresValueType = true;
        clause.typeConstraints = {nullptr, &type};
        context.expect(parameter.name.empty() && parameter.constantType == &type,
            "TA-AST-040 generic record must preserve incoherent fields");
        context.expect(!parameter.range.isValid(), "TA-AST-040 invalid range must remain invalid");
        context.expect(clause.requiresClass && clause.requiresValueType && clause.typeConstraints.size() == 2,
            "TA-AST-040 constraint record must not normalize incoherent fields");
    });
}
}

// Registers CA-AST-004 records and enum contracts without involving parser or semantic validation.
void registerAstRecordsModule(TestRegistry& registry)
{
    registerGenericDefaultCase(registry);
    registerGenericKindsCase(registry);
    registerGenericRecordCase(registry);
    registerLifetimeParameterCase(registry);
    registerOutlivesCase(registry);
    registerFloatingModesCase(registry);
    registerUnwindPoliciesCase(registry);
    registerConstraintDefaultsCase(registry);
    registerIsolatedCapabilitiesCase(registry);
    registerAllCapabilitiesCase(registry);
    registerConstraintPointersCase(registry);
    registerTypeConstraintVectorCase(registry);
    registerIndependentRangesCase(registry);
    registerIncoherentRecordCase(registry);
}
}
