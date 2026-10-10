#include "SemaTestSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeContext.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::sema;
using sema_support::addSemaCase;
constexpr std::array<TypeKind, 22> kinds = {TypeKind::Error, TypeKind::Void, TypeKind::Bool,
    TypeKind::Byte, TypeKind::Char, TypeKind::Short, TypeKind::UShort, TypeKind::Int,
    TypeKind::UInt, TypeKind::Long, TypeKind::ULong, TypeKind::NInt, TypeKind::NUInt,
    TypeKind::Float, TypeKind::Double, TypeKind::Pointer, TypeKind::Array, TypeKind::Struct,
    TypeKind::Enum, TypeKind::Function, TypeKind::NullLiteral, TypeKind::StringLiteral};

void c054(TestContext& c)
{
    const std::array<std::string_view, 22> names = {"<error>", "void", "bool", "byte", "char",
        "short", "ushort", "int", "uint", "long", "ulong", "nint", "nuint", "float",
        "double", "pointer", "array", "struct", "enum", "function", "null", "string-literal"};
    bool exact = true;
    for (std::size_t index = 0; index < kinds.size(); ++index)
        exact = exact && typeKindName(kinds[index]) == names[index];
    c.expect(exact, "TS-SEMA-054 all 22 type kind names match the exact table");
}

void c055(TestContext& c)
{
    c.expect(typeKindName(static_cast<TypeKind>(99)) == "<error>",
        "TS-SEMA-055 an external TypeKind uses the error fallback");
}

void c056(TestContext& c)
{
    for (const TypeKind kind : kinds)
    {
        const Type type(kind);
        c.expect(type.kind() == kind && type.ownership() == OwnershipKind::None &&
                !type.isOwnershipQualified() && type.unqualifiedType() == &type &&
                type.name().empty() && type.elementType() == nullptr && type.count() == 0 &&
                type.returnType() == nullptr && type.parameterTypes().empty() && type.isComplete() &&
                type.declaration() == nullptr,
            "TS-SEMA-056 each direct Type construction preserves canonical defaults");
    }
}

void c057(TestContext& c)
{
    Type base(TypeKind::Struct);
    Type owned(&base, OwnershipKind::Owned);
    Type borrowed(&base, OwnershipKind::Borrowed);
    c.expect(owned.kind() == TypeKind::Struct && borrowed.kind() == TypeKind::Struct &&
            owned.unqualifiedType() == &base && borrowed.unqualifiedType() == &base &&
            owned.ownership() == OwnershipKind::Owned && borrowed.ownership() == OwnershipKind::Borrowed,
        "TS-SEMA-057 Owned and Borrowed wrappers retain kind, base, and ownership");
}

void c058(TestContext& c)
{
    Type base(TypeKind::Int);
    Type wrapper(&base, OwnershipKind::Owned);
    const Type& constWrapper = wrapper;
    c.expect(base.unqualifiedType() == &base && wrapper.unqualifiedType() == &base &&
            constWrapper.unqualifiedType() == &base,
        "TS-SEMA-058 const and mutable unqualifiedType return self or the exact shared base");
}

void c059(TestContext& c)
{
    Type type(TypeKind::Struct);
    Type element(TypeKind::Int);
    Type result(TypeKind::Void);
    nativesharp::ast::AstNode declaration(nativesharp::ast::AstNodeKind::StructDeclaration, {});
    type.setName("Box"); type.setElementType(&element); type.setCount(9); type.setReturnType(&result);
    type.setParameterTypes({&element, &result}); type.setComplete(false); type.setDeclaration(&declaration);
    c.expect(type.name() == "Box" && type.elementType() == &element && type.count() == 9 &&
            type.returnType() == &result && type.parameterTypes() == std::vector<Type*>({&element, &result}) &&
            !type.isComplete() && type.declaration() == &declaration,
        "TS-SEMA-059 each unqualified type setter changes its corresponding field");
}

void c060(TestContext& c)
{
    Type base(TypeKind::Struct);
    Type wrapper(&base, OwnershipKind::Owned);
    Type element(TypeKind::Int);
    wrapper.setName("Shared"); wrapper.setElementType(&element); wrapper.setCount(4); wrapper.setComplete(false);
    c.expect(base.name() == "Shared" && base.elementType() == &element && base.count() == 4 &&
            !base.isComplete(),
        "TS-SEMA-060 setters through a qualified wrapper delegate to the shared base");
}

