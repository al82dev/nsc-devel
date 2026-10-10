#include "SemaTestSupport.h"

#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/ConversionInfo.h"
#include "nativesharp/Sema/OwnershipKind.h"
#include "nativesharp/Sema/TypeContext.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::sema;
using sema_support::addSemaCase;

void c001(TestContext& c)
{
    const ConstantValue v;
    c.expect(v.kind() == ConstantKind::None && v.signedInteger() == 0 && v.unsignedInteger() == 0 &&
            v.floating() == 0.0 && !v.boolean() && v.string().empty(),
        "TS-SEMA-001 default constant has exact passive state");
}

void c002(TestContext& c)
{
    for (const std::int64_t x : {std::numeric_limits<std::int64_t>::min(), std::int64_t{-1},
             std::int64_t{0}, std::int64_t{1}, std::numeric_limits<std::int64_t>::max()})
    {
        const ConstantValue v = ConstantValue::signedInteger(x);
        c.expect(v.kind() == ConstantKind::SignedInteger && v.signedInteger() == x,
            "TS-SEMA-002 signed factory preserves the exact payload");
    }
}

void c003(TestContext& c)
{
    for (const std::uint64_t x : {std::uint64_t{0}, std::uint64_t{1},
             std::numeric_limits<std::uint64_t>::max()})
    {
        const ConstantValue v = ConstantValue::unsignedInteger(x);
        c.expect(v.kind() == ConstantKind::UnsignedInteger && v.unsignedInteger() == x,
            "TS-SEMA-003 unsigned factory preserves the exact payload");
    }
}

void c004(TestContext& c)
{
    const std::array<double, 7> values = {0.0, -0.0, 1.5, -2.25,
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()};
    for (const double x : values)
    {
        const double actual = ConstantValue::floating(x).floating();
        const bool exact = std::isnan(x) ? std::isnan(actual)
            : actual == x && std::signbit(actual) == std::signbit(x);
        c.expect(exact, "TS-SEMA-004 floating factory preserves its IEEE payload");
    }
}

void c005(TestContext& c)
{
    const ConstantValue f = ConstantValue::boolean(false);
    const ConstantValue t = ConstantValue::boolean(true);
    const ConstantValue n = ConstantValue::nullValue();
    c.expect(f.kind() == ConstantKind::Boolean && !f.boolean() && t.boolean() &&
            n.kind() == ConstantKind::Null,
        "TS-SEMA-005 boolean and null factories preserve discriminants and payloads");
}

void c006(TestContext& c)
{
    const std::array<std::string, 4> values = {"", "ASCII", "caf\xC3\xA9", std::string("a\0b", 3)};
    for (const std::string& x : values)
        c.expect(ConstantValue::stringValue(x).string() == x,
            "TS-SEMA-006 string factory preserves every source byte");
}

void c007(TestContext& c)
{
    const ConstantValue v = ConstantValue::boolean(true);
    c.expect(v.boolean() && v.signedInteger() == 0 && v.unsignedInteger() == 0 &&
            v.floating() == 0.0 && v.string().empty(),
        "TS-SEMA-007 inactive constant channels remain zero-initialized and unguarded");
}

void c008(TestContext& c)
{
    ConstantValue source = ConstantValue::stringValue(std::string("x\0y", 3));
    const ConstantValue copy = source;
    const ConstantValue moved = std::move(source);
    c.expect(copy.string() == std::string("x\0y", 3) && moved.string() == std::string("x\0y", 3),
        "TS-SEMA-008 copy and move preserve discriminant and payload");
}

void c009(TestContext& c)
{
    const std::array<std::pair<ConversionKind, std::string_view>, 13> rows = {{
        {ConversionKind::Identity, "identity"}, {ConversionKind::IntegralPromotion, "integral-promotion"},
        {ConversionKind::ImplicitConstantInteger, "implicit-constant-integer"},
        {ConversionKind::ImplicitNumeric, "implicit-numeric"},
        {ConversionKind::ImplicitNullPointer, "implicit-null-pointer"},
        {ConversionKind::ImplicitPointerToVoid, "implicit-pointer-to-void"},
        {ConversionKind::ImplicitStringLiteralToCharPointer, "implicit-string-literal-to-char-pointer"},
        {ConversionKind::ExplicitNumeric, "explicit-numeric"},
        {ConversionKind::ExplicitEnum, "explicit-enum"},
        {ConversionKind::ExplicitPointer, "explicit-pointer"},
        {ConversionKind::ExplicitPointerInteger, "explicit-pointer-integer"},
        {ConversionKind::ExplicitNullPointer, "explicit-null-pointer"},
        {ConversionKind::ToVoid, "to-void"}}};
    for (const auto& [kind, name] : rows)
        c.expect(ToString(kind) == name, "TS-SEMA-009 conversion names match the exact 13-row table");
}

