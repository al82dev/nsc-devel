#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppInteropSession.h"
#include "nativesharp/Interop/CppVariadicAbiReconciliation.h"
#include "nativesharp/Interop/CppRegCallAbiValidation.h"
#include "nativesharp/Interop/CppVariadicAbiCodec.h"
#include "../../src/Interop/Internal/CppCRecordMetadataCodec.h"
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
        typedef struct Nested { Floats pair; float z; } Nested;
        typedef struct FloatArray { float values[3]; } FloatArray;
        typedef Small (__attribute__((regcall)) *SmallFn)(Small);
        typedef Large (__attribute__((regcall)) *LargeFn)(Large);
        typedef Floats (__attribute__((regcall)) *FloatsFn)(Floats);
        typedef Nested (__attribute__((regcall)) *NestedFn)(Nested);
        typedef FloatArray (__attribute__((regcall)) *FloatArrayFn)(FloatArray);
        Small __attribute__((regcall)) small(Small);
        Large __attribute__((regcall)) large(Large);
        Floats __attribute__((regcall)) floats(Floats);
        Nested __attribute__((regcall)) nested(Nested);
        FloatArray __attribute__((regcall)) float_array(FloatArray);
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
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& alias : *aliases)
            if (alias.name == aliasName) { aliasType = alias.underlyingType.canonicalSpelling; aliasPointer = &alias.underlyingType; }
        for (const auto& function : *functions)
            if (function.name == functionName) { functionType = function.canonicalFunctionType; declaration = &function; }
        if (aliasType.empty() || functionType.empty() || !declaration) return 4;
        const auto direct = session->cFunctionAbiPlan(functionName, functionType);
        const auto indirect = session->cFunctionPointerAbiPlan(aliasName, aliasType);
        if (std::string_view(aliasName) == "FloatsFn")
        {
            if (!direct || !indirect || direct->physicalReturn.kind != abi_plan::TypeKind::Structure ||
                direct->physicalParameters.size() != 2 ||
                direct->physicalParameters[0].type.kind != abi_plan::TypeKind::Float ||
                direct->physicalParameters[1].type.kind != abi_plan::TypeKind::Float ||
                direct->logicalParameters.size() != 1 || indirect->logicalParameters.size() != 1 ||
                direct->logicalParameters[0].passing != abi_plan::Passing::Expand ||
                indirect->logicalParameters[0].passing != abi_plan::Passing::Expand ||
                direct->logicalParameters[0].expandedOffsets != std::vector<std::uint64_t>{0, 4} ||
                indirect->logicalParameters[0].expandedOffsets != direct->logicalParameters[0].expandedOffsets) return 9;
            const auto encoded = abi_plan::encodeFunctionPointerPlanForCache(*indirect);
            if (!encoded) return 23;
            const auto decoded = abi_plan::decodeFunctionPointerPlanFromCache(*encoded);
            if (!decoded || decoded->logicalParameters.size() != 1 ||
                decoded->logicalParameters[0].passing != abi_plan::Passing::Expand ||
                decoded->logicalParameters[0].expandedOffsets != std::vector<std::uint64_t>{0, 4} ||
                abi_plan::decodeFunctionPointerPlanFromCache(encoded->substr(0, encoded->size() - 1))) return 24;
            if (!abi_plan::isRegCallWin64TwoFloatRecordRoundTrip(*direct, direct->symbol, 8, 4) ||
                !abi_plan::isRegCallWin64TwoFloatRecordRoundTrip(*decoded, decoded->symbol, 8, 4) ||
                abi_plan::isRegCallWin64TwoFloatRecordRoundTrip(*direct, "incorrect", 8, 4) ||
                abi_plan::isRegCallWin64TwoFloatRecordRoundTrip(*direct, direct->symbol, 8, 8)) return 28;
            auto wrongOffset = *direct; wrongOffset.logicalParameters[0].expandedOffsets[1] = 0;
            auto wrongType = *direct; wrongType.physicalParameters[1].type.kind = abi_plan::TypeKind::Integer;
            auto wrongReturn = *direct; wrongReturn.physicalReturn.elements[1].kind = abi_plan::TypeKind::Double;
            auto wrongAttribute = *direct; wrongAttribute.physicalParameters[0].inRegister = true;
            auto wrongConvention = *direct; wrongConvention.callingConvention = 78;
            auto wrongIndex = *direct; wrongIndex.logicalParameters[0].firstPhysical = 1;
            for (const auto* wrong : {&wrongOffset, &wrongType, &wrongReturn, &wrongAttribute, &wrongConvention, &wrongIndex})
                if (abi_plan::isRegCallWin64TwoFloatRecordRoundTrip(*wrong, wrong->symbol, 8, 4)) return 29;
            metadata_detail::CDependencyFunctionSnapshot snapshot;
            snapshot.moduleName = "PlanProbe"; snapshot.referenceIdentity = "reference:0";
            snapshot.linkSymbol = direct->symbol; snapshot.declaration = *declaration;
            snapshot.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*direct);
            std::vector<std::uint8_t> metadata;
            std::string error;
            metadata_detail::CDependencyFunctionSnapshot restored;
            if (!metadata_detail::encodeCDependencyFunction(snapshot, metadata, error) ||
                !metadata_detail::decodeCDependencyFunction(metadata, restored, error) || !restored.declaration.abiPlan ||
                !abi_plan::isRegCallWin64TwoFloatRecordRoundTrip(*restored.declaration.abiPlan, direct->symbol, 8, 4))
            { std::cerr << "metadata de record flotante: " << error << '\n'; return 30; }
            auto truncated = metadata; truncated.pop_back();
            if (metadata_detail::decodeCDependencyFunction(truncated, restored, error)) return 32;
            auto mismatched = snapshot;
            if (!mismatched.declaration.parameters[0].type.recordIdentity) return 33;
            mismatched.declaration.parameters[0].type.recordIdentity->clangUsr = "other-record";
            if (metadata_detail::encodeCDependencyFunction(mismatched, metadata, error)) return 34;
            mismatched = snapshot; mismatched.declaration.parameters[0].type.sizeBits = 32;
            if (metadata_detail::encodeCDependencyFunction(mismatched, metadata, error)) return 35;
            snapshot.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrongOffset);
            if (metadata_detail::encodeCDependencyFunction(snapshot, metadata, error)) return 31;
            auto invalid = *indirect; invalid.logicalParameters[0].expandedOffsets.pop_back();
            if (abi_plan::encodeFunctionPointerPlanForCache(invalid)) return 25;
            continue;
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
                  << " destino=" << direct->targetTriple
                  << " datos=" << direct->dataLayout
                  << " simbolo-puntero=" << indirect->symbol
                  << " retorno=" << static_cast<int>(direct->physicalReturn.kind)
                  << " bits=" << direct->physicalReturn.widthOrCount
                  << " paso=" << static_cast<int>(direct->logicalParameters[0].passing)
                  << " alineacion-retorno=" << direct->logicalReturn.alignment
                  << " alineacion-argumento=" << direct->logicalParameters[0].alignment
                  << " byval=" << direct->logicalParameters[0].byValue
                  << " retorno-paso=" << static_cast<int>(direct->logicalReturn.passing)
                  << " retorno-coercion=" << bool(direct->logicalReturn.coercion)
                  << " argumento-coercion=" << bool(direct->logicalParameters[0].coercion)
                  << " primero=" << direct->logicalParameters[0].firstPhysical
                  << " retorno-offset=" << direct->logicalReturn.offset
                  << " argumento-offset=" << direct->logicalParameters[0].offset
                  << " cuenta=" << direct->logicalParameters[0].physicalCount
                  << " inreg=" << direct->physicalParameters[0].inRegister
                  << " tipo-argumento=" << static_cast<int>(direct->physicalParameters[0].type.kind)
                  << " bits-argumento=" << direct->physicalParameters[0].type.widthOrCount
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
        const auto bytes = std::string_view(aliasName) == "SmallFn" ? 8U : 24U;
        if (!abi_plan::isRegCallWin64RecordRoundTrip(*direct, direct->symbol, bytes, 4) ||
            !abi_plan::isRegCallWin64RecordRoundTrip(*indirect, indirect->symbol, bytes, 4))
        { std::cerr << "rechazo de plan directo=" << abi_plan::isRegCallWin64RecordRoundTrip(*direct, direct->symbol, bytes, 4)
                    << " puntero=" << abi_plan::isRegCallWin64RecordRoundTrip(*indirect, indirect->symbol, bytes, 4) << '\n'; return 15; }
        auto invalidPlan = *direct; invalidPlan.callingConvention = 78;
        if (abi_plan::isRegCallWin64RecordRoundTrip(invalidPlan, direct->symbol, bytes, 4) ||
            abi_plan::isRegCallWin64RecordRoundTrip(*direct, "wrong-symbol", bytes, 4) ||
            (bytes == 24 && abi_plan::isRegCallWin64RecordRoundTrip(*direct, direct->symbol, bytes, 8)))
        { std::cerr << "plan alterado aceptado para " << aliasName << '\n'; return 16; }
        metadata_detail::CDependencyFunctionSnapshot snapshot;
        snapshot.moduleName = "PlanProbe"; snapshot.referenceIdentity = "reference:0";
        snapshot.linkSymbol = direct->symbol; snapshot.declaration = *declaration;
        snapshot.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*direct);
        std::vector<std::uint8_t> encoded;
        std::string error;
        metadata_detail::CDependencyFunctionSnapshot decoded;
        if (!metadata_detail::encodeCDependencyFunction(snapshot, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, decoded, error) || !decoded.declaration.abiPlan ||
            !abi_plan::isRegCallWin64RecordRoundTrip(*decoded.declaration.abiPlan, snapshot.linkSymbol, bytes, 4))
        { std::cerr << "contrato regcall: " << error << '\n'; return 17; }
        auto wrong = snapshot;
        auto wrongPlan = std::make_shared<abi_plan::Plan>(*direct);
        wrongPlan->callingConvention = 78;
        wrong.declaration.abiPlan = wrongPlan;
        if (metadata_detail::encodeCDependencyFunction(wrong, encoded, error)) return 18;
        wrong = snapshot;
        if (!wrong.declaration.parameters[0].type.recordIdentity) return 20;
        wrong.declaration.parameters[0].type.recordIdentity->clangUsr = "different-record";
        if (metadata_detail::encodeCDependencyFunction(wrong, encoded, error)) return 21;
        wrong = snapshot; wrong.declaration.parameters[0].type.sizeBits = 8;
        if (metadata_detail::encodeCDependencyFunction(wrong, encoded, error)) return 22;
        encoded.pop_back();
        if (metadata_detail::decodeCDependencyFunction(encoded, decoded, error)) return 19;
    }
    for (const auto& [aliasName, functionName] : {std::pair{"NestedFn", "nested"},
                                                  std::pair{"FloatArrayFn", "float_array"}})
    {
        std::string aliasType, functionType;
        for (const auto& alias : *aliases)
            if (alias.name == aliasName) aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == functionName) functionType = function.canonicalFunctionType;
        const auto direct = session->cFunctionAbiPlan(functionName, functionType);
        const auto pointer = session->cFunctionPointerAbiPlan(aliasName, aliasType);
        const std::vector<std::uint64_t> offsets{0, 4, 8};
        if (!direct || !pointer || direct->logicalParameters.size() != 1 || pointer->logicalParameters.size() != 1 ||
            direct->logicalParameters[0].passing != abi_plan::Passing::Expand ||
            pointer->logicalParameters[0].passing != abi_plan::Passing::Expand ||
            direct->logicalParameters[0].expandedOffsets != offsets ||
            pointer->logicalParameters[0].expandedOffsets != offsets || direct->physicalParameters.size() != 3 ||
            pointer->physicalParameters.size() != 3) return 26;
        for (std::size_t i = 0; i < 3; ++i)
            if (direct->physicalParameters[i].type.kind != abi_plan::TypeKind::Float ||
                pointer->physicalParameters[i].type.kind != abi_plan::TypeKind::Float) return 27;
    }
    CppSessionOptions cppOptions;
    cppOptions.language = CppLanguageMode::Cpp;
    cppOptions.languageStandard = "c++20";
    cppOptions.targetTriple = "x86_64-pc-windows-msvc";
    cppOptions.driverReferencePath = "clang.exe";
    cppOptions.resourceDirectory = argv[1];
    cppOptions.workingDirectory = argv[2];
    cppOptions.input = CppBufferInput{"regcall-record-plan.cpp", R"cpp(
        namespace A {
        struct Pair { float x, y; };
        using Callback = Pair (__attribute__((regcall)) *)(Pair);
        Pair __attribute__((regcall)) transform(Pair);
        int __attribute__((regcall)) transform(int);
        struct Incomplete;
        Incomplete __attribute__((regcall)) unavailable(Incomplete);
        }
        namespace B {
        struct Pair { float x, y; };
        using Callback = Pair (__attribute__((regcall)) *)(Pair);
        Pair __attribute__((regcall)) transform(Pair);
        }
    )cpp"};
    auto cppOpened = factory.open(std::move(cppOptions), {});
    if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(cppOpened)) return 36;
    auto cppSession = std::move(std::get<std::unique_ptr<CppInteropSession>>(cppOpened));
    const auto cppFunctions = cppSession->freeFunctionsSnapshot();
    if (!cppFunctions) return 37;
    std::string firstSymbol, secondSymbol;
    unsigned scalarCount = 0;
    bool sawIncomplete = false;
    for (const auto& function : *cppFunctions)
    {
        if (function.qualifiedName == "A::unavailable")
        { if (function.abiPlan) return 41; sawIncomplete = true; continue; }
        if (function.qualifiedName != "A::transform" && function.qualifiedName != "B::transform") continue;
        if (function.returnType.kind != CppTypeKind::Record)
        { if (function.abiPlan) return 38; ++scalarCount; continue; }
        if (!function.externalSymbol || !function.abiPlan ||
            !abi_plan::isRegCallWin64TwoFloatRecordRoundTrip(*function.abiPlan, *function.externalSymbol, 8, 4)) return 39;
        (function.qualifiedName == "A::transform" ? firstSymbol : secondSymbol) = *function.externalSymbol;
    }
    if (scalarCount != 1 || !sawIncomplete || firstSymbol.empty() || secondSymbol.empty() || firstSymbol == secondSymbol) return 40;
    const auto cppAliases = cppSession->typeAliasesSnapshot();
    if (!cppAliases) return 42;
    unsigned callbackCount = 0;
    for (const auto& alias : *cppAliases)
    {
        if (alias.qualifiedName != "A::Callback" && alias.qualifiedName != "B::Callback") continue;
        if (!alias.clangUsr || alias.clangUsr->empty()) return 43;
        const auto plan = cppSession->cppFunctionPointerAbiPlan(
            alias.qualifiedName, *alias.clangUsr, alias.underlyingType.canonicalSpelling);
        if (!plan || !abi_plan::isRegCallWin64TwoFloatRecordRoundTrip(
                *plan, "__regcall3____nsc_pointer_abi_probe", 8, 4) ||
            cppSession->cppFunctionPointerAbiPlan(alias.qualifiedName, "wrong-usr", alias.underlyingType.canonicalSpelling) ||
            cppSession->cppFunctionPointerAbiPlan("missing::Callback", *alias.clangUsr, alias.underlyingType.canonicalSpelling) ||
            cppSession->cppFunctionPointerAbiPlan(alias.qualifiedName, *alias.clangUsr, "wrong-type")) return 44;
        ++callbackCount;
    }
    if (callbackCount != 2) return 45;
    return 0;
}
