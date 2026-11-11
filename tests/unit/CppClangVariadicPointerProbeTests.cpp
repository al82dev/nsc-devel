#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppVariadicAbiCodec.h"
#include "nativesharp/Interop/CppVariadicAbiValidation.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

int main(int argc, char** argv)
{
    using namespace nativesharp::interop;
    if (argc != 3) return 1;
    CppClangSessionFactory factory;
    CppSessionOptions options;
    options.language = CppLanguageMode::C;
    options.languageStandard = "c11";
    options.targetTriple = "x86_64-pc-windows-msvc";
    options.driverReferencePath = "clang.exe";
    options.resourceDirectory = argv[1];
    options.workingDirectory = argv[2];
    options.input = CppBufferInput{"pointer-variadic-probe.c", R"c(
        typedef struct Mixed { double x; int y; } Mixed;
        typedef int (__attribute__((sysv_abi)) *MixedCallback)(int, ...);
        typedef int (__attribute__((sysv_abi)) *FixedCallback)(int);
        typedef int (*CdeclCallback)(int, ...);
        int __attribute__((sysv_abi)) direct(int, ...);
        void observe(Mixed, int);
    )c"};
    auto opened = factory.open(std::move(options), {});
    auto* holder = std::get_if<std::unique_ptr<CppInteropSession>>(&opened);
    if (!holder || !*holder) return 2;
    auto& session = **holder;
    const auto aliases = session.typeAliasesSnapshot();
    const auto functions = session.freeFunctionsSnapshot();
    if (!aliases || !functions) return 3;
    std::string pointerType, fixedType, cdeclType, directType;
    const CppType* mixed = nullptr;
    const CppType* integer = nullptr;
    for (const auto& alias : *aliases)
    {
        if (alias.name == "MixedCallback") pointerType = alias.underlyingType.canonicalSpelling;
        if (alias.name == "FixedCallback") fixedType = alias.underlyingType.canonicalSpelling;
        if (alias.name == "CdeclCallback") cdeclType = alias.underlyingType.canonicalSpelling;
    }
    for (const auto& function : *functions)
    {
        if (function.name == "direct") directType = function.canonicalFunctionType;
        if (function.name == "observe" && function.parameters.size() == 2)
        {
            mixed = &function.parameters[0].type;
            integer = &function.parameters[1].type;
        }
    }
    if (pointerType.empty() || fixedType.empty() || cdeclType.empty() || directType.empty() || !mixed || !integer) return 4;
    const auto pointerPlan = session.cVariadicFunctionPointerCallAbiPlan("MixedCallback", pointerType, {*mixed});
    auto directPlan = session.cVariadicCallAbiPlan("direct", directType, {*mixed});
    if (!pointerPlan || !directPlan ||
        !validateCppVariadicSysVCallPlan(*pointerPlan, "__nsc_variadic_pointer_abi_probe", 1) ||
        pointerPlan->arguments.size() != 2 || pointerPlan->arguments[1].physicalCount != 2 ||
        pointerPlan->physicalParameters.size() != 3 ||
        pointerPlan->physicalParameters[1].type.kind != abi_plan::TypeKind::Double ||
        pointerPlan->physicalParameters[2].type.kind != abi_plan::TypeKind::Integer ||
        pointerPlan->physicalParameters[2].type.widthOrCount != 32) return 5;
    directPlan->symbol = pointerPlan->symbol;
    if (abi_plan::encodeVariadicCallPlanForCache(*pointerPlan) !=
        abi_plan::encodeVariadicCallPlanForCache(*directPlan)) return 6;
    const std::vector<CppType> pressure{*integer, *integer, *integer, *integer, *integer, *mixed};
    const auto pointerPressure = session.cVariadicFunctionPointerCallAbiPlan("MixedCallback", pointerType, pressure);
    auto directPressure = session.cVariadicCallAbiPlan("direct", directType, pressure);
    if (!pointerPressure || !directPressure || pointerPressure->arguments.size() != 7 ||
        !pointerPressure->arguments[6].byValue ||
        !validateCppVariadicSysVCallPlan(*pointerPressure, "__nsc_variadic_pointer_abi_probe", 1)) return 9;
    directPressure->symbol = pointerPressure->symbol;
    if (abi_plan::encodeVariadicCallPlanForCache(*pointerPressure) !=
        abi_plan::encodeVariadicCallPlanForCache(*directPressure)) return 10;
    const auto afterQueries = session.freeFunctionsSnapshot();
    if (!afterQueries || afterQueries->size() != functions->size()) return 11;
    auto wrongRecord = *mixed;
    wrongRecord.sizeBits = 8;
    if (session.cVariadicFunctionPointerCallAbiPlan("MixedCallback", pointerType, {wrongRecord}) ||
        session.cVariadicFunctionPointerCallAbiPlan("MixedCallback", "wrong", {*mixed}) ||
        session.cVariadicFunctionPointerCallAbiPlan("Missing", pointerType, {*mixed}) ||
        session.cVariadicFunctionPointerCallAbiPlan("FixedCallback", fixedType, {*mixed}) ||
        session.cVariadicFunctionPointerCallAbiPlan("CdeclCallback", cdeclType, {*mixed})) return 7;
    if (!session.close().succeeded || session.cVariadicFunctionPointerCallAbiPlan("MixedCallback", pointerType, {*mixed})) return 8;
    return 0;
}