void c010(TestContext& c)
{
    c.expect(ToString(static_cast<ConversionKind>(999)) == "unknown",
        "TS-SEMA-010 external conversion kinds use the unknown fallback");
}

void c011(TestContext& c)
{
    const ConversionInfo i;
    c.expect(i.kind == ConversionKind::Identity && i.role == ConversionRole::Contextual &&
            i.operand == nullptr && i.sourceType == nullptr && i.destinationType == nullptr &&
            i.operandIndex == -1,
        "TS-SEMA-011 default ConversionInfo preserves all canonical fields");
}

void c012(TestContext& c)
{
    TypeContext types;
    const std::array<ConversionRole, 3> roles = {
        ConversionRole::Contextual, ConversionRole::Operand, ConversionRole::Result};
    for (int index = 0; index < 3; ++index)
    {
        ConversionInfo i;
        i.kind = ConversionKind::ExplicitNumeric;
        i.role = roles[static_cast<std::size_t>(index)];
        i.sourceType = types.builtin(TypeKind::Int);
        i.destinationType = types.builtin(TypeKind::Long);
        i.operandIndex = index;
        c.expect(i.role == roles[index] && i.operandIndex == index && i.sourceType != i.destinationType,
            "TS-SEMA-012 conversion role, index, and types remain exact");
    }
}

void c013(TestContext& c)
{
    ConversionInfo i;
    i.role = ConversionRole::Operand;
    i.operandIndex = 4;
    c.expect(i.operand == nullptr && i.sourceType == nullptr && i.destinationType == nullptr &&
            i.role == ConversionRole::Operand && i.operandIndex == 4,
        "TS-SEMA-013 partial-null conversion records are not normalized");
}

void c014(TestContext& c)
{
    for (const ConversionKind kind : {ConversionKind::Identity, ConversionKind::ImplicitNumeric,
             ConversionKind::ExplicitPointer})
        for (const ConversionRole role : {ConversionRole::Contextual, ConversionRole::Operand,
                 ConversionRole::Result})
        {
            ConversionInfo i;
            i.kind = kind;
            i.role = role;
            c.expect(i.kind == kind && i.role == role,
                "TS-SEMA-014 conversion classification and role remain independent");
        }
}

void c015(TestContext& c)
{
    c.expect(ownershipKindName(OwnershipKind::None) == "none" &&
            ownershipKindName(OwnershipKind::Owned) == "owned" &&
            ownershipKindName(OwnershipKind::Borrowed) == "borrowed",
        "TS-SEMA-015 ownership names match the exact table");
}

void c016(TestContext& c)
{
    c.expect(ownershipKindName(static_cast<OwnershipKind>(99)) == "none",
        "TS-SEMA-016 external ownership values use the none fallback");
}

void c017(TestContext& c)
{
    OwnershipKind value = OwnershipKind::None;
    value = OwnershipKind::Owned;
    c.expect(value == OwnershipKind::Owned,
        "TS-SEMA-017 default and assigned ownership values retain enum identity");
}
}

void registerSemaConstantConversionModule(TestRegistry& registry)
{
    const std::array<TestFunction, 17> tests = {c001, c002, c003, c004, c005, c006, c007, c008,
        c009, c010, c011, c012, c013, c014, c015, c016, c017};
    for (std::size_t index = 0; index < tests.size(); ++index)
    {
        const std::size_t ordinal = index + 1;
        const std::string id = "TS-SEMA-" + std::string(ordinal < 10 ? "00" : "0") +
            std::to_string(ordinal);
        const std::string contract = ordinal <= 8 ? "CS-SEMA-001"
            : ordinal <= 14 ? "CS-SEMA-002" : "CS-SEMA-003";
        addSemaCase(registry, id, contract, "unit", tests[index]);
    }
}
}
