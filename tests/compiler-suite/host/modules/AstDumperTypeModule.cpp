#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstDumper.h"
#include "nativesharp/AST/TypeSyntax.h"

#include <array>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::ast;

// Builds a nominal type fixture with no source-location dependency.
TypeSyntax makeType(std::string name)
{
    return TypeSyntax({}, nativesharp::lexer::TokenKind::Invalid, std::move(name));
}

// Observes private AstDumper::dumpType through the public node type channel.
std::string dumpType(TypeSyntax& type)
{
    AstNode node(AstNodeKind::IdentifierExpression, {});
    node.setTypeSyntax(&type);
    std::ostringstream output;
    AstDumper{}.dump(&node, output);
    constexpr std::string_view Prefix = "IdentifierExpression type=";
    const std::string fullDump = output.str();
    return fullDump.substr(Prefix.size(), fullDump.size() - Prefix.size() - 1);
}

// Registers one CA-AST-019 case with common component metadata.
void addTypeDumpCase(TestRegistry& registry, std::string id, std::string profile,
    TestFunction function)
{
    registry.add(TestCase{std::move(id), "CA-AST-019", "Host.AST", "component",
        std::move(profile), {"ast", "dumper", "type"}, std::move(function)});
}

// Freezes the base nominal spelling with no decorations.
void testSimpleNominalType(TestContext& context)
{
    TypeSyntax type = makeType("Widget");
    context.expect(dumpType(type) == "Widget",
        "TA-AST-194 simple nominal dump must equal baseName exactly");
}

// Covers omission of none and exact owned/borrowed prefixes.
void testOwnershipPrefixes(TestContext& context)
{
    TypeSyntax none = makeType("Widget");
    TypeSyntax owned = makeType("Widget");
    TypeSyntax borrowed = makeType("Widget");
    owned.setOwnershipQualifier(OwnershipQualifierSyntaxKind::Owned);
    borrowed.setOwnershipQualifier(OwnershipQualifierSyntaxKind::Borrowed);
    context.expect(dumpType(none) == "Widget" && dumpType(owned) == "owned Widget" &&
            dumpType(borrowed) == "borrowed Widget",
        "TA-AST-195 ownership prefix must be exact and none must be omitted");
}

// Freezes the borrowed lifetime annotation between qualifier and nominal name.
void testBorrowedLifetime(TestContext& context)
{
    TypeSyntax type = makeType("Widget");
    type.setOwnershipQualifier(OwnershipQualifierSyntaxKind::Borrowed);
    type.setLifetimeName("a");
    context.expect(dumpType(type) == "borrowed[a] Widget",
        "TA-AST-196 borrowed lifetime must use borrowed[a] prefix");
}

// Freezes dynamic-trait marker position before the nominal base.
void testDynamicTrait(TestContext& context)
{
    TypeSyntax type = makeType("Drawable");
    type.setDynamicTrait();
    context.expect(dumpType(type) == "dyn Drawable",
        "TA-AST-197 dyn marker must precede the nominal base");
}

// Covers absent, single, and multiple generic nominal arguments.
void testGenericNominalTypes(TestContext& context)
{
    TypeSyntax intType = makeType("int");
    TypeSyntax stringType = makeType("string");
    TypeSyntax zero = makeType("Box");
    TypeSyntax one = makeType("Box");
    TypeSyntax many = makeType("Map");
    one.addTypeArgument(&intType);
    many.addTypeArgument(&stringType);
    many.addTypeArgument(&intType);
    context.expect(dumpType(zero) == "Box" && dumpType(one) == "Box<int>" &&
            dumpType(many) == "Map<string,int>",
        "TA-AST-198 generic nominal angle list and commas must be exact");
}

// Covers empty, unnamed, and partially named structural tuples.
void testTupleTypes(TestContext& context)
{
    TypeSyntax intType = makeType("int");
    TypeSyntax stringType = makeType("string");
    TypeSyntax empty = makeType("");
    TypeSyntax unnamed = makeType("");
    TypeSyntax partial = makeType("");
    empty.setTuple();
    unnamed.addTupleElement(&intType);
    unnamed.addTupleElement(&stringType);
    partial.addTupleElement(&intType, "x");
    partial.addTupleElement(&stringType);
    context.expect(dumpType(empty) == "()" && dumpType(unnamed) == "(int,string)" &&
            dumpType(partial) == "(int x,string)",
        "TA-AST-199 tuple parentheses, commas, and partial names must be exact");
}

