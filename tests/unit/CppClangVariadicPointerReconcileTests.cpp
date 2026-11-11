#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppVariadicAbiReconciliation.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

int main(int argc, char** argv)
{
    using namespace nativesharp::interop;
    if (argc != 3) return 1;
    CppClangSessionFactory factory;
    const auto open = [&](const char* source) -> std::unique_ptr<CppInteropSession> {
        CppSessionOptions options;
        options.language = CppLanguageMode::C;
        options.languageStandard = "c11";
        options.targetTriple = "x86_64-pc-windows-msvc";
        options.driverReferencePath = "clang.exe";
        options.resourceDirectory = argv[1];
        options.workingDirectory = argv[2];
        options.input = CppBufferInput{"variadic-pointer-reconcile.c", source};
        auto opened = factory.open(std::move(options), {});
        auto* holder = std::get_if<std::unique_ptr<CppInteropSession>>(&opened);
        return holder ? std::move(*holder) : nullptr;
    };
    auto target = open("typedef struct Mixed { double x; int y; } Mixed; "
                       "typedef int (__attribute__((sysv_abi)) *Callback)(int, ...); void observe(Mixed);");
    auto equivalent = open("struct Mixed { double x; int y; };");
    auto incompatible = open("struct Mixed { long long x; int y; };");
    if (!target || !equivalent || !incompatible) return 2;
    const auto aliases = target->typeAliasesSnapshot();
    const auto functions = target->freeFunctionsSnapshot();
    if (!aliases || !functions) return 3;
    const CppType* pointer = nullptr;
    const CppType* record = nullptr;
    for (const auto& alias : *aliases) if (alias.name == "Callback") pointer = &alias.underlyingType;
    for (const auto& function : *functions)
        if (function.name == "observe" && function.parameters.size() == 1) record = &function.parameters[0].type;
    if (!pointer || !record || !pointer->element || !pointer->element->function) return 4;
    std::vector<CppVariadicArgumentOrigin> arguments{{*record, target.get()}};
    if (!reconcileCppVariadicFunctionPointerCallAbiPlan(*target, "Callback", *pointer, arguments)) return 5;
    arguments[0].origin = equivalent.get();
    if (!reconcileCppVariadicFunctionPointerCallAbiPlan(*target, "Callback", *pointer, arguments)) return 6;
    arguments[0].origin = incompatible.get();
    if (reconcileCppVariadicFunctionPointerCallAbiPlan(*target, "Callback", *pointer, arguments)) return 7;
    arguments[0].origin = nullptr;
    if (reconcileCppVariadicFunctionPointerCallAbiPlan(*target, "Callback", *pointer, arguments)) return 8;
    arguments[0].origin = target.get();
    arguments[0].type.sizeBits = 64;
    if (reconcileCppVariadicFunctionPointerCallAbiPlan(*target, "Callback", *pointer, arguments)) return 9;
    arguments[0].type = *record;
    auto forged = *pointer;
    auto element = std::make_shared<CppType>(*pointer->element);
    auto signature = std::make_shared<CppFunctionType>(*element->function);
    signature->parameterTypes[0].sizeBits = 64;
    element->function = signature;
    forged.element = element;
    if (reconcileCppVariadicFunctionPointerCallAbiPlan(*target, "Callback", forged, arguments) ||
        reconcileCppVariadicFunctionPointerCallAbiPlan(*target, "Missing", *pointer, arguments)) return 10;
    if (!target->close().succeeded ||
        reconcileCppVariadicFunctionPointerCallAbiPlan(*target, "Callback", *pointer, arguments)) return 11;
    return 0;
}
