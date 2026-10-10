#include "ParserFoundationTestSupport.h"

#include "nativesharp/Language/GenericSemantics.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CP-PAR-006", "Host.Parser", "component", "PR",
        {"parser", "type", "function"}, std::move(function)});
}

void testVariadicFunctionType(TestContext& context)
{
    ParserFixture fixture("fn*C(int,...)->void");
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->isFunction() && type->functionIsVariadic() &&
            type->typeArguments().size() == 1 && type->typeArguments()[0]->baseName() == "int" &&
            type->functionCallingConvention() == FunctionCallingConventionSyntaxKind::C &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-051 a C variadic function type must retain one fixed parameter and final ellipsis");
}

void testIndependentFunctionEffects(TestContext& context)
{
    ParserFixture unsafeOnly("fn*()->void unsafe");
    ParserFixture noHeapOnly("fn*()->void noheap");
    ParserFixture asyncOnly("fn*()->void async");
    ParserFixture all("fn*()->void unsafe noheap async");
    TypeSyntax* const unsafeType = unsafeOnly.parser().parseType();
    TypeSyntax* const noHeapType = noHeapOnly.parser().parseType();
    TypeSyntax* const asyncType = asyncOnly.parser().parseType();
    TypeSyntax* const allType = all.parser().parseType();
    context.expect(unsafeType != nullptr && unsafeType->functionIsUnsafe() &&
            !unsafeType->functionIsNoHeap() && !unsafeType->functionIsAsync() &&
            noHeapType != nullptr && !noHeapType->functionIsUnsafe() &&
            noHeapType->functionIsNoHeap() && !noHeapType->functionIsAsync() &&
            asyncType != nullptr && !asyncType->functionIsUnsafe() &&
            !asyncType->functionIsNoHeap() && asyncType->functionIsAsync() &&
            allType != nullptr && allType->functionIsUnsafe() &&
            allType->functionIsNoHeap() && allType->functionIsAsync(),
        "TP-PAR-052 unsafe, noheap, and async must remain three independent function effects");
}

void testNullableLifetimeFunctionReference(TestContext& context)
{
    ParserFixture fixture("fn&?[scope](int)->bool");
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->isFunction() &&
            type->functionCarrier() == FunctionTypeCarrierSyntaxKind::Reference &&
            type->functionIsNullable() && type->functionLifetimeName() == "scope" &&
            type->typeArguments().size() == 1 &&
            type->functionReturnType() != nullptr && type->functionReturnType()->baseName() == "bool" &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-053 nullable function reference and named lifetime must be retained exactly");
}

void testFunctionErrorType(TestContext& context)
{
    ParserFixture fixture("fn*()->int throws Error", {"Error"});
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->isFunction() &&
            type->functionErrorType() != nullptr &&
            type->functionErrorType()->baseName() == "Error" &&
            type->functionErrorType()->range().begin().offset() == 18 &&
            type->functionErrorType()->range().end().offset() == 23 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-054 typed error must be represented independently from the return type");
}

void testDynAndSomeRequireNamedTraits(TestContext& context)
{
    ParserFixture dynamicTuple("dyn ()");
    ParserFixture opaqueTuple("some ()");
    TypeSyntax* const dynamicType = dynamicTuple.parser().parseType();
    TypeSyntax* const opaqueType = opaqueTuple.parser().parseType();
    context.expect(dynamicType != nullptr && dynamicType->isTuple() &&
            dynamicTuple.parser().current().kind() == TokenKind::EndOfFile &&
            diagnosticCount(dynamicTuple.diagnostics(), DiagnosticId::InvalidType) == 1,
        "TP-PAR-055 dyn applied to a tuple must diagnose and still make finite progress");
    context.expect(opaqueType != nullptr && opaqueType->isTuple() &&
            opaqueTuple.parser().current().kind() == TokenKind::EndOfFile &&
            diagnosticCount(opaqueTuple.diagnostics(), DiagnosticId::InvalidType) == 1,
        "TP-PAR-055 some applied to a tuple must diagnose and still make finite progress");
}

void testPortableSimdShape(TestContext& context)
{
    ParserFixture fixture("simd<float,8>");
    TypeSyntax* const type = fixture.parser().parseType();
    std::string countType;
    std::string countValue;
    const bool countDecoded = type != nullptr && type->typeArguments().size() == 2 &&
        nativesharp::language::parseConstGenericArgumentMarker(
            type->typeArguments()[1]->baseName(), countType, countValue);
    context.expect(type != nullptr && type->baseName() == "simd" &&
            type->typeArguments().size() == 2 && type->typeArguments()[0]->baseName() == "float" &&
            countDecoded && countType == "builtin:int" && countValue == "8" &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-056 portable SIMD must retain lane type and const lane count exactly");
}

void testPortableMaskShape(TestContext& context)
{
    ParserFixture fixture("mask<int,4>");
    TypeSyntax* const type = fixture.parser().parseType();
    std::string countType;
    std::string countValue;
    const bool countDecoded = type != nullptr && type->typeArguments().size() == 2 &&
        nativesharp::language::parseConstGenericArgumentMarker(
            type->typeArguments()[1]->baseName(), countType, countValue);
    context.expect(type != nullptr && type->baseName() == "mask" &&
            type->typeArguments().size() == 2 && type->typeArguments()[0]->baseName() == "int" &&
            countDecoded && countValue == "4" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-057 portable mask must retain its lane type and exact lane count syntax");
}

void testInvalidPortableLaneRemainsSyntactic(TestContext& context)
{
    // Lane admissibility and count bounds require semantic type information. Parser's stable
    // contract is to preserve the two arguments without manufacturing a parser-only policy.
    ParserFixture fixture("simd<string,3>");
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->baseName() == "simd" &&
            type->typeArguments().size() == 2 && type->typeArguments()[0]->baseName() == "string" &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-058 parser must preserve invalid-looking SIMD arguments for semantic validation");
}

void testNestedTupleAndFunctionDelimiters(TestContext& context)
{
    ParserFixture fixture("(fn*C(int,...)->void,(int,fn&[a]()->bool))");
    TypeSyntax* const outer = fixture.parser().parseType();
    const TypeSyntax* const left = outer != nullptr && outer->typeArguments().size() == 2
        ? outer->typeArguments()[0] : nullptr;
    const TypeSyntax* const right = outer != nullptr && outer->typeArguments().size() == 2
        ? outer->typeArguments()[1] : nullptr;
    context.expect(outer != nullptr && outer->isTuple() && left != nullptr && left->isFunction() &&
            left->functionIsVariadic() && right != nullptr && right->isTuple() &&
            right->typeArguments().size() == 2 && right->typeArguments()[1]->isFunction() &&
            right->typeArguments()[1]->functionLifetimeName() == "a" &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-059 nested tuple/function parsing must retain every delimiter boundary");
}
}

void registerParserFunctionTypeContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-051", testVariadicFunctionType);
    addCase(registry, "TP-PAR-052", testIndependentFunctionEffects);
    addCase(registry, "TP-PAR-053", testNullableLifetimeFunctionReference);
    addCase(registry, "TP-PAR-054", testFunctionErrorType);
    addCase(registry, "TP-PAR-055", testDynAndSomeRequireNamedTraits);
    addCase(registry, "TP-PAR-056", testPortableSimdShape);
    addCase(registry, "TP-PAR-057", testPortableMaskShape);
    addCase(registry, "TP-PAR-058", testInvalidPortableLaneRemainsSyntactic);
    addCase(registry, "TP-PAR-059", testNestedTupleAndFunctionDelimiters);
}
}