// Distinguishes pointer and reference function carriers.
void testFunctionCarriers(TestContext& context)
{
    TypeSyntax voidType = makeType("void");
    TypeSyntax pointer = makeType("");
    TypeSyntax reference = makeType("");
    pointer.setFunction();
    pointer.setFunctionReturnType(&voidType);
    reference.setFunction();
    reference.setFunctionCarrier(FunctionTypeCarrierSyntaxKind::Reference);
    reference.setFunctionReturnType(&voidType);
    context.expect(dumpType(pointer) == "fn* native()->void" &&
            dumpType(reference) == "fn& native()->void",
        "TA-AST-200 function carriers must use fn* and fn& spellings");
}

// Freezes nullable marker before the function lifetime annotation.
void testFunctionNullableLifetime(TestContext& context)
{
    TypeSyntax voidType = makeType("void");
    TypeSyntax function = makeType("");
    function.setFunction();
    function.setFunctionNullable();
    function.setFunctionLifetimeName("a");
    function.setFunctionReturnType(&voidType);
    context.expect(dumpType(function) == "fn*?[a] native()->void",
        "TA-AST-201 function nullable and lifetime markers must be exact");
}

// Covers every calling-convention spelling, including the explicit native default.
void testCallingConventions(TestContext& context)
{
    using Convention = FunctionCallingConventionSyntaxKind;
    constexpr std::array conventions = {
        Convention::NativeSharp, Convention::C, Convention::StdCall,
        Convention::FastCall, Convention::VectorCall, Convention::Win64,
    };
    constexpr std::array<std::string_view, 6> spellings = {
        " native", " C", " stdcall", " fastcall", " vectorcall", " win64",
    };
    TypeSyntax voidType = makeType("void");
    for (std::size_t index = 0; index < conventions.size(); ++index)
    {
        TypeSyntax function = makeType("");
        function.setFunction();
        function.setFunctionCallingConvention(conventions[index]);
        function.setFunctionReturnType(&voidType);
        context.expect(dumpType(function) == "fn*" + std::string(spellings[index]) + "()->void",
            "TA-AST-202 calling convention spelling differs at index " + std::to_string(index));
    }
}

// Freezes parameter commas and variadic ellipsis placement.
void testFunctionParametersAndVariadic(TestContext& context)
{
    TypeSyntax intType = makeType("int");
    TypeSyntax stringType = makeType("string");
    TypeSyntax voidType = makeType("void");
    TypeSyntax emptyVariadic = makeType("");
    TypeSyntax populatedVariadic = makeType("");
    emptyVariadic.setFunction();
    emptyVariadic.setFunctionVariadic();
    emptyVariadic.setFunctionReturnType(&voidType);
    populatedVariadic.setFunction();
    populatedVariadic.addTypeArgument(&intType);
    populatedVariadic.addTypeArgument(&stringType);
    populatedVariadic.setFunctionVariadic();
    populatedVariadic.setFunctionReturnType(&voidType);
    context.expect(dumpType(emptyVariadic) == "fn* native(...)->void" &&
            dumpType(populatedVariadic) == "fn* native(int,string,...)->void",
        "TA-AST-203 variadic ellipsis and parameter commas must be exact");
}

// Distinguishes an explicit return type from the missing-return marker.
void testFunctionReturnPresence(TestContext& context)
{
    TypeSyntax intType = makeType("int");
    TypeSyntax present = makeType("");
    TypeSyntax absent = makeType("");
    present.setFunction();
    present.setFunctionReturnType(&intType);
    absent.setFunction();
    context.expect(dumpType(present) == "fn* native()->int" &&
            dumpType(absent) == "fn* native()-><missing>",
        "TA-AST-204 function return must be type or <missing>");
}

// Freezes unsafe, noheap, async, and typed-error suffix order.
void testFunctionSuffixes(TestContext& context)
{
    TypeSyntax voidType = makeType("void");
    TypeSyntax errorType = makeType("Error");
    TypeSyntax function = makeType("");
    function.setFunction();
    function.setFunctionReturnType(&voidType);
    function.setFunctionUnsafe();
    function.setFunctionNoHeap();
    function.setFunctionAsync();
    function.setFunctionErrorType(&errorType);
    context.expect(dumpType(function) ==
            "fn* native()->void unsafe noheap async throws Error",
        "TA-AST-205 function suffixes must retain their fixed order");
}

// Freezes pointee const placement before every physical modifier.
void testPointeeConst(TestContext& context)
{
    TypeSyntax type = makeType("Widget");
    type.setPointeeConst();
    TypeModifierSyntax pointer;
    pointer.kind = TypeModifierSyntaxKind::Pointer;
    type.addModifier(pointer);
    context.expect(dumpType(type) == "Widget const*",
        "TA-AST-206 pointee const must precede pointer modifiers");
}