void c061(TestContext& c)
{
    TypeContext types;
    const std::array<TypeKind, 15> builtins = {TypeKind::Error, TypeKind::Void, TypeKind::Bool,
        TypeKind::Byte, TypeKind::Char, TypeKind::Short, TypeKind::UShort, TypeKind::Int,
        TypeKind::UInt, TypeKind::Long, TypeKind::ULong, TypeKind::NInt, TypeKind::NUInt,
        TypeKind::Float, TypeKind::Double};
    bool exact = true;
    for (const TypeKind kind : builtins)
        exact = exact && types.builtin(kind)->displayName() == typeKindName(kind);
    Type* nominal = types.createStruct("Packet", nullptr);
    exact = exact && nominal->displayName() == "Packet";
    c.expect(exact, "TS-SEMA-061 builtin, error, and nominal display names are exact");
}

void c062(TestContext& c)
{
    TypeContext types;
    c.expect(types.pointerTo(types.builtin(TypeKind::Int))->displayName() == "int*" &&
            types.arrayOf(types.builtin(TypeKind::Byte), 7)->displayName() == "byte[7]" &&
            types.stringLiteral(5)->displayName() == "StringLiteralType<5>",
        "TS-SEMA-062 pointer, array, and string-literal display composition is exact");
}

void c063(TestContext& c)
{
    TypeContext types;
    Type* zero = types.functionType(types.builtin(TypeKind::Void), {});
    Type* one = types.functionType(types.builtin(TypeKind::Int), {types.builtin(TypeKind::Byte)});
    Type* many = types.functionType(types.builtin(TypeKind::Long),
        {types.builtin(TypeKind::Int), types.builtin(TypeKind::Double)});
    c.expect(zero->displayName() == "() -> void" && one->displayName() == "(byte) -> int" &&
            many->displayName() == "(int, double) -> long",
        "TS-SEMA-063 function display preserves zero, one, and ordered multiple parameters");
}

void c064(TestContext& c)
{
    nativesharp::ast::AstNode declaration(nativesharp::ast::AstNodeKind::StructDeclaration, {});
    Type base(TypeKind::Struct);
    Type wrapper(&base, OwnershipKind::Borrowed);
    base.setComplete(false); base.setDeclaration(&declaration);
    const bool initial = !wrapper.isComplete() && wrapper.declaration() == &declaration;
    wrapper.setComplete(true); wrapper.setDeclaration(nullptr);
    c.expect(initial && base.isComplete() && base.declaration() == nullptr,
        "TS-SEMA-064 nominal completeness and declaration delegate through wrappers");
}

void c065(TestContext& c)
{
    Type type(TypeKind::Array);
    bool exact = true;
    for (const std::uint64_t count : {std::uint64_t{0}, std::uint64_t{1},
             std::numeric_limits<std::uint64_t>::max()})
    {
        type.setCount(count);
        exact = exact && type.count() == count;
    }
    c.expect(exact, "TS-SEMA-065 count payload preserves zero, one, and maximum");
}

void c066(TestContext& c)
{
    TypeContext types;
    bool exact = true;
    for (const TypeKind kind : {TypeKind::Error, TypeKind::Void, TypeKind::Bool, TypeKind::Byte,
             TypeKind::Char, TypeKind::Short, TypeKind::UShort, TypeKind::Int, TypeKind::UInt,
             TypeKind::Long, TypeKind::ULong, TypeKind::NInt, TypeKind::NUInt, TypeKind::Float,
             TypeKind::Double, TypeKind::NullLiteral})
        exact = exact && types.builtin(kind) != nullptr && types.builtin(kind) == types.builtin(kind);
    c.expect(exact, "TS-SEMA-066 TypeContext creates stable non-null required builtins");
}

void c067(TestContext& c)
{
    TypeContext types;
    bool exact = true;
    for (const TypeKind kind : kinds)
    {
        const bool expected = kind <= TypeKind::Double || kind == TypeKind::NullLiteral;
        exact = exact && (types.builtin(kind) != nullptr) == expected;
    }
    c.expect(exact, "TS-SEMA-067 builtin returns pointers or null according to the exact table");
}

void c068(TestContext& c)
{
    TypeContext types;
    c.expect(types.builtin(static_cast<TypeKind>(999)) == nullptr,
        "TS-SEMA-068 an external builtin kind returns null without indexing out of range");
}

void c069(TestContext& c)
{
    TypeContext types;
    Type* integer = types.builtin(TypeKind::Int);
    Type* error = types.builtin(TypeKind::Error);
    c.expect(types.qualified(nullptr, OwnershipKind::Owned) == error &&
            types.qualified(integer, OwnershipKind::None) == integer &&
            types.qualified(error, OwnershipKind::Owned) == error,
        "TS-SEMA-069 null, None, and Error qualification preserve exact identity rules");
}

void c070(TestContext& c)
{
    TypeContext types;
    Type* base = types.createStruct("Resource", nullptr);
    Type* first = types.qualified(base, OwnershipKind::Owned);
    c.expect(first == types.qualified(base, OwnershipKind::Owned) &&
            first == types.qualified(first, OwnershipKind::Owned),
        "TS-SEMA-070 repeated same-base qualification reuses interned identity");
}

