#include "TypeSyntaxTestSupport.h"

#include <array>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::ast;
using type_syntax_test::addCase;
using type_syntax_test::type;
}

void registerTypeSyntaxFunctionModule(TestRegistry& registry)
{
    addCase(registry, "TA-AST-134", "CA-AST-014", {"ast", "type-syntax", "function-default"},
        [](TestContext& context) {
            const auto value = type();
            context.expect(!value.isFunction() && value.functionCarrier() == FunctionTypeCarrierSyntaxKind::Pointer &&
                    value.functionCallingConvention() == FunctionCallingConventionSyntaxKind::NativeSharp,
                "TA-AST-134 function identity defaults must match");
            context.expect(value.functionReturnType() == nullptr && value.functionErrorType() == nullptr &&
                    !value.functionIsVariadic() && !value.functionIsUnsafe() && !value.functionIsNoHeap() &&
                    !value.functionIsAsync() && !value.functionIsNullable() && value.functionLifetimeName().empty(),
                "TA-AST-134 function metadata must start empty and false");
        });

    addCase(registry, "TA-AST-135", "CA-AST-014", {"ast", "type-syntax", "function"},
        [](TestContext& context) {
            auto value = type(); value.setFunction();
            context.expect(value.isFunction(), "TA-AST-135 default setter must enable function");
            value.setFunction(false);
            context.expect(!value.isFunction(), "TA-AST-135 explicit false must clear only function bit");
        });

    addCase(registry, "TA-AST-136", "CA-AST-014", {"ast", "type-syntax", "carrier"},
        [](TestContext& context) {
            auto value = type();
            value.setFunctionCarrier(FunctionTypeCarrierSyntaxKind::Reference);
            context.expect(value.functionCarrier() == FunctionTypeCarrierSyntaxKind::Reference,
                "TA-AST-136 Reference carrier must round-trip");
            value.setFunctionCarrier(FunctionTypeCarrierSyntaxKind::Pointer);
            context.expect(value.functionCarrier() == FunctionTypeCarrierSyntaxKind::Pointer,
                "TA-AST-136 Pointer carrier must round-trip");
        });

    addCase(registry, "TA-AST-137", "CA-AST-014", {"ast", "type-syntax", "calling-convention"},
        [](TestContext& context) {
            auto value = type();
            constexpr std::array conventions{FunctionCallingConventionSyntaxKind::NativeSharp,
                FunctionCallingConventionSyntaxKind::C, FunctionCallingConventionSyntaxKind::StdCall,
                FunctionCallingConventionSyntaxKind::FastCall, FunctionCallingConventionSyntaxKind::VectorCall,
                FunctionCallingConventionSyntaxKind::Win64};
            for (const auto convention : conventions)
            {
                value.setFunctionCallingConvention(convention);
                context.expect(value.functionCallingConvention() == convention,
                    "TA-AST-137 every calling convention must round-trip");
            }
        });

    addCase(registry, "TA-AST-138", "CA-AST-014", {"ast", "type-syntax", "return-error"},
        [](TestContext& context) {
            auto value = type(); auto returnType = type("Return"); auto errorType = type("Error");
            value.setFunctionReturnType(&returnType); value.setFunctionErrorType(&errorType);
            context.expect(value.functionReturnType() == &returnType && value.functionErrorType() == &errorType,
                "TA-AST-138 return and error pointer identities must be independent");
            value.setFunctionReturnType(nullptr);
            context.expect(value.functionReturnType() == nullptr && value.functionErrorType() == &errorType,
                "TA-AST-138 resetting return must preserve error");
        });

    addCase(registry, "TA-AST-139", "CA-AST-014", {"ast", "type-syntax", "variadic"},
        [](TestContext& context) {
            auto value = type(); value.setFunctionVariadic();
            context.expect(value.functionIsVariadic(), "TA-AST-139 default setter must enable variadic");
            value.setFunctionVariadic(false);
            context.expect(!value.functionIsVariadic(), "TA-AST-139 explicit false must clear variadic");
        });

    addCase(registry, "TA-AST-140", "CA-AST-014", {"ast", "type-syntax", "unsafe"},
        [](TestContext& context) {
            auto value = type(); value.setFunctionUnsafe();
            context.expect(value.functionIsUnsafe(), "TA-AST-140 default setter must enable unsafe");
            value.setFunctionUnsafe(false);
            context.expect(!value.functionIsUnsafe(), "TA-AST-140 explicit false must clear unsafe");
        });

    addCase(registry, "TA-AST-141", "CA-AST-014", {"ast", "type-syntax", "noheap"},
        [](TestContext& context) {
            auto value = type(); value.setFunctionNoHeap();
            context.expect(value.functionIsNoHeap(), "TA-AST-141 default setter must enable noheap");
            value.setFunctionNoHeap(false);
            context.expect(!value.functionIsNoHeap(), "TA-AST-141 explicit false must clear noheap");
        });

    addCase(registry, "TA-AST-142", "CA-AST-014", {"ast", "type-syntax", "async"},
        [](TestContext& context) {
            auto value = type(); value.setFunctionAsync();
            context.expect(value.functionIsAsync(), "TA-AST-142 default setter must enable async");
            value.setFunctionAsync(false);
            context.expect(!value.functionIsAsync(), "TA-AST-142 explicit false must clear async");
        });

    addCase(registry, "TA-AST-143", "CA-AST-014", {"ast", "type-syntax", "nullable"},
        [](TestContext& context) {
            auto value = type(); value.setFunctionNullable();
            context.expect(value.functionIsNullable(), "TA-AST-143 default setter must enable nullable");
            value.setFunctionNullable(false);
            context.expect(!value.functionIsNullable(), "TA-AST-143 explicit false must clear nullable");
        });

    addCase(registry, "TA-AST-144", "CA-AST-014", {"ast", "type-syntax", "function-lifetime"},
        [](TestContext& context) {
            auto value = type();
            { std::string temporary = "scope"; value.setFunctionLifetimeName(std::move(temporary)); }
            context.expect(value.functionLifetimeName() == "scope",
                "TA-AST-144 function lifetime must own temporary input");
        });

    addCase(registry, "TA-AST-145", "CA-AST-014", {"ast", "type-syntax", "parameters"},
        [](TestContext& context) {
            auto value = type(); auto a = type("A"); auto b = type("B");
            value.addTypeArgument(&a); value.addTypeArgument(&b);
            context.expect(value.typeArguments().size() == 2 && value.typeArguments()[0] == &a &&
                    value.typeArguments()[1] == &b,
                "TA-AST-145 function parameters must retain typeArguments order");
        });

    addCase(registry, "TA-AST-146", "CA-AST-014", {"ast", "type-syntax", "orthogonal"},
        [](TestContext& context) {
            auto value = type(); auto parameter = type("P"); auto result = type("R"); auto error = type("E");
            value.setFunction(); value.setFunctionCarrier(FunctionTypeCarrierSyntaxKind::Reference);
            value.setFunctionCallingConvention(FunctionCallingConventionSyntaxKind::Win64);
            value.addTypeArgument(&parameter); value.setFunctionReturnType(&result); value.setFunctionErrorType(&error);
            value.setFunctionVariadic(); value.setFunctionUnsafe(); value.setFunctionNoHeap();
            value.setFunctionAsync(); value.setFunctionNullable(); value.setFunctionLifetimeName("call");
            context.expect(value.isFunction() && value.functionCarrier() == FunctionTypeCarrierSyntaxKind::Reference &&
                    value.functionCallingConvention() == FunctionCallingConventionSyntaxKind::Win64 &&
                    value.typeArguments()[0] == &parameter && value.functionReturnType() == &result &&
                    value.functionErrorType() == &error,
                "TA-AST-146 structural function fields must coexist");
            context.expect(value.functionIsVariadic() && value.functionIsUnsafe() && value.functionIsNoHeap() &&
                    value.functionIsAsync() && value.functionIsNullable() && value.functionLifetimeName() == "call",
                "TA-AST-146 function flags must coexist without cross-field mutation");
        });

    addCase(registry, "TA-AST-147", "CA-AST-014", {"ast", "type-syntax", "retention"},
        [](TestContext& context) {
            auto value = type(); auto result = type("R");
            value.setFunction(); value.setFunctionReturnType(&result); value.setFunctionUnsafe();
            value.setFunction(false);
            context.expect(!value.isFunction() && value.functionReturnType() == &result && value.functionIsUnsafe(),
                "TA-AST-147 clearing function bit must retain populated metadata");
        });

    addCase(registry, "TA-AST-148", "CA-AST-014", {"ast", "type-syntax", "missing-return"},
        [](TestContext& context) {
            auto value = type(); value.setFunction(); value.setFunctionReturnType(nullptr);
            context.expect(value.isFunction() && value.functionReturnType() == nullptr,
                "TA-AST-148 function container must permit missing return type");
        });

    addCase(registry, "TA-AST-149", "CA-AST-014", {"ast", "type-syntax", "no-validation"},
        [](TestContext& context) {
            auto value = type(); value.setFunction(); value.setTuple();
            context.expect(value.isFunction() && value.isTuple(),
                "TA-AST-149 container must preserve an incongruent function-plus-tuple combination");
        });
}
}