// Covers legacy, nullable, and nonnull pointer marker spellings.
void testPointerNullability(TestContext& context)
{
    constexpr std::array nullabilities = {
        PointerNullabilitySyntaxKind::LegacyNullable,
        PointerNullabilitySyntaxKind::Nullable,
        PointerNullabilitySyntaxKind::NonNull,
    };
    constexpr std::array<std::string_view, 3> expected = {"Widget*", "Widget*?", "Widget*!"};
    for (std::size_t index = 0; index < nullabilities.size(); ++index)
    {
        TypeSyntax type = makeType("Widget");
        TypeModifierSyntax pointer;
        pointer.kind = TypeModifierSyntaxKind::Pointer;
        pointer.pointerNullability = nullabilities[index];
        type.addModifier(pointer);
        context.expect(dumpType(type) == expected[index],
            "TA-AST-207 pointer nullability spelling differs at index " + std::to_string(index));
    }
}

// Freezes nondefault pointer suffixes and omission of default pointer metadata.
void testPointerMetadata(TestContext& context)
{
    TypeSyntax defaults = makeType("Widget");
    TypeSyntax explicitMetadata = makeType("Widget");
    TypeModifierSyntax defaultPointer;
    defaultPointer.kind = TypeModifierSyntaxKind::Pointer;
    TypeModifierSyntax detailedPointer = defaultPointer;
    detailedPointer.pointerKnownAlignment = 16;
    detailedPointer.pointerAddressSpace = 3;
    detailedPointer.pointerHasProvenance = false;
    defaults.addModifier(defaultPointer);
    explicitMetadata.addModifier(detailedPointer);
    context.expect(dumpType(defaults) == "Widget*" &&
            dumpType(explicitMetadata) ==
                "Widget* align(16) addrspace(3) provenance(none)",
        "TA-AST-208 pointer metadata suffixes and default omission must be exact");
}

// Freezes dynamic-array suffix spelling.
void testDynamicArray(TestContext& context)
{
    TypeSyntax type = makeType("Widget");
    TypeModifierSyntax modifier;
    modifier.kind = TypeModifierSyntaxKind::DynamicArray;
    type.addModifier(modifier);
    context.expect(dumpType(type) == "Widget[]",
        "TA-AST-209 dynamic array must use [] suffix");
}

// Covers rank zero, one, and many using max(rank - 1, 0) commas.
void testRangeArrayRanks(TestContext& context)
{
    constexpr std::array<std::size_t, 3> ranks = {0, 1, 4};
    constexpr std::array<std::string_view, 3> expected = {"Widget[]", "Widget[]", "Widget[,,,]"};
    for (std::size_t index = 0; index < ranks.size(); ++index)
    {
        TypeSyntax type = makeType("Widget");
        TypeModifierSyntax modifier;
        modifier.kind = TypeModifierSyntaxKind::RangeArray;
        modifier.arrayRank = ranks[index];
        type.addModifier(modifier);
        context.expect(dumpType(type) == expected[index],
            "TA-AST-210 range-array rank spelling differs at index " + std::to_string(index));
    }
}

// Freezes the flexible-array suffix.
void testFlexibleArray(TestContext& context)
{
    TypeSyntax type = makeType("Widget");
    TypeModifierSyntax modifier;
    modifier.kind = TypeModifierSyntaxKind::FlexibleArray;
    type.addModifier(modifier);
    context.expect(dumpType(type) == "Widget[*]",
        "TA-AST-211 flexible array must use [*] suffix");
}

// Proves fixed-array count payload is deliberately omitted from the dump.
void testFixedArrayCountOmission(TestContext& context)
{
    AstNode count(AstNodeKind::IntegerLiteralExpression, {});
    count.setIntegerValue(64);
    TypeSyntax withoutCount = makeType("Widget");
    TypeSyntax withCount = makeType("Widget");
    TypeModifierSyntax absent;
    absent.kind = TypeModifierSyntaxKind::Array;
    TypeModifierSyntax present = absent;
    present.arrayCount = &count;
    withoutCount.addModifier(absent);
    withCount.addModifier(present);
    context.expect(dumpType(withoutCount) == "Widget[expr]" &&
            dumpType(withCount) == dumpType(withoutCount),
        "TA-AST-212 fixed-array count expression must not alter [expr] dump");
}