void c071(TestContext& c)
{
    TypeContext types;
    Type* base = types.createStruct("Resource", nullptr);
    Type* owned = types.qualified(base, OwnershipKind::Owned);
    Type* borrowed = types.qualified(base, OwnershipKind::Borrowed);
    c.expect(owned != borrowed && owned->unqualifiedType() == base && borrowed->unqualifiedType() == base,
        "TS-SEMA-071 Owned and Borrowed wrappers are distinct over one shared base");
}

void c072(TestContext& c)
{
    TypeContext types;
    Type* integer = types.builtin(TypeKind::Int);
    Type* byte = types.builtin(TypeKind::Byte);
    c.expect(types.pointerTo(integer) == types.pointerTo(integer) &&
            types.pointerTo(integer) != types.pointerTo(byte),
        "TS-SEMA-072 pointer interning depends exactly on pointee identity");
}

void c073(TestContext& c)
{
    TypeContext types;
    Type* integer = types.builtin(TypeKind::Int);
    c.expect(types.arrayOf(integer, 4) == types.arrayOf(integer, 4) &&
            types.arrayOf(integer, 4) != types.arrayOf(integer, 5) &&
            types.arrayOf(integer, 4) != types.arrayOf(types.builtin(TypeKind::Byte), 4),
        "TS-SEMA-073 array interning requires equal element and count");
}

void c074(TestContext& c)
{
    TypeContext types;
    c.expect(types.stringLiteral(0) == types.stringLiteral(0) &&
            types.stringLiteral(0) != types.stringLiteral(1) &&
            types.stringLiteral(1) != types.stringLiteral(17),
        "TS-SEMA-074 string-literal types intern exactly by count");
}

void c075(TestContext& c)
{
    TypeContext types;
    Type* first = types.functionType(types.builtin(TypeKind::Int),
        {types.builtin(TypeKind::Byte), types.builtin(TypeKind::Long)});
    c.expect(first == types.functionType(types.builtin(TypeKind::Int),
            {types.builtin(TypeKind::Byte), types.builtin(TypeKind::Long)}) &&
            first != types.functionType(types.builtin(TypeKind::Long),
                {types.builtin(TypeKind::Byte), types.builtin(TypeKind::Long)}) &&
            first != types.functionType(types.builtin(TypeKind::Int),
                {types.builtin(TypeKind::Long), types.builtin(TypeKind::Byte)}),
        "TS-SEMA-075 function interning uses return type, parameter order, and arity");
}

void c076(TestContext& c)
{
    TypeContext types;
    Type* firstStruct = types.createStruct("N", nullptr);
    Type* secondStruct = types.createStruct("N", nullptr);
    Type* firstEnum = types.createEnum("E", nullptr);
    Type* secondEnum = types.createEnum("E", nullptr);
    c.expect(firstStruct != secondStruct && !firstStruct->isComplete() && !secondStruct->isComplete() &&
            firstEnum != secondEnum && firstEnum->isComplete() && secondEnum->isComplete(),
        "TS-SEMA-076 repeated nominal creation yields fresh struct and enum identities");
}

void c077(TestContext& c)
{
    {
        TypeContext types;
        Type* record = types.createStruct("Node", nullptr);
        Type* pointer = types.pointerTo(record);
        Type* array = types.arrayOf(pointer, 16);
        Type* function = types.functionType(types.builtin(TypeKind::Void), {array, pointer});
        Type* owned = types.qualified(record, OwnershipKind::Owned);
        c.expect(function != nullptr && owned != nullptr,
            "TS-SEMA-077 the interned type graph must be fully constructed before destruction");
    }
    c.expect(false,
        "TS-SEMA-077 TEST-INFRASTRUCTURE: leak and unique-delete certification requires the sanitizer profile reserved for the later coverage phase");
}
}

void registerSemaTypeSystemModule(TestRegistry& registry)
{
    const std::array<TestFunction, 24> tests = {c054, c055, c056, c057, c058, c059, c060, c061,
        c062, c063, c064, c065, c066, c067, c068, c069, c070, c071, c072, c073, c074, c075,
        c076, c077};
    for (std::size_t index = 0; index < tests.size(); ++index)
    {
        const std::size_t ordinal = index + 54;
        const std::string id = "TS-SEMA-0" + std::to_string(ordinal);
        const TestDisposition disposition = ordinal == 77
            ? TestDisposition::InfrastructurePending : TestDisposition::Required;
        std::vector<std::string> tags;
        if (ordinal == 77) tags = {"infrastructure-special"};
        addSemaCase(registry, id, ordinal <= 65 ? "CS-SEMA-008" : "CS-SEMA-009", "unit",
            tests[index], ordinal == 77 ? "NIGHTLY" : "PR", disposition, std::move(tags));
    }
}
}
