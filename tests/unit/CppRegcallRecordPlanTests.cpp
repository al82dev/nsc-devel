#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppInteropSession.h"
#include "nativesharp/Interop/CppVariadicAbiReconciliation.h"
#include <llvm/IR/CallingConv.h>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using namespace nativesharp::interop;

int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    CppSessionOptions options;
    options.language = CppLanguageMode::C;
    options.languageStandard = "c11";
    options.targetTriple = "x86_64-pc-windows-msvc";
    options.driverReferencePath = "clang.exe";
    options.resourceDirectory = argv[1];
    options.workingDirectory = argv[2];
    options.input = CppBufferInput{"regcall-record-plan.c", R"c(
        typedef struct Small { int x, y; } Small;
        typedef struct Large { int a, b, c, d, e, f; } Large;
        typedef struct Floats { float x, y; } Floats;
        typedef Small (__attribute__((regcall)) *SmallFn)(Small);
        typedef Large (__attribute__((regcall)) *LargeFn)(Large);
        typedef Floats (__attribute__((regcall)) *FloatsFn)(Floats);
        Small __attribute__((regcall)) small(Small);
        Large __attribute__((regcall)) large(Large);
        Floats __attribute__((regcall)) floats(Floats);
    )c"};
    CppClangSessionFactory factory;
    auto opened = factory.open(std::move(options), {});
    if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(opened)) return 2;
    auto session = std::move(std::get<std::unique_ptr<CppInteropSession>>(opened));
    const auto aliases = session->typeAliasesSnapshot();
    const auto functions = session->freeFunctionsSnapshot();
    if (!aliases || !functions) return 3;
    for (const auto& [aliasName, functionName] : {std::pair{"SmallFn", "small"},
            std::pair{"LargeFn", "large"}, std::pair{"FloatsFn", "floats"}})
    {
        std::string aliasType, functionType;
        const CppType* aliasPointer = nullptr;
        for (const auto& alias : *aliases)
            if (alias.name == aliasName) { aliasType = alias.underlyingType.canonicalSpelling; aliasPointer = &alias.underlyingType; }
        for (const auto& function : *functions)
            if (function.name == functionName) functionType = function.canonicalFunctionType;
        if (aliasType.empty() || functionType.empty()) return 4;
        const auto direct = session->cFunctionAbiPlan(functionName, functionType);
        const auto indirect = session->cFunctionPointerAbiPlan(aliasName, aliasType);
        // The current plan may not express the expanded HFA. If it becomes
        // available, both query paths must agree on Clang's physical shape.
        if (std::string_view(aliasName) == "FloatsFn")
        {
            if (!direct && !indirect) continue;
            if (!direct || !indirect || direct->physicalReturn.kind != abi_plan::TypeKind::Structure ||
                direct->physicalParameters.size() != 2 ||
                direct->physicalParameters[0].type.kind != abi_plan::TypeKind::Float ||
                direct->physicalParameters[1].type.kind != abi_plan::TypeKind::Float) return 9;
        }
        if (!direct || !indirect)
        { std::cerr << aliasName << ": directo=" << bool(direct) << " puntero=" << bool(indirect) << '\n'; return 5; }
        if (!aliasPointer || !aliasPointer->element || !aliasPointer->element->function ||
            aliasPointer->element->function->parameterTypes.size() != 1) return 10;
        const auto& signature = *aliasPointer->element->function;
        CppVariadicArgumentOrigin result{signature.returnType, session.get()};
        const std::vector<CppVariadicArgumentOrigin> arguments{{signature.parameterTypes[0], session.get()}};
        if (!reconcileCppFunctionPointerAbiPlan(*session, aliasName, *aliasPointer, result, arguments) ||
            reconcileCppFunctionPointerAbiPlan(*session, "missing", *aliasPointer, result, arguments)) return 11;
        auto wrongResult = result;
        wrongResult.type.sizeBits = 1;
        if (reconcileCppFunctionPointerAbiPlan(*session, aliasName, *aliasPointer, wrongResult, arguments)) return 12;
        if (std::string_view(aliasName) == "SmallFn")
        {
            CppSessionOptions otherOptions;
            otherOptions.language = CppLanguageMode::C;
            otherOptions.languageStandard = "c11";
            otherOptions.targetTriple = "x86_64-pc-windows-msvc";
            otherOptions.driverReferencePath = "clang.exe";
            otherOptions.resourceDirectory = argv[1];
            otherOptions.workingDirectory = argv[2];
            otherOptions.input = CppBufferInput{"regcall-other.c", "struct Small { unsigned x, y; };"};
            auto otherOpened = factory.open(std::move(otherOptions), {});
            if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(otherOpened)) return 13;
            auto other = std::move(std::get<std::unique_ptr<CppInteropSession>>(otherOpened));
            wrongResult = result; wrongResult.origin = other.get();
            if (reconcileCppFunctionPointerAbiPlan(*session, aliasName, *aliasPointer, wrongResult, arguments)) return 14;
        }
        if (direct->callingConvention != llvm::CallingConv::X86_RegCall ||
            indirect->callingConvention != direct->callingConvention ||
            indirect->physicalReturn.kind != direct->physicalReturn.kind ||
            indirect->physicalReturn.widthOrCount != direct->physicalReturn.widthOrCount ||
            indirect->physicalParameters.size() != direct->physicalParameters.size() ||
            indirect->logicalParameters.size() != 1 || direct->logicalParameters.size() != 1 ||
            indirect->logicalParameters[0].physicalCount != direct->logicalParameters[0].physicalCount)
        { std::cerr << "plan no disponible o incompatible para " << aliasName << '\n'; return 5; }
        std::cout << aliasName << ": cc=" << direct->callingConvention
                  << " simbolo-puntero=" << indirect->symbol
                  << " retorno=" << static_cast<int>(direct->physicalReturn.kind)
                  << " bits=" << direct->physicalReturn.widthOrCount
                  << " paso=" << static_cast<int>(direct->logicalParameters[0].passing)
                  << " fisicos=" << direct->physicalParameters.size() << '\n';
        for (std::size_t i = 0; i < direct->physicalParameters.size(); ++i)
            if (indirect->physicalParameters[i].type.kind != direct->physicalParameters[i].type.kind ||
                indirect->physicalParameters[i].type.widthOrCount != direct->physicalParameters[i].type.widthOrCount ||
                indirect->physicalParameters[i].structureReturn != direct->physicalParameters[i].structureReturn)
                return 6;
        if (std::string_view(aliasName) == "SmallFn" &&
            (direct->physicalReturn.kind != abi_plan::TypeKind::Integer || direct->physicalReturn.widthOrCount != 64)) return 7;
        if (std::string_view(aliasName) == "LargeFn" &&
            (direct->physicalReturn.kind != abi_plan::TypeKind::Void || direct->physicalParameters.size() != 2 ||
             !direct->physicalParameters[0].structureReturn)) return 8;
    }
    return 0;
}