// Combines recursive generic arguments and modifiers into one exact nested spelling.
void testNestedTypes(TestContext& context)
{
    TypeSyntax intType = makeType("int");
    TypeModifierSyntax pointer;
    pointer.kind = TypeModifierSyntaxKind::Pointer;
    intType.addModifier(pointer);
    TypeSyntax list = makeType("List");
    list.addTypeArgument(&intType);
    TypeModifierSyntax dynamicArray;
    dynamicArray.kind = TypeModifierSyntaxKind::DynamicArray;
    list.addModifier(dynamicArray);
    TypeSyntax stringType = makeType("string");
    TypeSyntax map = makeType("Map");
    map.addTypeArgument(&stringType);
    map.addTypeArgument(&list);
    context.expect(dumpType(map) == "Map<string,List<int*>[]>",
        "TA-AST-213 nested type recursion must concatenate exact spellings");
}

// Characterizes semantic/provenance channels intentionally absent from the diagnostic dump.
void testOmittedTypeChannels(TestContext& context)
{
    TypeSyntax baseline = makeType("Widget");
    TypeSyntax variant = makeType("Widget");
    AstNode constExpression(AstNodeKind::IntegerLiteralExpression, {});
    variant.setOpaqueResult();
    variant.setOpaqueResultTraitName("Trait");
    variant.setExplicitTraitImplementation();
    variant.setConstExpression(&constExpression);
    variant.setPackExpansion();
    variant.setOriginProjectOrdinal(17);
    variant.setOriginNamespace("Other.Namespace");
    variant.setRange({{nativesharp::FileId(1), 2}, {nativesharp::FileId(1), 9}});
    context.expect(dumpType(variant) == dumpType(baseline),
        "TA-AST-214 omitted opaque, trait, const, pack, origin, and range channels must match");
}

// Exercises a deep but finite nested type and verifies the complete recursive output.
void testDeeplyNestedType(TestContext& context)
{
    constexpr std::size_t Depth = 512;
    std::vector<std::unique_ptr<TypeSyntax>> types;
    types.reserve(Depth + 1);
    types.push_back(std::make_unique<TypeSyntax>(makeType("int")));
    for (std::size_t index = 0; index < Depth; ++index)
    {
        auto outer = std::make_unique<TypeSyntax>(makeType("Box"));
        outer->addTypeArgument(types.back().get());
        types.push_back(std::move(outer));
    }
    std::string expected;
    expected.reserve(Depth * 5 + 3);
    for (std::size_t index = 0; index < Depth; ++index) expected += "Box<";
    expected += "int";
    for (std::size_t index = 0; index < Depth; ++index) expected += '>';
    context.expect(dumpType(*types.back()) == expected,
        "TA-AST-216 deeply nested type must produce complete recursive output");
}
}

// Adds all CA-AST-019 cases executable without an isolated null-dereference death test.
void registerAstDumperTypeModule(TestRegistry& registry)
{
    addTypeDumpCase(registry, "TA-AST-194", "PR", testSimpleNominalType);
    addTypeDumpCase(registry, "TA-AST-195", "PR", testOwnershipPrefixes);
    addTypeDumpCase(registry, "TA-AST-196", "PR", testBorrowedLifetime);
    addTypeDumpCase(registry, "TA-AST-197", "PR", testDynamicTrait);
    addTypeDumpCase(registry, "TA-AST-198", "PR", testGenericNominalTypes);
    addTypeDumpCase(registry, "TA-AST-199", "PR", testTupleTypes);
    addTypeDumpCase(registry, "TA-AST-200", "PR", testFunctionCarriers);
    addTypeDumpCase(registry, "TA-AST-201", "PR", testFunctionNullableLifetime);
    addTypeDumpCase(registry, "TA-AST-202", "PR", testCallingConventions);
    addTypeDumpCase(registry, "TA-AST-203", "PR", testFunctionParametersAndVariadic);
    addTypeDumpCase(registry, "TA-AST-204", "PR", testFunctionReturnPresence);
    addTypeDumpCase(registry, "TA-AST-205", "PR", testFunctionSuffixes);
    addTypeDumpCase(registry, "TA-AST-206", "PR", testPointeeConst);
    addTypeDumpCase(registry, "TA-AST-207", "PR", testPointerNullability);
    addTypeDumpCase(registry, "TA-AST-208", "PR", testPointerMetadata);
    addTypeDumpCase(registry, "TA-AST-209", "PR", testDynamicArray);
    addTypeDumpCase(registry, "TA-AST-210", "PR", testRangeArrayRanks);
    addTypeDumpCase(registry, "TA-AST-211", "PR", testFlexibleArray);
    addTypeDumpCase(registry, "TA-AST-212", "PR", testFixedArrayCountOmission);
    addTypeDumpCase(registry, "TA-AST-213", "PR", testNestedTypes);
    addTypeDumpCase(registry, "TA-AST-214", "PR", testOmittedTypeChannels);
    addTypeDumpCase(registry, "TA-AST-216", "NIGHTLY", testDeeplyNestedType);
}
}
