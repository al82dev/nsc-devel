#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppInteropSession.h"
#include "nativesharp/Interop/CppVariadicAbiReconciliation.h"
#include "nativesharp/Interop/CppRegCallAbiValidation.h"
#include "nativesharp/Interop/CppSysVRecordAbiValidation.h"
#include "nativesharp/Interop/CppVariadicAbiCodec.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryMetadataCodec.h"
#include "../../src/Interop/Internal/CppCRecordMetadataCodec.h"
#include <llvm/IR/CallingConv.h>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

using namespace nativesharp::interop;

int main(int argc, char** argv)
{
    if (argc == 3 && std::string_view(argv[1]) == "--inspect-cpp-hfa-catalog")
    {
        LibraryMetadataCodec codec;
        LibraryMetadata metadata;
        std::string error;
        if (!codec.read(argv[2], metadata, error) || !codec.verifyCurrentCompiler(metadata, error))
        { std::cerr << error << '\n'; return 46; }
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        metadata_detail::CDependencyCatalog dependencies;
        if (!metadata_detail::decodeCTypeCatalogAndDependencies(metadata.cppCRecordCatalog, metadata.formatMinor,
                records, enums, dependencies, error))
        { std::cerr << error << '\n'; return 47; }
        for (const auto& alias : dependencies.aliases)
            if (alias.declaration.name == "Callback")
                std::cout << "alias " << alias.declaration.qualifiedName << " ref=" << alias.referenceIdentity
                          << " plan=" << bool(alias.declaration.abiPlan) << '\n';
        for (const auto& function : dependencies.functions)
            if (function.declaration.name == "transform" || function.declaration.name == "fetch")
                std::cout << "function " << function.declaration.qualifiedName << " ref=" << function.referenceIdentity
                          << " plan=" << bool(function.declaration.abiPlan) << '\n';
        for (const auto& record : records)
            if (record.name == "Pair")
                std::cout << "record " << record.name << " cpp=" << bool(record.cppValueObservation)
                          << " fields=" << record.fields.size() << '\n';
        return 0;
    }
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
        typedef struct Other { int x; } Other;
        typedef struct FloatArray { float values[3]; } FloatArray;
        typedef struct Three { float x, y, z; } Three;
        typedef struct Four { float x, y, z, w; } Four;
        typedef Small (__attribute__((regcall)) *SmallFn)(Small);
        typedef Large (__attribute__((regcall)) *LargeFn)(Large);
        typedef Floats (__attribute__((regcall)) *FloatsFn)(Floats);
        typedef Nested (__attribute__((regcall)) *NestedFn)(Nested);
        typedef FloatArray (__attribute__((regcall)) *FloatArrayFn)(FloatArray);
        typedef Three (__attribute__((regcall)) *ThreeFn)(Three);
        typedef Four (__attribute__((regcall)) *FourFn)(Four);
        typedef Nested (__attribute__((regcall)) *NestedMixedFn)(Nested, int);
        typedef Nested (__attribute__((regcall)) *NestedManyFn)(Nested, int,int,int,int,int,int,int,int);
        typedef Nested (__attribute__((regcall)) *NestedRealFn)(Nested, int,float,double);
        typedef Nested (__attribute__((regcall)) *NestedWideFn)(Nested, long long,unsigned int,unsigned long long,int*,double);
        typedef Nested (__attribute__((regcall)) *NestedNarrowFn)(Nested, signed char,unsigned char,short,unsigned short,_Bool);
        typedef Nested (__attribute__((regcall)) *NestedLong32Fn)(Nested, long,unsigned long,double);
        typedef Nested (__attribute__((regcall)) *NestedPointersFn)(Nested, unsigned char*,short*,double*,void*);
        typedef Nested (__attribute__((regcall)) *NestedDoublePtrFn)(Nested, unsigned char**,short**,double**);
        typedef Nested (__attribute__((regcall)) *NestedRecordPtrFn)(Nested, Other*);
        typedef Nested (__attribute__((regcall)) *NestedConstFn)(Nested, const int*,int*);
        Small __attribute__((regcall)) small(Small);
        Large __attribute__((regcall)) large(Large);
        Floats __attribute__((regcall)) floats(Floats);
        Nested __attribute__((regcall)) nested(Nested);
        FloatArray __attribute__((regcall)) float_array(FloatArray);
        Three __attribute__((regcall)) three(Three);
        Four __attribute__((regcall)) four(Four);
        Nested __attribute__((regcall)) nested_mixed(Nested, int);
        Nested __attribute__((regcall)) nested_many(Nested, int,int,int,int,int,int,int,int);
        Nested __attribute__((regcall)) nested_real(Nested, int,float,double);
        Nested __attribute__((regcall)) nested_wide(Nested, long long,unsigned int,unsigned long long,int*,double);
        Nested __attribute__((regcall)) nested_narrow(Nested, signed char,unsigned char,short,unsigned short,_Bool);
        Nested __attribute__((regcall)) nested_long32(Nested, long,unsigned long,double);
        Nested __attribute__((regcall)) nested_pointers(Nested, unsigned char*,short*,double*,void*);
        Nested __attribute__((regcall)) nested_doubleptr(Nested, unsigned char**,short**,double**);
        Nested __attribute__((regcall)) nested_recordptr(Nested, Other*);
        Nested __attribute__((regcall)) nested_const(Nested, const int*,int*);
        Nested __attribute__((regcall)) nested_const_float(Nested, const float*,const double*);
        Nested __attribute__((regcall)) nested_const_void(Nested, const void*,int*);
        typedef Nested (__attribute__((regcall)) *NestedTwoFn)(Nested,Nested,int);
        Nested __attribute__((regcall)) nested_two(Nested,Nested,int);
        typedef struct DifferentFloats { float a,b,c; } DifferentFloats;
        Nested __attribute__((regcall)) nested_distinct(Nested,DifferentFloats,int);
        typedef Nested (__attribute__((regcall)) *NestedVariedFn)(Nested,Floats,int);
        Nested __attribute__((regcall)) nested_varied(Nested,Floats,int);
        Nested __attribute__((regcall)) nested_first(Floats,Nested,int);
        Nested __attribute__((regcall)) nested_interleaved(Nested,int,Floats);
        typedef struct MixedScalar { float x; int y; } MixedScalar;
        int __attribute__((regcall)) nested_mixed_scalar(Nested,int,MixedScalar);
        Nested __attribute__((regcall)) nested_mixed_result(Nested,int,MixedScalar);
        typedef struct CompactResult { int x,y; } CompactResult;
        CompactResult __attribute__((regcall)) nested_compact_result(Nested,int);
        typedef struct CompactShort { short x,y; } CompactShort;
        int __attribute__((regcall)) nested_compact_short(Nested,int,CompactShort);
        typedef struct LargeMixed { int x,y,z,w,u,v; } LargeMixed;
        LargeMixed __attribute__((regcall)) nested_indirect_result(Nested,int);
        int __attribute__((regcall)) nested_large_mixed(Nested,int,LargeMixed);
        typedef struct ThreeBytes { unsigned char x,y,z; } ThreeBytes;
        typedef struct DoublePair { double x,y; } DoublePair;
        double __attribute__((regcall)) double_pair(DoublePair,int);
        typedef struct DoubleTriple { double x,y,z; } DoubleTriple;
        int __attribute__((regcall)) double_triple_int(DoubleTriple,int);
        float __attribute__((regcall)) double_triple_float(DoubleTriple,int);
        _Bool __attribute__((regcall)) double_triple_bool(DoubleTriple,int);
        void __attribute__((regcall)) double_triple_void(DoubleTriple,int*);
        long long __attribute__((regcall)) double_triple_i64(DoubleTriple,int);
        short __attribute__((regcall)) double_triple_short(DoubleTriple,int);
        int* __attribute__((regcall)) double_triple_pointer(DoubleTriple,int*);
        DoublePair __attribute__((regcall)) double_result(Nested,int);
        DoublePair __attribute__((regcall)) double_result_scalar(int);
        typedef struct MixedReal { float x; double y; } MixedReal;
        double __attribute__((regcall)) mixed_real(MixedReal,int);
        int __attribute__((regcall)) mixed_real_int(MixedReal,int);
        void __attribute__((regcall)) mixed_real_void(MixedReal,int*);
        float __attribute__((regcall)) mixed_real_float(MixedReal,int);
        long long __attribute__((regcall)) mixed_real_i64(MixedReal,int);
        short __attribute__((regcall)) mixed_real_short(MixedReal,int);
        _Bool __attribute__((regcall)) mixed_real_bool(MixedReal,int);
        int* __attribute__((regcall)) mixed_real_pointer(MixedReal,int*);
        int __attribute__((regcall)) nested_three_bytes(Nested,int,ThreeBytes);
        int __attribute__((regcall)) nested_return_int(Nested,int,Floats);
        unsigned int __attribute__((regcall)) nested_return_uint(Nested,int,Floats);
        long long __attribute__((regcall)) nested_return_i64(Nested,int,Floats);
        signed char __attribute__((regcall)) nested_return_narrow(Nested,int,Floats);
        _Bool __attribute__((regcall)) nested_return_bool(Nested,int,Floats);
        double __attribute__((regcall)) nested_return_double(Nested,int,Floats);
        float __attribute__((regcall)) nested_return_float(Nested,int,Floats);
        void __attribute__((regcall)) nested_return_void(Nested,int,Floats,int*);
        int* __attribute__((regcall)) nested_return_pointer(Nested,int,Floats,int*);
        float* __attribute__((regcall)) nested_return_float_pointer(Nested,int,Floats,float*);
        Nested* __attribute__((regcall)) nested_return_record_pointer(Nested,int,Floats,Nested*);
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
            if (!abi_plan::isRegCallWin64FloatRecordRoundTrip(*direct, direct->symbol, 8, 4) ||
                !abi_plan::isRegCallWin64FloatRecordRoundTrip(*decoded, decoded->symbol, 8, 4) ||
                abi_plan::isRegCallWin64FloatRecordRoundTrip(*direct, "incorrect", 8, 4) ||
                abi_plan::isRegCallWin64FloatRecordRoundTrip(*direct, direct->symbol, 8, 8)) return 28;
            auto wrongOffset = *direct; wrongOffset.logicalParameters[0].expandedOffsets[1] = 0;
            auto wrongType = *direct; wrongType.physicalParameters[1].type.kind = abi_plan::TypeKind::Integer;
            auto wrongReturn = *direct; wrongReturn.physicalReturn.elements[1].kind = abi_plan::TypeKind::Double;
            auto wrongAttribute = *direct; wrongAttribute.physicalParameters[0].inRegister = true;
            auto wrongConvention = *direct; wrongConvention.callingConvention = 78;
            auto wrongIndex = *direct; wrongIndex.logicalParameters[0].firstPhysical = 1;
            for (const auto* wrong : {&wrongOffset, &wrongType, &wrongReturn, &wrongAttribute, &wrongConvention, &wrongIndex})
                if (abi_plan::isRegCallWin64FloatRecordRoundTrip(*wrong, wrong->symbol, 8, 4)) return 29;
            metadata_detail::CDependencyFunctionSnapshot snapshot;
            snapshot.moduleName = "PlanProbe"; snapshot.referenceIdentity = "reference:0";
            snapshot.linkSymbol = direct->symbol; snapshot.declaration = *declaration;
            snapshot.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*direct);
            std::vector<std::uint8_t> metadata;
            std::string error;
            metadata_detail::CDependencyFunctionSnapshot restored;
            if (!metadata_detail::encodeCDependencyFunction(snapshot, metadata, error) ||
                !metadata_detail::decodeCDependencyFunction(metadata, restored, error) || !restored.declaration.abiPlan ||
                !abi_plan::isRegCallWin64FloatRecordRoundTrip(*restored.declaration.abiPlan, direct->symbol, 8, 4))
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
        if (!abi_plan::isRegCallWin64FloatRecordRoundTrip(*direct, direct->symbol, 12, 4) ||
            !abi_plan::isRegCallWin64FloatRecordRoundTrip(*pointer, pointer->symbol, 12, 4)) return 53;
        auto wrong = *direct;
        wrong.physicalReturn.elements[0].kind = abi_plan::TypeKind::Integer;
        if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4)) return 54;
        wrong = *direct;
        wrong.logicalReturn.coercion->elements[0].widthOrCount = 99;
        if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4)) return 55;
    }
    for (const auto& [aliasName, functionName, bytes] : {std::tuple{"ThreeFn", "three", 12U},
                                                         std::tuple{"FourFn", "four", 16U}})
    {
        std::string aliasType, functionType;
        for (const auto& alias : *aliases)
            if (alias.name == aliasName) aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == functionName) functionType = function.canonicalFunctionType;
        const auto direct = session->cFunctionAbiPlan(functionName, functionType);
        const auto pointer = session->cFunctionPointerAbiPlan(aliasName, aliasType);
        if (!direct || !pointer || !abi_plan::isRegCallWin64FloatRecordRoundTrip(*direct, direct->symbol, bytes, 4) ||
            !abi_plan::isRegCallWin64FloatRecordRoundTrip(*pointer, pointer->symbol, bytes, 4)) return 51;
        auto wrong = *direct; wrong.logicalParameters[0].expandedOffsets.back() = 0;
        if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, bytes, 4)) return 52;
    }
    {
        std::string aliasType, functionType;
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedMixedFn") aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == "nested_mixed") { functionType = function.canonicalFunctionType; declaration = &function; }
        const auto direct = session->cFunctionAbiPlan("nested_mixed", functionType);
        const auto indirect = session->cFunctionPointerAbiPlan("NestedMixedFn", aliasType);
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!plan || plan->logicalParameters.size() != 2 || plan->physicalParameters.size() != 4 ||
                plan->logicalParameters[0].passing != abi_plan::Passing::Expand ||
                plan->logicalParameters[0].expandedOffsets != std::vector<std::uint64_t>{0, 4, 8} ||
                plan->logicalParameters[1].passing != abi_plan::Passing::Direct ||
                plan->logicalParameters[1].firstPhysical != 3 ||
                plan->logicalParameters[1].physicalCount != 1 ||
                plan->physicalParameters[3].type.kind != abi_plan::TypeKind::Integer ||
                plan->physicalParameters[3].type.widthOrCount != 32) return 56;
            if (!abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, true) ||
                abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4)) return 57;
            auto wrong = *plan;
            wrong.logicalParameters[1].firstPhysical = 2;
            if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4, true)) return 58;
            wrong = *plan;
            wrong.physicalParameters[3].type.widthOrCount = 64;
            if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4, true)) return 59;
            wrong = *plan;
            wrong.physicalParameters[3].signExtend = true;
            if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4, true)) return 60;
        }
        if (!declaration || !direct) return 61;
        metadata_detail::CDependencyFunctionSnapshot snapshot;
        snapshot.moduleName = "PlanProbe"; snapshot.referenceIdentity = "reference:0";
        snapshot.linkSymbol = direct->symbol; snapshot.declaration = *declaration;
        snapshot.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*direct);
        std::vector<std::uint8_t> encoded;
        std::string error;
        metadata_detail::CDependencyFunctionSnapshot restored;
        if (!metadata_detail::encodeCDependencyFunction(snapshot, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error) || !restored.declaration.abiPlan ||
            !abi_plan::isRegCallWin64FloatRecordRoundTrip(*restored.declaration.abiPlan, direct->symbol, 12, 4, true))
            return 62;
        auto altered = snapshot;
        altered.declaration.parameters[1].type.canonicalSpelling = "unsigned int";
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 63;
    }
    {
        std::string aliasType, functionType;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedManyFn") aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == "nested_many") functionType = function.canonicalFunctionType;
        const auto direct = session->cFunctionAbiPlan("nested_many", functionType);
        const auto indirect = session->cFunctionPointerAbiPlan("NestedManyFn", aliasType);
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 8) ||
                abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 7)) return 68;
            auto wrong = *plan; wrong.logicalParameters[7].firstPhysical = 0;
            if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4, 8)) return 69;
        }
    }
    {
        std::string aliasType, functionType;
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedRealFn") aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == "nested_real") { functionType = function.canonicalFunctionType; declaration = &function; }
        const auto direct = session->cFunctionAbiPlan("nested_real", functionType);
        const auto indirect = session->cFunctionPointerAbiPlan("NestedRealFn", aliasType);
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!declaration || !plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(
                    *plan, plan->symbol, 12, 4, 3, true) ||
                abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 3)) return 70;
            for (std::size_t i = 1; i < declaration->parameters.size(); ++i)
                if (!abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[i + 2].type,
                        declaration->parameters[i].type)) return 71;
            if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[4].type,
                    declaration->parameters[3].type)) return 72;
            auto wrong = *plan; wrong.physicalParameters[4].type.kind = abi_plan::TypeKind::Double;
            if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4, 3, true)) return 73;
        }
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = direct->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*direct);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error) || !restored.declaration.abiPlan ||
            !abi_plan::isRegCallWin64FloatRecordRoundTrip(*restored.declaration.abiPlan, direct->symbol, 12, 4, 3, true))
            return 74;
        saved.declaration.parameters[2].type = saved.declaration.parameters[3].type;
        if (metadata_detail::encodeCDependencyFunction(saved, encoded, error)) return 75;
    }
    {
        std::string aliasType, functionType;
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedWideFn") aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == "nested_wide") { functionType = function.canonicalFunctionType; declaration = &function; }
        const auto direct = session->cFunctionAbiPlan("nested_wide", functionType);
        const auto indirect = session->cFunctionPointerAbiPlan("NestedWideFn", aliasType);
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!declaration || !plan || plan->logicalParameters.size() != 6 || plan->physicalParameters.size() != 8 ||
                plan->physicalParameters[3].type.kind != abi_plan::TypeKind::Integer ||
                plan->physicalParameters[3].type.widthOrCount != 64 ||
                plan->physicalParameters[4].type.kind != abi_plan::TypeKind::Integer ||
                plan->physicalParameters[4].type.widthOrCount != 32 ||
                plan->physicalParameters[5].type.kind != abi_plan::TypeKind::Integer ||
                plan->physicalParameters[5].type.widthOrCount != 64 ||
                plan->physicalParameters[6].type.kind != abi_plan::TypeKind::Pointer ||
                plan->physicalParameters[7].type.kind != abi_plan::TypeKind::Double) return 76;
            const auto& pointer = plan->logicalParameters[4];
            const auto& physical = plan->physicalParameters[6];
            if (pointer.passing != abi_plan::Passing::Direct || !pointer.coercion ||
                pointer.coercion->kind != abi_plan::TypeKind::Pointer || pointer.firstPhysical != 6 ||
                pointer.physicalCount != 1 || pointer.offset || pointer.alignment || pointer.byValue ||
                physical.pointee || physical.alignment || physical.structureReturn || physical.byValue ||
                physical.signExtend || physical.zeroExtend || physical.inRegister || physical.noAlias) return 77;
            if (!abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 5, true)) return 78;
            for (std::size_t i = 1; i < declaration->parameters.size(); ++i)
                if (!abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[i + 2].type,
                    declaration->parameters[i].type)) return 79;
            if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[6].type,
                    declaration->parameters[1].type)) return 80;
            auto wrong = *plan; wrong.physicalParameters[6].inRegister = true;
            if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4, 5, true)) return 81;
        }
    }
    {
        std::string aliasType, functionType;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedNarrowFn") aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == "nested_narrow") functionType = function.canonicalFunctionType;
        const auto direct = session->cFunctionAbiPlan("nested_narrow", functionType);
        const auto indirect = session->cFunctionPointerAbiPlan("NestedNarrowFn", aliasType);
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!plan || plan->logicalParameters.size() != 6 || plan->physicalParameters.size() != 8) return 82;
            const std::uint64_t widths[] = {8,8,16,16,1};
            for (std::size_t i = 0; i < 5; ++i)
            {
                const auto& logical = plan->logicalParameters[i+1];
                const auto& physical = plan->physicalParameters[i+3];
                if (physical.type.kind != abi_plan::TypeKind::Integer || physical.type.widthOrCount != widths[i] ||
                    logical.firstPhysical != i+3 || logical.physicalCount != 1 ||
                    physical.zeroExtend != (i==4) || physical.signExtend) return 83;
                if (logical.passing != (i==4 ? abi_plan::Passing::Extend : abi_plan::Passing::Direct) ||
                    !logical.coercion || logical.signExtend || logical.zeroExtend != (i==4)) return 84;
            }
            if (!abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 5, true)) return 85;
            auto wrong = *plan; wrong.physicalParameters[7].zeroExtend = false;
            if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4, 5, true)) return 86;
        }
    }
    {
        std::string aliasType, functionType;
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedLong32Fn") aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == "nested_long32") { functionType = function.canonicalFunctionType; declaration = &function; }
        const auto direct = session->cFunctionAbiPlan("nested_long32", functionType);
        const auto indirect = session->cFunctionPointerAbiPlan("NestedLong32Fn", aliasType);
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!declaration || !plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(
                    *plan, plan->symbol, 12, 4, 3, true)) return 87;
            for (std::size_t i = 1; i < declaration->parameters.size(); ++i)
                if (!abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[i + 2].type,
                        declaration->parameters[i].type)) return 88;
            auto wrong = declaration->parameters[1].type; wrong.sizeBits = 64;
            if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type, wrong)) return 89;
        }
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = direct->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*direct);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 90;
        saved.declaration.parameters[1].type.sizeBits = 64;
        if (metadata_detail::encodeCDependencyFunction(saved, encoded, error)) return 91;
    }
    {
        std::string aliasType, functionType;
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedPointersFn") aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == "nested_pointers") { functionType = function.canonicalFunctionType; declaration = &function; }
        const auto direct = session->cFunctionAbiPlan("nested_pointers", functionType);
        const auto indirect = session->cFunctionPointerAbiPlan("NestedPointersFn", aliasType);
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!declaration || !plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(
                    *plan, plan->symbol, 12, 4, 4, true)) return 92;
            for (std::size_t i = 1; i < declaration->parameters.size(); ++i)
                if (!abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[i + 2].type,
                        declaration->parameters[i].type)) return 93;
            auto wrong = declaration->parameters[1].type;
            auto qualified = std::make_shared<CppType>(*wrong.element); qualified->isConst = true;
            wrong.element = qualified;
            if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type, wrong)) return 94;
            wrong = declaration->parameters[1].type; wrong.canonicalSpelling = "int *";
            if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type, wrong)) return 95;
        }
    }
    {
        std::string aliasType, functionType;
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedDoublePtrFn") aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == "nested_doubleptr") { functionType = function.canonicalFunctionType; declaration = &function; }
        const auto direct = session->cFunctionAbiPlan("nested_doubleptr", functionType);
        const auto indirect = session->cFunctionPointerAbiPlan("NestedDoublePtrFn", aliasType);
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!declaration || !plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(
                    *plan, plan->symbol, 12, 4, 3, true)) return 96;
            for (std::size_t i = 1; i < declaration->parameters.size(); ++i)
                if (!abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[i + 2].type,
                        declaration->parameters[i].type)) return 97;
            auto wrong = declaration->parameters[1].type;
            auto intermediate = std::make_shared<CppType>(*wrong.element); intermediate->isConst = true;
            wrong.element = intermediate;
            if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type, wrong)) return 98;
        }
    }
    {
        std::string aliasType, functionType;
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedRecordPtrFn") aliasType = alias.underlyingType.canonicalSpelling;
        for (const auto& function : *functions)
            if (function.name == "nested_recordptr") { functionType = function.canonicalFunctionType; declaration = &function; }
        const auto direct = session->cFunctionAbiPlan("nested_recordptr", functionType);
        const auto indirect = session->cFunctionPointerAbiPlan("NestedRecordPtrFn", aliasType);
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!declaration || !plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(
                    *plan, plan->symbol, 12, 4, 1, true) ||
                !abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type,
                    declaration->parameters[1].type)) return 99;
            auto wrong = declaration->parameters[1].type;
            auto record = std::make_shared<CppType>(*wrong.element);
            record->recordIdentity->clangUsr.reset(); wrong.element = record;
            if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type, wrong)) return 100;
            wrong = declaration->parameters[1].type;
            record = std::make_shared<CppType>(*wrong.element);
            record->recordIdentity->name = "Forged"; wrong.element = record;
            if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type, wrong)) return 101;
            wrong = declaration->parameters[1].type;
            record = std::make_shared<CppType>(*wrong.element);
            record->isConst = true; record->canonicalSpelling = "const " + record->canonicalSpelling;
            wrong.canonicalSpelling = record->canonicalSpelling + " *"; wrong.element = record;
            if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type, wrong)) return 106;
        }
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_const") declaration = &function;
        if (!declaration || declaration->parameters.size() != 3 ||
            !declaration->parameters[1].type.element) return 102;
        const auto& pointer = declaration->parameters[1].type;
        const auto& element = *pointer.element;
        if (!element.isConst || element.kind != CppTypeKind::SignedInteger) return 103;
        const auto plan = session->cFunctionAbiPlan("nested_const", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 2, true) ||
            !abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type, pointer)) return 104;
        auto altered = pointer;
        auto bad = std::make_shared<CppType>(*altered.element);
        bad->unqualifiedCanonicalSpelling = "double"; altered.element = bad;
        if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type, altered)) return 105;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_const_float") declaration = &function;
        if (!declaration || declaration->parameters.size() != 3) return 107;
        const auto plan = session->cFunctionAbiPlan("nested_const_float", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 2, true)) return 108;
        for (std::size_t i = 1; i < 3; ++i)
            if (!abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[i + 2].type,
                    declaration->parameters[i].type)) return 109;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_const_void") declaration = &function;
        if (!declaration || declaration->parameters.size() != 3 ||
            !declaration->parameters[1].type.element) return 110;
        const auto plan = session->cFunctionAbiPlan("nested_const_void", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 2, true) ||
            !abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type,
                declaration->parameters[1].type)) return 111;
        auto wrong = declaration->parameters[1].type;
        auto pointee = std::make_shared<CppType>(*wrong.element);
        pointee->unqualifiedCanonicalSpelling = "int"; wrong.element = pointee;
        if (abi_plan::regCallFloatRecordScalarMatches(plan->physicalParameters[3].type, wrong)) return 112;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_two") declaration = &function;
        if (!declaration) return 113;
        const auto direct = session->cFunctionAbiPlan("nested_two", declaration->canonicalFunctionType);
        std::string pointerType;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedTwoFn") pointerType = alias.underlyingType.canonicalSpelling;
        const auto indirect = session->cFunctionPointerAbiPlan("NestedTwoFn", pointerType);
        if (!direct || !indirect)
            std::cerr << "nested_two: direct=" << bool(direct) << " indirect=" << bool(indirect) << '\n';
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!plan) { std::cerr << "nested_two: plan absent\n"; return 114; }
            if (!abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 1, true, 2))
            {
                std::cerr << "nested_two: logical=" << plan->logicalParameters.size()
                          << " physical=" << plan->physicalParameters.size() << '\n';
                for (const auto& p : plan->logicalParameters)
                    std::cerr << "passing=" << static_cast<int>(p.passing) << " first=" << p.firstPhysical
                              << " count=" << p.physicalCount << " offsets=" << p.expandedOffsets.size() << '\n';
                return 114;
            }
            auto wrong = *plan;
            wrong.logicalParameters[1].firstPhysical = 2;
            if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4, 1, true, 2)) return 115;
            wrong = *plan; wrong.logicalParameters[1].expandedOffsets[1] = 8;
            if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4, 1, true, 2)) return 116;
            wrong = *plan; wrong.physicalParameters[5].type.kind = abi_plan::TypeKind::Double;
            if (abi_plan::isRegCallWin64FloatRecordRoundTrip(wrong, wrong.symbol, 12, 4, 1, true, 2)) return 117;
        }
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = direct->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*direct);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error) || !restored.declaration.abiPlan ||
            !abi_plan::isRegCallWin64FloatRecordRoundTrip(*restored.declaration.abiPlan, direct->symbol, 12, 4, 1, true, 2)) return 118;
        auto wrong = saved;
        if (!wrong.declaration.parameters[1].type.recordIdentity) return 119;
        wrong.declaration.parameters[1].type.recordIdentity->clangUsr = "other-record";
        if (metadata_detail::encodeCDependencyFunction(wrong, encoded, error)) return 120;
        wrong = saved;
        auto wrongPlan = std::make_shared<abi_plan::Plan>(*direct);
        wrongPlan->logicalParameters[1].expandedOffsets[1] = 8;
        wrong.declaration.abiPlan = wrongPlan;
        if (metadata_detail::encodeCDependencyFunction(wrong, encoded, error)) return 121;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_distinct") declaration = &function;
        if (!declaration) return 126;
        const auto plan = session->cFunctionAbiPlan("nested_distinct", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 1, true, 2)) return 127;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 128;
        auto wrong = saved;
        if (!wrong.declaration.parameters[1].type.recordIdentity || !wrong.declaration.returnType.recordIdentity) return 129;
        wrong.declaration.parameters[1].type.recordIdentity->clangUsr = wrong.declaration.returnType.recordIdentity->clangUsr;
        if (metadata_detail::encodeCDependencyFunction(wrong, encoded, error)) return 130;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        std::string aliasType;
        for (const auto& function : *functions)
            if (function.name == "nested_varied") declaration = &function;
        for (const auto& alias : *aliases)
            if (alias.name == "NestedVariedFn") aliasType = alias.underlyingType.canonicalSpelling;
        if (!declaration || aliasType.empty()) return 131;
        const auto direct = session->cFunctionAbiPlan("nested_varied", declaration->canonicalFunctionType);
        const auto indirect = session->cFunctionPointerAbiPlan("NestedVariedFn", aliasType);
        for (const auto* plan : {direct ? &*direct : nullptr, indirect ? &*indirect : nullptr})
        {
            if (!plan || !abi_plan::isRegCallWin64FloatRecordSequence(*plan, plan->symbol, 12, 4) ||
                plan->logicalParameters[0].physicalCount != 3 || plan->logicalParameters[1].physicalCount != 2 ||
                plan->logicalParameters[2].firstPhysical != 5 ||
                abi_plan::isRegCallWin64FloatRecordRoundTrip(*plan, plan->symbol, 12, 4, 1, true, 2)) return 132;
            auto wrong = *plan; wrong.logicalParameters[1].firstPhysical = 4;
            if (abi_plan::isRegCallWin64FloatRecordSequence(wrong, wrong.symbol, 12, 4)) return 133;
            wrong = *plan; wrong.logicalParameters[1].expandedOffsets[1] = 0;
            if (abi_plan::isRegCallWin64FloatRecordSequence(wrong, wrong.symbol, 12, 4)) return 134;
        }
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = direct->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*direct);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 135;
        auto wrong = saved; wrong.declaration.parameters[1].type.sizeBits = 96;
        if (metadata_detail::encodeCDependencyFunction(wrong, encoded, error)) return 136;
        wrong = saved; wrong.declaration.parameters[1].type.recordIdentity->clangUsr =
            wrong.declaration.returnType.recordIdentity->clangUsr;
        if (metadata_detail::encodeCDependencyFunction(wrong, encoded, error)) return 137;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_first") declaration = &function;
        if (!declaration) return 142;
        const auto plan = session->cFunctionAbiPlan("nested_first", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordSequence(*plan, plan->symbol, 12, 4) ||
            plan->logicalParameters[0].physicalCount != 2 || plan->logicalParameters[1].physicalCount != 3) return 143;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 144;
        auto wrong = saved; wrong.declaration.parameters[0].type.sizeBits = 96;
        if (metadata_detail::encodeCDependencyFunction(wrong, encoded, error)) return 145;
        wrong = saved; wrong.declaration.parameters[0].type.recordIdentity->clangUsr =
            wrong.declaration.returnType.recordIdentity->clangUsr;
        if (metadata_detail::encodeCDependencyFunction(wrong, encoded, error)) return 146;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_interleaved") declaration = &function;
        if (!declaration) return 151;
        const auto plan = session->cFunctionAbiPlan("nested_interleaved", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordInterleaved(*plan, plan->symbol, 12, 4) ||
            plan->logicalParameters[0].physicalCount != 3 || plan->logicalParameters[1].firstPhysical != 3 ||
            plan->logicalParameters[2].firstPhysical != 4 || plan->logicalParameters[2].physicalCount != 2 ||
            abi_plan::isRegCallWin64FloatRecordSequence(*plan, plan->symbol, 12, 4)) return 152;
        auto wrong = *plan; wrong.logicalParameters[2].firstPhysical = 3;
        if (abi_plan::isRegCallWin64FloatRecordInterleaved(wrong, wrong.symbol, 12, 4)) return 153;
        wrong = *plan; wrong.physicalParameters[3].type.widthOrCount = 64;
        if (abi_plan::isRegCallWin64FloatRecordInterleaved(wrong, wrong.symbol, 12, 4) &&
            abi_plan::regCallFloatRecordScalarMatches(wrong.physicalParameters[3].type, declaration->parameters[1].type)) return 154;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 155;
        auto altered = saved; altered.declaration.parameters[2].type.sizeBits = 96;
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 156;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 157;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_return_int") declaration = &function;
        if (!declaration) return 166;
        const auto plan = session->cFunctionAbiPlan("nested_return_int", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningInt(*plan, plan->symbol) ||
            !abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, declaration->returnType)) return 167;
        auto wrong = *plan; wrong.physicalReturn.widthOrCount = 64;
        if (abi_plan::isRegCallWin64FloatRecordsReturningInt(wrong, wrong.symbol)) return 168;
        wrong = *plan; wrong.logicalParameters[2].expandedOffsets[1] = 0;
        if (abi_plan::isRegCallWin64FloatRecordsReturningInt(wrong, wrong.symbol)) return 169;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 170;
        auto altered = saved; altered.declaration.returnType.sizeBits = 64;
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 171;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 172;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_return_double") declaration = &function;
        if (!declaration) return 177;
        const auto plan = session->cFunctionAbiPlan("nested_return_double", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningDouble(*plan, plan->symbol) ||
            !abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, declaration->returnType)) return 178;
        auto wrong = *plan; wrong.physicalReturn.kind = abi_plan::TypeKind::Float;
        if (abi_plan::isRegCallWin64FloatRecordsReturningDouble(wrong, wrong.symbol)) return 179;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 180;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 181;
        altered = saved; altered.declaration.returnType.sizeBits = 32;
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 182;
    }
    {
        const CppFunctionDeclaration* scalar = nullptr;
        const CppFunctionDeclaration* record = nullptr;
        for (const auto& function : *functions)
        {
            if (function.name == "nested_return_float_pointer") scalar = &function;
            if (function.name == "nested_return_record_pointer") record = &function;
        }
        if (!scalar || !record || !abi_plan::regCallFloatRecordScalarPointerReturnSupported(scalar->returnType) ||
            abi_plan::regCallFloatRecordScalarPointerReturnSupported(record->returnType)) return 210;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_return_pointer") declaration = &function;
        if (!declaration) return 205;
        const auto plan = session->cFunctionAbiPlan("nested_return_pointer", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningPointer(*plan, plan->symbol) ||
            !abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, declaration->returnType)) return 206;
        auto wrong = *plan; wrong.physicalReturn.kind = abi_plan::TypeKind::Integer;
        if (abi_plan::isRegCallWin64FloatRecordsReturningPointer(wrong, wrong.symbol)) return 207;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 208;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 209;
    }
    {
        for (const auto& [name, validate] : std::vector<std::pair<std::string_view, bool (*)(const abi_plan::Plan&, std::string_view) noexcept>>{
                {"double_triple_i64", abi_plan::isRegCallWin64FloatRecordsReturningInt64},
                {"double_triple_short", abi_plan::isRegCallWin64FloatRecordsReturningNarrowInteger},
                {"double_triple_pointer", abi_plan::isRegCallWin64FloatRecordsReturningPointer}})
        {
            const CppFunctionDeclaration* declaration = nullptr;
            for (const auto& function : *functions)
                if (function.name == name) declaration = &function;
            if (!declaration) return 158;
            const auto plan = session->cFunctionAbiPlan(name, declaration->canonicalFunctionType);
            if (!plan || !validate(*plan, plan->symbol)) return 159;
            std::vector<CppType> parameters;
            for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
            if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 160;
            auto wrong = *plan;
            wrong.logicalParameters[0].expandedOffsets[1] = 4;
            if (validate(wrong, wrong.symbol)) return 161;
            metadata_detail::CDependencyFunctionSnapshot saved;
            saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
            saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
            saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
            metadata_detail::CDependencyFunctionSnapshot restored;
            std::vector<std::uint8_t> encoded;
            std::string error;
            if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
                !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 162;
            auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 163;
        }
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "double_triple_void") declaration = &function;
        if (!declaration) return 151;
        const auto plan = session->cFunctionAbiPlan("double_triple_void", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningVoid(*plan, plan->symbol)) return 152;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 153;
        auto wrong = *plan;
        wrong.logicalParameters[0].expandedOffsets[1] = 4;
        if (abi_plan::isRegCallWin64FloatRecordsReturningVoid(wrong, wrong.symbol)) return 154;
        wrong = *plan;
        wrong.physicalReturn.kind = abi_plan::TypeKind::Integer;
        if (abi_plan::isRegCallWin64FloatRecordsReturningVoid(wrong, wrong.symbol)) return 155;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 156;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 157;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "double_triple_bool") declaration = &function;
        if (!declaration) return 144;
        const auto plan = session->cFunctionAbiPlan("double_triple_bool", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningBool(*plan, plan->symbol)) return 145;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 146;
        auto wrong = *plan;
        wrong.logicalParameters[0].expandedOffsets[1] = 4;
        if (abi_plan::isRegCallWin64FloatRecordsReturningBool(wrong, wrong.symbol)) return 147;
        wrong = *plan;
        wrong.returnZeroExtend = false;
        if (abi_plan::isRegCallWin64FloatRecordsReturningBool(wrong, wrong.symbol)) return 148;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 149;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 150;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "double_triple_float") declaration = &function;
        if (!declaration) return 137;
        const auto plan = session->cFunctionAbiPlan("double_triple_float", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningFloat(*plan, plan->symbol)) return 138;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 139;
        auto wrong = *plan;
        wrong.logicalParameters[0].expandedOffsets[1] = 4;
        if (abi_plan::isRegCallWin64FloatRecordsReturningFloat(wrong, wrong.symbol)) return 140;
        wrong = *plan;
        wrong.physicalReturn.kind = abi_plan::TypeKind::Double;
        if (abi_plan::isRegCallWin64FloatRecordsReturningFloat(wrong, wrong.symbol)) return 141;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 142;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 143;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "double_triple_int") declaration = &function;
        if (!declaration) return 130;
        const auto plan = session->cFunctionAbiPlan("double_triple_int", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningInt(*plan, plan->symbol)) return 131;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 132;
        auto wrong = *plan;
        wrong.logicalParameters[0].expandedOffsets[1] = 4;
        if (abi_plan::isRegCallWin64FloatRecordsReturningInt(wrong, wrong.symbol)) return 133;
        wrong = *plan;
        wrong.physicalParameters[0].type.kind = abi_plan::TypeKind::Float;
        if (abi_plan::isRegCallWin64FloatRecordsReturningInt(wrong, wrong.symbol)) return 134;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 135;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 136;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "mixed_real_pointer") declaration = &function;
        if (!declaration) return 123;
        const auto plan = session->cFunctionAbiPlan("mixed_real_pointer", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningPointer(*plan, plan->symbol)) return 124;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 125;
        auto wrong = *plan;
        wrong.physicalParameters[0].alignment = 4;
        if (abi_plan::isRegCallWin64FloatRecordsReturningPointer(wrong, wrong.symbol)) return 126;
        wrong = *plan;
        wrong.physicalReturn.kind = abi_plan::TypeKind::Integer;
        if (abi_plan::isRegCallWin64FloatRecordsReturningPointer(wrong, wrong.symbol)) return 127;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 128;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 129;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "mixed_real_bool") declaration = &function;
        if (!declaration) return 116;
        const auto plan = session->cFunctionAbiPlan("mixed_real_bool", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningBool(*plan, plan->symbol)) return 117;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 118;
        auto wrong = *plan;
        wrong.physicalParameters[0].alignment = 4;
        if (abi_plan::isRegCallWin64FloatRecordsReturningBool(wrong, wrong.symbol)) return 119;
        wrong = *plan;
        wrong.returnZeroExtend = false;
        if (abi_plan::isRegCallWin64FloatRecordsReturningBool(wrong, wrong.symbol)) return 120;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 121;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 122;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "mixed_real_short") declaration = &function;
        if (!declaration) return 109;
        const auto plan = session->cFunctionAbiPlan("mixed_real_short", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningNarrowInteger(*plan, plan->symbol)) return 110;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 111;
        auto wrong = *plan;
        wrong.physicalParameters[0].alignment = 4;
        if (abi_plan::isRegCallWin64FloatRecordsReturningNarrowInteger(wrong, wrong.symbol)) return 112;
        wrong = *plan;
        wrong.physicalReturn.widthOrCount = 32;
        if (abi_plan::isRegCallWin64FloatRecordsReturningNarrowInteger(wrong, wrong.symbol)) return 113;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 114;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 115;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "mixed_real_i64") declaration = &function;
        if (!declaration) return 102;
        const auto plan = session->cFunctionAbiPlan("mixed_real_i64", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningInt64(*plan, plan->symbol)) return 103;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 104;
        auto wrong = *plan;
        wrong.physicalParameters[0].alignment = 4;
        if (abi_plan::isRegCallWin64FloatRecordsReturningInt64(wrong, wrong.symbol)) return 105;
        wrong = *plan;
        wrong.physicalReturn.widthOrCount = 32;
        if (abi_plan::isRegCallWin64FloatRecordsReturningInt64(wrong, wrong.symbol)) return 106;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 107;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 108;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "mixed_real_float") declaration = &function;
        if (!declaration) return 95;
        const auto plan = session->cFunctionAbiPlan("mixed_real_float", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningFloat(*plan, plan->symbol)) return 96;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 97;
        auto wrong = *plan;
        wrong.physicalParameters[0].alignment = 4;
        if (abi_plan::isRegCallWin64FloatRecordsReturningFloat(wrong, wrong.symbol)) return 98;
        wrong = *plan;
        wrong.physicalReturn.kind = abi_plan::TypeKind::Double;
        if (abi_plan::isRegCallWin64FloatRecordsReturningFloat(wrong, wrong.symbol)) return 99;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 100;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 101;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "mixed_real_void") declaration = &function;
        if (!declaration) return 88;
        const auto plan = session->cFunctionAbiPlan("mixed_real_void", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64IndirectRecordsReturningVoid(*plan, plan->symbol)) return 89;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 90;
        auto wrong = *plan;
        wrong.physicalParameters[0].alignment = 4;
        if (abi_plan::isRegCallWin64IndirectRecordsReturningVoid(wrong, wrong.symbol) ||
            abi_plan::regCallFloatRecordArgumentsMatch(wrong, declaration->returnType, parameters, true, true)) return 91;
        wrong = *plan;
        wrong.physicalReturn.kind = abi_plan::TypeKind::Integer;
        if (abi_plan::isRegCallWin64IndirectRecordsReturningVoid(wrong, wrong.symbol)) return 92;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 93;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 94;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "double_result_scalar") declaration = &function;
        if (!declaration) return 82;
        const auto plan = session->cFunctionAbiPlan("double_result_scalar", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64DoubleRecordInterleaved(*plan, plan->symbol, 16, 8)) return 83;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 84;
        auto wrong = *plan;
        wrong.physicalParameters[0].type.widthOrCount = 64;
        if (abi_plan::isRegCallWin64DoubleRecordInterleaved(wrong, wrong.symbol, 16, 8)) return 85;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 86;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 87;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "double_result") declaration = &function;
        if (!declaration) return 75;
        const auto plan = session->cFunctionAbiPlan("double_result", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64DoubleRecordInterleaved(*plan, plan->symbol, 16, 8)) return 76;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 77;
        auto wrong = *plan;
        wrong.physicalReturn.elements[0].kind = abi_plan::TypeKind::Float;
        if (abi_plan::isRegCallWin64DoubleRecordInterleaved(wrong, wrong.symbol, 16, 8)) return 78;
        if (abi_plan::isRegCallWin64DoubleRecordInterleaved(*plan, plan->symbol, 16, 4)) return 79;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 80;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 81;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "mixed_real_int") declaration = &function;
        if (!declaration) return 68;
        const auto plan = session->cFunctionAbiPlan("mixed_real_int", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64IndirectRecordsReturningInt(*plan, plan->symbol)) return 69;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 70;
        auto wrong = *plan;
        wrong.physicalParameters[0].alignment = 4;
        if (abi_plan::isRegCallWin64IndirectRecordsReturningInt(wrong, wrong.symbol) ||
            abi_plan::regCallFloatRecordArgumentsMatch(wrong, declaration->returnType, parameters, true, true)) return 71;
        wrong = *plan;
        wrong.physicalReturn.widthOrCount = 64;
        if (abi_plan::isRegCallWin64IndirectRecordsReturningInt(wrong, wrong.symbol)) return 72;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 73;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 74;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "mixed_real") declaration = &function;
        if (!declaration) return 62;
        const auto plan = session->cFunctionAbiPlan("mixed_real", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64IndirectRecordsReturningDouble(*plan, plan->symbol)) return 63;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 64;
        auto wrong = *plan;
        wrong.physicalParameters[0].alignment = 4;
        if (abi_plan::isRegCallWin64IndirectRecordsReturningDouble(wrong, wrong.symbol) ||
            abi_plan::regCallFloatRecordArgumentsMatch(wrong, declaration->returnType, parameters, true, true)) return 65;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 66;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 67;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "double_pair") declaration = &function;
        if (!declaration) return 55;
        const auto plan = session->cFunctionAbiPlan("double_pair", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64DoubleRecordsReturningDouble(*plan, plan->symbol) ||
            !abi_plan::isRegCallWin64FloatRecordsReturningDouble(*plan, plan->symbol)) return 56;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 57;
        auto wrong = *plan;
        wrong.logicalParameters[0].expandedOffsets[1] = 4;
        if (abi_plan::isRegCallWin64DoubleRecordsReturningDouble(wrong, wrong.symbol)) return 58;
        wrong = *plan;
        wrong.physicalParameters[1].type.kind = abi_plan::TypeKind::Float;
        if (abi_plan::isRegCallWin64DoubleRecordsReturningDouble(wrong, wrong.symbol)) return 59;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 60;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 61;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_indirect_result") declaration = &function;
        if (!declaration) return 48;
        const auto plan = session->cFunctionAbiPlan("nested_indirect_result", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningIndirectRecord(*plan, plan->symbol, 24, 4)) return 49;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 50;
        auto wrong = *plan;
        wrong.physicalParameters[0].structureReturn = false;
        if (abi_plan::isRegCallWin64FloatRecordsReturningIndirectRecord(wrong, wrong.symbol, 24, 4)) return 51;
        wrong = *plan;
        wrong.logicalParameters[0].firstPhysical = 0;
        if (abi_plan::isRegCallWin64FloatRecordsReturningIndirectRecord(wrong, wrong.symbol, 24, 4)) return 52;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 53;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 54;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_compact_result") declaration = &function;
        if (!declaration) return 41;
        const auto plan = session->cFunctionAbiPlan("nested_compact_result", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningCompactRecord(*plan, plan->symbol, 8, 4)) return 42;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 43;
        if (abi_plan::isRegCallWin64FloatRecordsReturningCompactRecord(*plan, plan->symbol, 4, 4) ||
            abi_plan::isRegCallWin64FloatRecordsReturningCompactRecord(*plan, plan->symbol, 8, 3)) return 44;
        auto wrong = *plan;
        wrong.physicalReturn.kind = abi_plan::TypeKind::Pointer;
        if (abi_plan::isRegCallWin64FloatRecordsReturningCompactRecord(wrong, wrong.symbol, 8, 4)) return 45;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 46;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 47;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_mixed_result") declaration = &function;
        if (!declaration) return 253;
        const auto plan = session->cFunctionAbiPlan("nested_mixed_result", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordInterleaved(*plan, plan->symbol, 12, 4)) return 254;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 255;
        auto wrong = parameters;
        wrong[2].recordIdentity->clangUsr = declaration->returnType.recordIdentity->clangUsr;
        if (abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, wrong, true, true)) return 256;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 257;
        auto altered = saved; altered.declaration.parameters[2].type.recordIdentity->clangUsr = declaration->returnType.recordIdentity->clangUsr;
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 258;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_three_bytes") declaration = &function;
        if (!declaration) return 248;
        const auto plan = session->cFunctionAbiPlan("nested_three_bytes", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningInt(*plan, plan->symbol) ||
            plan->logicalParameters.size() != 3) return 249;
        const auto& logical = plan->logicalParameters[2];
        if (logical.firstPhysical >= plan->physicalParameters.size() ||
            !abi_plan::regCallIndirectRecordMatches(declaration->parameters[2].type, logical,
                plan->physicalParameters[logical.firstPhysical])) return 250;
        auto wrong = plan->physicalParameters[logical.firstPhysical]; wrong.alignment = 2;
        if (abi_plan::regCallIndirectRecordMatches(declaration->parameters[2].type, logical, wrong)) return 251;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (!abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 252;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_large_mixed") declaration = &function;
        if (!declaration) return 241;
        const auto plan = session->cFunctionAbiPlan("nested_large_mixed", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningInt(*plan, plan->symbol) ||
            plan->logicalParameters.size() != 3) return 242;
        const auto& logical = plan->logicalParameters[2];
        if (logical.firstPhysical >= plan->physicalParameters.size() ||
            !abi_plan::regCallIndirectRecordMatches(declaration->parameters[2].type, logical,
                plan->physicalParameters[logical.firstPhysical])) return 243;
        auto bad = plan->physicalParameters[logical.firstPhysical]; bad.alignment = 8;
        if (abi_plan::regCallIndirectRecordMatches(declaration->parameters[2].type, logical, bad)) return 244;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true) ||
            !abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true, true)) return 245;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 246;
        auto altered = saved; altered.declaration.parameters[2].type.alignmentBits = 64;
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 247;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_compact_short") declaration = &function;
        if (!declaration) return 236;
        const auto plan = session->cFunctionAbiPlan("nested_compact_short", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningInt(*plan, plan->symbol) ||
            plan->logicalParameters.size() != 3) return 237;
        const auto& logical = plan->logicalParameters[2];
        if (logical.firstPhysical >= plan->physicalParameters.size() ||
            !abi_plan::regCallDirectIntegerRecordMatches(declaration->parameters[2].type, logical,
                plan->physicalParameters[logical.firstPhysical])) return 238;
        auto wrong = declaration->parameters[2].type; wrong.alignmentBits = 64;
        if (abi_plan::regCallDirectIntegerRecordMatches(wrong, logical, plan->physicalParameters[logical.firstPhysical])) return 239;
        auto badPhysical = plan->physicalParameters[logical.firstPhysical]; badPhysical.type.widthOrCount = 64;
        if (abi_plan::regCallDirectIntegerRecordMatches(declaration->parameters[2].type, logical, badPhysical)) return 240;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_mixed_scalar") declaration = &function;
        if (!declaration) return 227;
        const auto plan = session->cFunctionAbiPlan("nested_mixed_scalar", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningInt(*plan, plan->symbol) ||
            plan->logicalParameters.size() != 3) return 228;
        const auto& logical = plan->logicalParameters[2];
        if (logical.firstPhysical >= plan->physicalParameters.size() ||
            !abi_plan::regCallDirectIntegerRecordMatches(declaration->parameters[2].type, logical,
                plan->physicalParameters[logical.firstPhysical])) return 229;
        auto wrong = declaration->parameters[2].type; wrong.sizeBits = 32;
        if (abi_plan::regCallDirectIntegerRecordMatches(wrong, logical,
                plan->physicalParameters[logical.firstPhysical])) return 230;
        auto badPhysical = plan->physicalParameters[logical.firstPhysical]; badPhysical.type.widthOrCount = 32;
        if (abi_plan::regCallDirectIntegerRecordMatches(declaration->parameters[2].type, logical, badPhysical)) return 231;
        std::vector<CppType> parameters;
        for (const auto& parameter : declaration->parameters) parameters.push_back(parameter.type);
        if (abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters) ||
            !abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true)) return 232;
        parameters[2].alignmentBits = 128;
        if (abi_plan::regCallFloatRecordArgumentsMatch(*plan, declaration->returnType, parameters, true)) return 233;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 234;
        auto altered = saved; altered.declaration.parameters[2].type.alignmentBits = 128;
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 235;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_return_uint") declaration = &function;
        if (!declaration) return 223;
        const auto plan = session->cFunctionAbiPlan("nested_return_uint", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningInt(*plan, plan->symbol) ||
            !abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, declaration->returnType)) return 224;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 225;
        auto altered = saved; altered.declaration.returnType.kind = CppTypeKind::SignedInteger;
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 226;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_return_bool") declaration = &function;
        if (!declaration) return 217;
        const auto plan = session->cFunctionAbiPlan("nested_return_bool", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningBool(*plan, plan->symbol) ||
            !abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, declaration->returnType)) return 218;
        auto wrong = *plan; wrong.returnZeroExtend = false;
        if (abi_plan::isRegCallWin64FloatRecordsReturningBool(wrong, wrong.symbol)) return 219;
        wrong = *plan; wrong.logicalReturn.zeroExtend = false;
        if (abi_plan::isRegCallWin64FloatRecordsReturningBool(wrong, wrong.symbol)) return 220;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 221;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 222;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_return_narrow") declaration = &function;
        if (!declaration) return 211;
        const auto plan = session->cFunctionAbiPlan("nested_return_narrow", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningNarrowInteger(*plan, plan->symbol) ||
            !abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, declaration->returnType)) return 212;
        auto wrong = *plan; wrong.physicalReturn.widthOrCount = 32;
        if (abi_plan::isRegCallWin64FloatRecordsReturningNarrowInteger(wrong, wrong.symbol)) return 213;
        wrong = *plan; wrong.returnZeroExtend = true;
        if (abi_plan::isRegCallWin64FloatRecordsReturningNarrowInteger(wrong, wrong.symbol)) return 214;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 215;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 216;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_return_i64") declaration = &function;
        if (!declaration) return 200;
        const auto plan = session->cFunctionAbiPlan("nested_return_i64", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningInt64(*plan, plan->symbol) ||
            !abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, declaration->returnType)) return 201;
        auto wrong = *plan; wrong.physicalReturn.widthOrCount = 32;
        if (abi_plan::isRegCallWin64FloatRecordsReturningInt64(wrong, wrong.symbol)) return 202;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 203;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 204;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_return_float") declaration = &function;
        if (!declaration) return 195;
        const auto plan = session->cFunctionAbiPlan("nested_return_float", declaration->canonicalFunctionType);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningFloat(*plan, plan->symbol) ||
            !abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, declaration->returnType)) return 196;
        auto wrong = *plan; wrong.physicalReturn.kind = abi_plan::TypeKind::Double;
        if (abi_plan::isRegCallWin64FloatRecordsReturningFloat(wrong, wrong.symbol)) return 197;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 198;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 199;
    }
    {
        const CppFunctionDeclaration* declaration = nullptr;
        for (const auto& function : *functions)
            if (function.name == "nested_return_void") declaration = &function;
        if (!declaration) return 187;
        const auto plan = session->cFunctionAbiPlan("nested_return_void", declaration->canonicalFunctionType);
        if (!plan) return 188;
        if (!abi_plan::isRegCallWin64FloatRecordsReturningVoid(*plan, plan->symbol)) return 189;
        auto wrong = *plan; wrong.physicalReturn.kind = abi_plan::TypeKind::Integer;
        if (abi_plan::isRegCallWin64FloatRecordsReturningVoid(wrong, wrong.symbol)) return 190;
        wrong = *plan; wrong.logicalReturn.passing = abi_plan::Passing::Direct;
        if (abi_plan::isRegCallWin64FloatRecordsReturningVoid(wrong, wrong.symbol)) return 191;
        metadata_detail::CDependencyFunctionSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.linkSymbol = plan->symbol; saved.declaration = *declaration;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyFunctionSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyFunction(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyFunction(encoded, restored, error)) return 192;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 193;
        altered = saved; altered.declaration.returnType.isConst = true;
        if (metadata_detail::encodeCDependencyFunction(altered, encoded, error)) return 194;
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
        using MixedCallback = Pair (__attribute__((regcall)) *)(Pair, int);
        using TwoCallback = Pair (__attribute__((regcall)) *)(Pair, Pair, int);
        struct Triple { float x,y,z; };
        using VariedCallback = Pair (__attribute__((regcall)) *)(Pair, Triple, int);
        using ReversedCallback = Triple (__attribute__((regcall)) *)(Pair, Triple, int);
        using InterleavedCallback = Pair (__attribute__((regcall)) *)(Pair, int, Triple);
        using LeadingCallback = Pair (__attribute__((regcall)) *)(int, Pair, Triple);
        using ReturnIntCallback = int (__attribute__((regcall)) *)(Pair, int, Triple);
        using ReturnDoubleCallback = double (__attribute__((regcall)) *)(Pair, int, Triple);
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
    {
        CppSessionOptions onlyOptions;
        onlyOptions.language = CppLanguageMode::Cpp;
        onlyOptions.languageStandard = "c++20";
        onlyOptions.targetTriple = "x86_64-pc-windows-msvc";
        onlyOptions.driverReferencePath = "clang.exe";
        onlyOptions.resourceDirectory = argv[1];
        onlyOptions.workingDirectory = argv[2];
        onlyOptions.input = CppBufferInput{"preserve-none-only.cpp", R"cpp(
            namespace NoMix {
                struct Huge { int data[11]; };
                Huge __attribute__((preserve_none)) only(Huge);
            }
        )cpp"};
        auto opened = factory.open(std::move(onlyOptions), {});
        if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(opened)) return 339;
        auto only = std::move(std::get<std::unique_ptr<CppInteropSession>>(opened));
        const auto functions = only->freeFunctionsSnapshot();
        if (!functions) return 340;
        const auto function = std::find_if(functions->begin(), functions->end(),
            [](const auto& value) { return value.qualifiedName == "NoMix::only"; });
        if (function == functions->end() || !function->externalSymbol || !function->abiPlan ||
            !abi_plan::isPreserveNoneWin64RecordRoundTrip(*function->abiPlan, *function->externalSymbol, 44, 4)) return 341;
    }
    {
        CppSessionOptions sysvOptions;
        sysvOptions.language = CppLanguageMode::Cpp;
        sysvOptions.languageStandard = "c++20";
        sysvOptions.targetTriple = "x86_64-pc-windows-msvc";
        sysvOptions.driverReferencePath = "clang.exe";
        sysvOptions.resourceDirectory = argv[1];
        sysvOptions.workingDirectory = argv[2];
        sysvOptions.input = CppBufferInput{"sysv-cpp-record-plan.cpp", R"cpp(
            namespace S {
                struct Small { int x, y; };
                struct One { int x; };
                struct PairFloat { float x, y; };
                struct TripleFloat { float x, y, z; };
                struct PairDouble { double x, y; };
                struct DoubleInt { double x; int y; };
                struct IntDouble { int x; double y; };
                struct FloatFloatInt { float x, y; int z; };
                struct OtherFloatFloatInt { float x, y; int z; };
                struct IntIntFloat { int x, y; float z; };
                using FloatFloatIntFromIntIntFloat = FloatFloatInt (__attribute__((sysv_abi)) *)(IntIntFloat);
                using FloatFloatIntFromDoubleInt = FloatFloatInt (__attribute__((sysv_abi)) *)(DoubleInt);
                using FloatFloatIntRoundTrip = FloatFloatInt (__attribute__((sysv_abi)) *)(FloatFloatInt);
                using FloatFloatIntFactory = FloatFloatInt (__attribute__((sysv_abi)) *)();
                using FloatFloatIntScalarFactory = FloatFloatInt (__attribute__((sysv_abi)) *)(int, float);
                using FloatFloatIntFromPairFloat = FloatFloatInt (__attribute__((sysv_abi)) *)(PairFloat);
                using FloatFloatIntFromSmall = FloatFloatInt (__attribute__((sysv_abi)) *)(Small);
                using FloatFloatIntWithScalars = FloatFloatInt (__attribute__((sysv_abi)) *)(FloatFloatInt, int, float);
                using FloatFloatIntTwoRecords = FloatFloatInt (__attribute__((sysv_abi)) *)(FloatFloatInt, int, FloatFloatInt);
                using FloatFloatIntDistinct = FloatFloatInt (__attribute__((sysv_abi)) *)(FloatFloatInt, int, OtherFloatFloatInt);
                using FloatFloatIntFromOther = FloatFloatInt (__attribute__((sysv_abi)) *)(OtherFloatFloatInt);
                using FloatFloatIntSixRecords = FloatFloatInt (__attribute__((sysv_abi)) *)(FloatFloatInt, int, FloatFloatInt, FloatFloatInt, FloatFloatInt, FloatFloatInt, FloatFloatInt);
                using TripleFloatRoundTrip = TripleFloat (__attribute__((sysv_abi)) *)(TripleFloat);
                using PairDoubleRoundTrip = PairDouble (__attribute__((sysv_abi)) *)(PairDouble);
                using DoubleIntRoundTrip = DoubleInt (__attribute__((sysv_abi)) *)(DoubleInt);
                using IntDoubleRoundTrip = IntDouble (__attribute__((sysv_abi)) *)(IntDouble);
                using IntIntFloatRoundTrip = IntIntFloat (__attribute__((sysv_abi)) *)(IntIntFloat);
                using Callback = Small (__attribute__((sysv_abi)) *)(Small);
                struct Triple { int x, y, z; };
                using FloatFloatIntFromTriple = FloatFloatInt (__attribute__((sysv_abi)) *)(Triple);
                using TripleCallback = Triple (__attribute__((sysv_abi)) *)(Triple);
                struct Quad { int x, y, z, w; };
                using QuadCallback = Quad (__attribute__((sysv_abi)) *)(Quad);
                struct Five { int x, y, z, w, v; };
                using FloatFloatIntFromFive = FloatFloatInt (__attribute__((sysv_abi)) *)(Five);
                using FiveCallback = Five (__attribute__((sysv_abi)) *)(Five);
                struct Six { int x, y, z, w, v, u; };
                using SixCallback = Six (__attribute__((sysv_abi)) *)(Six);
                using SixIntCallback = Six (__attribute__((sysv_abi)) *)(Six, int);
                using SixThreeIntCallback = Six (__attribute__((sysv_abi)) *)(Six, int, int, int);
                using SixMiddleCallback = Six (__attribute__((sysv_abi)) *)(int, Six, int);
                using SixLastCallback = Six (__attribute__((sysv_abi)) *)(int, int, Six);
                using SixTwoCallback = Six (__attribute__((sysv_abi)) *)(Six, int, Six);
                using SixThreeCallback = Six (__attribute__((sysv_abi)) *)(Six, Six, Six);
                using SixFloatingCallback = Six (__attribute__((sysv_abi)) *)(Six, float, Six, double, int);
                using SixFiveCallback = Six (__attribute__((sysv_abi)) *)(Six, int, Five);
                struct Wide { long long x, y, z; };
                using SixWideCallback = Six (__attribute__((sysv_abi)) *)(Six, int, Wide);
                using WideSixCallback = Wide (__attribute__((sysv_abi)) *)(Wide, int, Six);
                struct Real { double x, y, z; };
                using FloatFloatIntFromReal = FloatFloatInt (__attribute__((sysv_abi)) *)(Real);
                struct MixedFive { float x; int y; float z; int w,v; };
                using FloatFloatIntFromMixedFive = FloatFloatInt (__attribute__((sysv_abi)) *)(MixedFive);
                struct Padded { int x; double y; int z; };
                using FloatFloatIntFromPadded = FloatFloatInt (__attribute__((sysv_abi)) *)(Padded);
                struct PaddedNarrow { signed char x; double y; unsigned short z; };
                using FloatFloatIntFromPaddedNarrow = FloatFloatInt (__attribute__((sysv_abi)) *)(PaddedNarrow);
                struct PointerField { int x; double y; int* z; };
                using FloatFloatIntFromPointerField = FloatFloatInt (__attribute__((sysv_abi)) *)(PointerField);
                struct NestedInner { int a, b; };
                struct NestedOuter { NestedInner x; double y; int z; };
                using FloatFloatIntFromNested = FloatFloatInt (__attribute__((sysv_abi)) *)(NestedOuter);
                using PreserveNoneSmall = NestedInner (__attribute__((preserve_none)) *)(NestedInner);
                struct PreserveNoneLarge { int a,b,c,d,e,f,g,h,i,j,k; };
                using PreserveNoneLargePtr = PreserveNoneLarge (__attribute__((preserve_none)) *)(PreserveNoneLarge);
                PreserveNoneLarge __attribute__((preserve_none)) preserveNoneLarge(PreserveNoneLarge);
                struct ArrayField { int x[3]; double y; };
                using FloatFloatIntFromArrayField = FloatFloatInt (__attribute__((sysv_abi)) *)(ArrayField);
                struct BoolField { bool x; double y; int z; };
                using FloatFloatIntFromBoolField = FloatFloatInt (__attribute__((sysv_abi)) *)(BoolField);
                struct BoolArrayField { bool x[3]; double y; int z; };
                using FloatFloatIntFromBoolArrayField = FloatFloatInt (__attribute__((sysv_abi)) *)(BoolArrayField);
                using SixRealCallback = Six (__attribute__((sysv_abi)) *)(Six, int, Real);
                using RealSixCallback = Real (__attribute__((sysv_abi)) *)(Real, int, Six);
                using SixScalarCallback = Six (__attribute__((sysv_abi)) *)(Six, unsigned int, long long, unsigned long long, int*);
                using SixNarrowCallback = Six (__attribute__((sysv_abi)) *)(Six, signed char, unsigned char, short, unsigned short, bool);
                using IntRecordCallback = int (__attribute__((sysv_abi)) *)(Six, int, Five);
                using OneIntegerResultCallback = int (__attribute__((sysv_abi)) *)(One, int, Five);
                using TwoIntegerResultCallback = int (__attribute__((sysv_abi)) *)(Small, int, Five);
                using PairFloatResultCallback = int (__attribute__((sysv_abi)) *)(PairFloat, int, Five);
                using TripleFloatResultCallback = int (__attribute__((sysv_abi)) *)(TripleFloat, int, Five);
                using PairDoubleResultCallback = int (__attribute__((sysv_abi)) *)(PairDouble, int, Five);
                using DoubleIntResultCallback = int (__attribute__((sysv_abi)) *)(DoubleInt, int, Five);
                using IntDoubleResultCallback = int (__attribute__((sysv_abi)) *)(IntDouble, int, Five);
                using FloatFloatIntResultCallback = int (__attribute__((sysv_abi)) *)(FloatFloatInt, int, Five);
                using IntIntFloatResultCallback = int (__attribute__((sysv_abi)) *)(IntIntFloat, int, Five);
                using SmallIntegerResultCallback = int (__attribute__((sysv_abi)) *)(Triple, int, Five);
                using UintRecordCallback = unsigned int (__attribute__((sysv_abi)) *)(Six, int, Five);
                using LongRecordCallback = long long (__attribute__((sysv_abi)) *)(Six, int, Five);
                using UlongRecordCallback = unsigned long long (__attribute__((sysv_abi)) *)(Six, int, Five);
                using FloatRecordCallback = float (__attribute__((sysv_abi)) *)(Six, int, Five);
                using DoubleRecordCallback = double (__attribute__((sysv_abi)) *)(Six, int, Five);
                using SbyteRecordCallback = signed char (__attribute__((sysv_abi)) *)(Six, int, Five);
                using ByteRecordCallback = unsigned char (__attribute__((sysv_abi)) *)(Six, int, Five);
                using ShortRecordCallback = short (__attribute__((sysv_abi)) *)(Six, int, Five);
                using UshortRecordCallback = unsigned short (__attribute__((sysv_abi)) *)(Six, int, Five);
                using BoolRecordCallback = bool (__attribute__((sysv_abi)) *)(Six, int, Five);
                using VoidRecordCallback = void (__attribute__((sysv_abi)) *)(Six, int*, Five);
                using PointerRecordCallback = int* (__attribute__((sysv_abi)) *)(Six, int, Five);
                using OtherPointerRecordCallback = double* (__attribute__((sysv_abi)) *)(Six, short*, Five);
                using FloatPointerRecordCallback = float* (__attribute__((sysv_abi)) *)(Six, unsigned char*, Five);
                using NestedPointerRecordCallback = int** (__attribute__((sysv_abi)) *)(Six, int**, Five);
                using VoidPointerRecordCallback = void* (__attribute__((sysv_abi)) *)(Six, void*, Five);
                using SameRecordPointerCallback = int (__attribute__((sysv_abi)) *)(Six, Six*, Five);
                using RecordPointerResultCallback = Six* (__attribute__((sysv_abi)) *)(Six, Six*, Five);
                struct OtherPointerTarget { int value; };
                using DistinctRecordPointerCallback = int (__attribute__((sysv_abi)) *)(Six, OtherPointerTarget*, Five);
                using NestedRecordPointerCallback = int (__attribute__((sysv_abi)) *)(Six, OtherPointerTarget**, Five);
                using RecordFloatPointerCallback = Six (__attribute__((sysv_abi)) *)(Six, float*);
                using MixedScalarRecordCallback = int (__attribute__((sysv_abi)) *)(Six, signed char, unsigned char, short, unsigned short, bool, unsigned int, long long, unsigned long long, float, double, Five);
                struct Eight { int x, y, z, w, v, u, t, q; };
                using EightCallback = Eight (__attribute__((sysv_abi)) *)(Eight);
                Small __attribute__((sysv_abi)) transform(Small);
                int __attribute__((sysv_abi)) transform(int);
                struct Incomplete;
                Incomplete __attribute__((sysv_abi)) unavailable(Incomplete);
            }
            namespace T {
                struct Small { int x, y; };
                Small __attribute__((sysv_abi)) transform(Small);
            }
        )cpp"};
        auto opened = factory.open(std::move(sysvOptions), {});
        if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(opened)) return 164;
        auto observed = std::move(std::get<std::unique_ptr<CppInteropSession>>(opened));
        const auto functions = observed->freeFunctionsSnapshot();
        const auto aliases = observed->typeAliasesSnapshot();
        if (!functions || !aliases) return 165;
        const auto directPreserveNone = std::find_if(functions->begin(), functions->end(),
            [](const auto& value) { return value.qualifiedName == "S::preserveNoneLarge"; });
        if (directPreserveNone == functions->end() || !directPreserveNone->externalSymbol || !directPreserveNone->abiPlan) return 335;
        metadata_detail::CDependencyFunctionSnapshot directSaved;
        directSaved.moduleName = "PlanProbe";
        directSaved.referenceIdentity = "reference:0";
        directSaved.linkSymbol = *directPreserveNone->externalSymbol;
        directSaved.declaration = *directPreserveNone;
        metadata_detail::CDependencyFunctionSnapshot directRestored;
        std::vector<std::uint8_t> directBytes;
        std::string directError;
        if (!metadata_detail::encodeCDependencyFunction(directSaved, directBytes, directError) ||
            !metadata_detail::decodeCDependencyFunction(directBytes, directRestored, directError) ||
            !directRestored.declaration.abiPlan ||
            !abi_plan::isPreserveNoneWin64RecordRoundTrip(*directRestored.declaration.abiPlan,
                directSaved.linkSymbol, 44, 4)) return 336;
        auto directWrong = directSaved;
        auto directWrongPlan = *directSaved.declaration.abiPlan;
        directWrongPlan.callingConvention = 92;
        directWrong.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(directWrongPlan);
        if (metadata_detail::encodeCDependencyFunction(directWrong, directBytes, directError)) return 337;
        directWrong = directSaved;
        directWrong.declaration.parameters[0].type.recordIdentity->clangUsr = "other-record";
        if (metadata_detail::encodeCDependencyFunction(directWrong, directBytes, directError)) return 338;
        unsigned records = 0, scalars = 0, incomplete = 0;
        std::string first, second;
        for (const auto& function : *functions)
        {
            if (function.name == "unavailable") { if (function.abiPlan) return 166; ++incomplete; continue; }
            if (function.name != "transform") continue;
            if (function.returnType.kind != CppTypeKind::Record)
            { if (function.abiPlan) return 167; ++scalars; continue; }
            if (!function.externalSymbol || !function.abiPlan || function.abiPlan->symbol != *function.externalSymbol ||
                function.abiPlan->callingConvention != 78 || function.abiPlan->logicalParameters.size() != 1 ||
                function.abiPlan->physicalReturn.kind != abi_plan::TypeKind::Integer ||
                function.abiPlan->physicalReturn.widthOrCount != 64) return 168;
            (function.qualifiedName == "S::transform" ? first : second) = *function.externalSymbol;
            ++records;
        }
        if (records != 2 || scalars != 1 || incomplete != 1 || first.empty() || second.empty() || first == second) return 169;
        const auto alias = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::Callback"; });
        if (alias == aliases->end() || !alias->clangUsr) return 170;
        const auto plan = observed->cppFunctionPointerAbiPlan(alias->qualifiedName, *alias->clangUsr,
            alias->underlyingType.canonicalSpelling);
        if (!plan || plan->symbol != "__nsc_pointer_abi_probe" || plan->callingConvention != 78 ||
            plan->logicalParameters.size() != 1 || plan->physicalReturn.kind != abi_plan::TypeKind::Integer ||
            plan->physicalReturn.widthOrCount != 64 ||
            observed->cppFunctionPointerAbiPlan(alias->qualifiedName, "wrong-usr", alias->underlyingType.canonicalSpelling) ||
            observed->cppFunctionPointerAbiPlan(alias->qualifiedName, *alias->clangUsr, "wrong-type")) return 171;
        if (!alias->underlyingType.element || !alias->underlyingType.element->function ||
            !abi_plan::isSysVCppRecordPointerRoundTrip(*plan, *alias->underlyingType.element->function)) return 172;
        metadata_detail::CDependencyAliasSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.declaration = *alias; saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyAliasSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 173;
        auto wrong = *plan;
        wrong.physicalReturn.widthOrCount = 32;
        auto altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 174;
        auto wrongSignature = *alias->underlyingType.element->function;
        if (!wrongSignature.parameterTypes[0].recordIdentity) return 175;
        wrongSignature.parameterTypes[0].recordIdentity->clangUsr = "wrong-usr";
        if (abi_plan::isSysVCppRecordPointerRoundTrip(*plan, wrongSignature)) return 176;
        const auto triple = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::TripleCallback"; });
        if (triple == aliases->end() || !triple->clangUsr || !triple->underlyingType.element ||
            !triple->underlyingType.element->function) return 177;
        const auto triplePlan = observed->cppFunctionPointerAbiPlan(triple->qualifiedName, *triple->clangUsr,
            triple->underlyingType.canonicalSpelling);
        if (!triplePlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*triplePlan,
                *triple->underlyingType.element->function)) return 178;
        saved.declaration = *triple;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*triplePlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 179;
        wrong = *triplePlan;
        wrong.physicalParameters[1].type.widthOrCount = 64;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 180;
        const auto quad = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::QuadCallback"; });
        if (quad == aliases->end() || !quad->clangUsr || !quad->underlyingType.element ||
            !quad->underlyingType.element->function) return 181;
        const auto quadPlan = observed->cppFunctionPointerAbiPlan(quad->qualifiedName, *quad->clangUsr,
            quad->underlyingType.canonicalSpelling);
        if (!quadPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*quadPlan,
                *quad->underlyingType.element->function)) return 182;
        saved.declaration = *quad;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*quadPlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 183;
        wrong = *quadPlan;
        wrong.physicalParameters[1].type.widthOrCount = 32;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 184;
        const auto five = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::FiveCallback"; });
        if (five == aliases->end() || !five->clangUsr || !five->underlyingType.element ||
            !five->underlyingType.element->function) return 185;
        const auto fivePlan = observed->cppFunctionPointerAbiPlan(five->qualifiedName, *five->clangUsr,
            five->underlyingType.canonicalSpelling);
        if (!fivePlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*fivePlan,
                *five->underlyingType.element->function)) return 186;
        saved.declaration = *five;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*fivePlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 187;
        wrong = *fivePlan;
        wrong.physicalParameters[1].alignment = 4;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 188;
        wrong = *fivePlan;
        wrong.physicalParameters[1].pointee->elements[4].widthOrCount = 16;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 189;
        const auto six = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::SixCallback"; });
        if (six == aliases->end() || !six->clangUsr || !six->underlyingType.element ||
            !six->underlyingType.element->function) return 190;
        const auto sixPlan = observed->cppFunctionPointerAbiPlan(six->qualifiedName, *six->clangUsr,
            six->underlyingType.canonicalSpelling);
        if (!sixPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*sixPlan,
                *six->underlyingType.element->function)) return 191;
        saved.declaration = *six;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*sixPlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 192;
        wrong = *sixPlan;
        wrong.physicalParameters[1].alignment = 4;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 193;
        wrong = *sixPlan;
        wrong.physicalParameters[1].pointee->elements[5].widthOrCount = 16;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 194;
        const auto eight = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::EightCallback"; });
        if (eight == aliases->end() || !eight->clangUsr || !eight->underlyingType.element ||
            !eight->underlyingType.element->function) return 195;
        const auto eightPlan = observed->cppFunctionPointerAbiPlan(eight->qualifiedName, *eight->clangUsr,
            eight->underlyingType.canonicalSpelling);
        if (!eightPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*eightPlan,
                *eight->underlyingType.element->function)) return 196;
        saved.declaration = *eight;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*eightPlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 197;
        wrong = *eightPlan;
        wrong.physicalParameters[0].pointee->elements.pop_back();
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 198;
        wrong = *eightPlan;
        wrong.physicalParameters[1].pointee->elements[7].widthOrCount = 16;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 199;
        for (const auto* name : {"S::SixIntCallback", "S::SixThreeIntCallback"})
        {
            const auto mixed = std::find_if(aliases->begin(), aliases->end(),
                [&](const auto& value) { return value.qualifiedName == name; });
            if (mixed == aliases->end() || !mixed->clangUsr || !mixed->underlyingType.element ||
                !mixed->underlyingType.element->function) return 200;
            const auto mixedPlan = observed->cppFunctionPointerAbiPlan(mixed->qualifiedName, *mixed->clangUsr,
                mixed->underlyingType.canonicalSpelling);
            if (!mixedPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*mixedPlan,
                    *mixed->underlyingType.element->function)) return 201;
            saved.declaration = *mixed;
            saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*mixedPlan);
            if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
                !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 202;
            wrong = *mixedPlan;
            wrong.physicalParameters[2].type.widthOrCount = 64;
            altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 203;
            auto differentScalar = *mixed->underlyingType.element->function;
            differentScalar.parameterTypes[1].canonicalSpelling = "unsigned int";
            if (abi_plan::isSysVCppRecordPointerRoundTrip(*mixedPlan, differentScalar)) return 204;
            wrong = *mixedPlan; wrong.logicalParameters[0].offset = 4;
            altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 205;
        }
        for (const auto* name : {"S::SixMiddleCallback", "S::SixLastCallback"})
        {
            const auto mixed = std::find_if(aliases->begin(), aliases->end(),
                [&](const auto& value) { return value.qualifiedName == name; });
            if (mixed == aliases->end() || !mixed->clangUsr || !mixed->underlyingType.element ||
                !mixed->underlyingType.element->function) return 206;
            const auto mixedPlan = observed->cppFunctionPointerAbiPlan(mixed->qualifiedName, *mixed->clangUsr,
                mixed->underlyingType.canonicalSpelling);
            if (!mixedPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*mixedPlan,
                    *mixed->underlyingType.element->function)) return 207;
            saved.declaration = *mixed;
            saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*mixedPlan);
            if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
                !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 208;
            const auto logicalRecord = std::find_if(mixedPlan->logicalParameters.begin(),
                mixedPlan->logicalParameters.end(), [](const auto& p) { return p.passing == abi_plan::Passing::Indirect; });
            if (logicalRecord == mixedPlan->logicalParameters.end()) return 209;
            const auto index = static_cast<std::size_t>(logicalRecord - mixedPlan->logicalParameters.begin());
            wrong = *mixedPlan;
            wrong.logicalParameters[index].offset = 4;
            altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 210;
            wrong = *mixedPlan;
            wrong.physicalParameters[logicalRecord->firstPhysical].pointee->elements[0].widthOrCount = 16;
            altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 211;
            auto swapped = *mixed->underlyingType.element->function;
            std::swap(swapped.parameterTypes[0], swapped.parameterTypes[index]);
            if (abi_plan::isSysVCppRecordPointerRoundTrip(*mixedPlan, swapped)) return 212;
        }
        for (const auto* name : {"S::SixTwoCallback", "S::SixThreeCallback"})
        {
            const auto multiple = std::find_if(aliases->begin(), aliases->end(),
                [&](const auto& value) { return value.qualifiedName == name; });
            if (multiple == aliases->end() || !multiple->clangUsr || !multiple->underlyingType.element ||
                !multiple->underlyingType.element->function) return 213;
            const auto multiplePlan = observed->cppFunctionPointerAbiPlan(multiple->qualifiedName, *multiple->clangUsr,
                multiple->underlyingType.canonicalSpelling);
            if (!multiplePlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*multiplePlan,
                    *multiple->underlyingType.element->function)) return 214;
            saved.declaration = *multiple;
            saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*multiplePlan);
            if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
                !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 215;
            const auto& second = multiplePlan->logicalParameters.back();
            if (second.passing != abi_plan::Passing::Indirect) return 216;
            wrong = *multiplePlan;
            wrong.physicalParameters[second.firstPhysical].pointee->elements[5].widthOrCount = 16;
            altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 217;
            auto different = *multiple->underlyingType.element->function;
            different.parameterTypes.back().recordIdentity->clangUsr = "other-record";
            if (abi_plan::isSysVCppRecordPointerRoundTrip(*multiplePlan, different)) return 218;
            wrong = *multiplePlan;
            wrong.logicalParameters.back().offset = 4;
            altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 219;
        }
        const auto floating = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::SixFloatingCallback"; });
        if (floating == aliases->end() || !floating->clangUsr || !floating->underlyingType.element ||
            !floating->underlyingType.element->function) return 220;
        const auto floatingPlan = observed->cppFunctionPointerAbiPlan(floating->qualifiedName, *floating->clangUsr,
            floating->underlyingType.canonicalSpelling);
        if (!floatingPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*floatingPlan,
                *floating->underlyingType.element->function)) return 221;
        saved.declaration = *floating;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*floatingPlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 222;
        wrong = *floatingPlan;
        wrong.physicalParameters[wrong.logicalParameters[1].firstPhysical].type.kind = abi_plan::TypeKind::Double;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 223;
        auto differentFloating = *floating->underlyingType.element->function;
        differentFloating.parameterTypes[3].canonicalSpelling = "float";
        if (abi_plan::isSysVCppRecordPointerRoundTrip(*floatingPlan, differentFloating)) return 224;
        const auto distinct = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::SixFiveCallback"; });
        if (distinct == aliases->end() || !distinct->clangUsr || !distinct->underlyingType.element ||
            !distinct->underlyingType.element->function) return 225;
        const auto distinctPlan = observed->cppFunctionPointerAbiPlan(distinct->qualifiedName, *distinct->clangUsr,
            distinct->underlyingType.canonicalSpelling);
        if (!distinctPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*distinctPlan,
                *distinct->underlyingType.element->function)) return 226;
        saved.declaration = *distinct;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*distinctPlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 227;
        wrong = *distinctPlan;
        wrong.physicalParameters[wrong.logicalParameters[2].firstPhysical].pointee->elements.pop_back();
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 228;
        auto badDistinct = *distinct->underlyingType.element->function;
        badDistinct.parameterTypes[2].canonicalSpelling = badDistinct.returnType.canonicalSpelling;
        if (abi_plan::isSysVCppRecordPointerRoundTrip(*distinctPlan, badDistinct)) return 229;
        for (const auto* name : {"S::SixWideCallback", "S::WideSixCallback"})
        {
            const auto wide = std::find_if(aliases->begin(), aliases->end(),
                [&](const auto& value) { return value.qualifiedName == name; });
            if (wide == aliases->end() || !wide->clangUsr || !wide->underlyingType.element ||
                !wide->underlyingType.element->function) return 230;
            const auto widePlan = observed->cppFunctionPointerAbiPlan(wide->qualifiedName, *wide->clangUsr,
                wide->underlyingType.canonicalSpelling);
            if (!widePlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*widePlan,
                    *wide->underlyingType.element->function)) return 231;
            saved.declaration = *wide;
            saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*widePlan);
            if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
                !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 232;
            const auto& wideSignature = *wide->underlyingType.element->function;
            const auto wideArgument = wideSignature.parameterTypes[0].alignmentBits == 64 ? 0U : 2U;
            wrong = *widePlan;
            wrong.physicalParameters[wrong.logicalParameters[wideArgument].firstPhysical].pointee->elements[0].widthOrCount = 32;
            altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 233;
            auto wrongAlignment = wideSignature;
            wrongAlignment.parameterTypes[wideArgument].alignmentBits = 32;
            if (abi_plan::isSysVCppRecordPointerRoundTrip(*widePlan, wrongAlignment)) return 234;
        }
        for (const auto* name : {"S::SixRealCallback", "S::RealSixCallback"})
        {
            const auto real = std::find_if(aliases->begin(), aliases->end(),
                [&](const auto& value) { return value.qualifiedName == name; });
            if (real == aliases->end() || !real->clangUsr || !real->underlyingType.element ||
                !real->underlyingType.element->function) return 235;
            const auto realPlan = observed->cppFunctionPointerAbiPlan(real->qualifiedName, *real->clangUsr,
                real->underlyingType.canonicalSpelling);
            if (!realPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*realPlan,
                    *real->underlyingType.element->function)) return 236;
            saved.declaration = *real;
            saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*realPlan);
            if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
                !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 237;
            const auto& realSignature = *real->underlyingType.element->function;
            const auto realArgument = realSignature.parameterTypes[0].canonicalSpelling.find("Real") != std::string::npos ? 0U : 2U;
            const auto observedRecords = observed->recordsSnapshot();
            if (!observedRecords) return 240;
            const auto record = std::find_if(observedRecords->begin(), observedRecords->end(),
                [](const auto& value) { return value.qualifiedName == "S::Real"; });
            if (record == observedRecords->end()) return 241;
            auto realPointee = *realPlan->physicalParameters[realPlan->logicalParameters[realArgument].firstPhysical].pointee;
            if (!abi_plan::sysVFloatingRecordFieldsMatch(record->fields, realPointee)) return 242;
            for (auto& field : realPointee.elements)
            {
                field.kind = abi_plan::TypeKind::Integer;
                field.widthOrCount = 64;
            }
            if (abi_plan::sysVFloatingRecordFieldsMatch(record->fields, realPointee)) return 243;
            wrong = *realPlan;
            wrong.physicalParameters[wrong.logicalParameters[realArgument].firstPhysical].pointee->elements[0].widthOrCount = 16;
            altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 238;
            auto wrongAlignment = realSignature;
            wrongAlignment.parameterTypes[realArgument].alignmentBits = 32;
            if (abi_plan::isSysVCppRecordPointerRoundTrip(*realPlan, wrongAlignment)) return 239;
        }
        const auto scalarAlias = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::SixScalarCallback"; });
        if (scalarAlias == aliases->end() || !scalarAlias->clangUsr || !scalarAlias->underlyingType.element ||
            !scalarAlias->underlyingType.element->function) return 244;
        const auto scalarPlan = observed->cppFunctionPointerAbiPlan(scalarAlias->qualifiedName, *scalarAlias->clangUsr,
            scalarAlias->underlyingType.canonicalSpelling);
        if (!scalarPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*scalarPlan,
                *scalarAlias->underlyingType.element->function)) return 245;
        saved.declaration = *scalarAlias;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*scalarPlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 246;
        wrong = *scalarPlan;
        wrong.physicalParameters[wrong.logicalParameters[2].firstPhysical].type.widthOrCount = 32;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 247;
        auto wrongPointer = *scalarAlias->underlyingType.element->function;
        wrongPointer.parameterTypes.back().canonicalSpelling = "double *";
        if (abi_plan::isSysVCppRecordPointerRoundTrip(*scalarPlan, wrongPointer)) return 248;
        const auto narrowAlias = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::SixNarrowCallback"; });
        if (narrowAlias == aliases->end() || !narrowAlias->clangUsr || !narrowAlias->underlyingType.element ||
            !narrowAlias->underlyingType.element->function) return 249;
        const auto narrowPlan = observed->cppFunctionPointerAbiPlan(narrowAlias->qualifiedName, *narrowAlias->clangUsr,
            narrowAlias->underlyingType.canonicalSpelling);
        if (!narrowPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*narrowPlan,
                *narrowAlias->underlyingType.element->function)) return 250;
        saved.declaration = *narrowAlias;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*narrowPlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 251;
        wrong = *narrowPlan;
        const auto signedPhysical = wrong.logicalParameters[1].firstPhysical;
        wrong.physicalParameters[signedPhysical].signExtend = false;
        wrong.physicalParameters[signedPhysical].zeroExtend = true;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 252;
        auto wrongNarrow = *narrowAlias->underlyingType.element->function;
        wrongNarrow.parameterTypes[1].canonicalSpelling = "unsigned char";
        if (abi_plan::isSysVCppRecordPointerRoundTrip(*narrowPlan, wrongNarrow)) return 253;
        const auto recordPointer = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::RecordFloatPointerCallback"; });
        if (recordPointer == aliases->end() || !recordPointer->clangUsr || !recordPointer->underlyingType.element ||
            !recordPointer->underlyingType.element->function) return 236;
        const auto recordPointerPlan = observed->cppFunctionPointerAbiPlan(recordPointer->qualifiedName, *recordPointer->clangUsr,
            recordPointer->underlyingType.canonicalSpelling);
        if (!recordPointerPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*recordPointerPlan,
                *recordPointer->underlyingType.element->function)) return 237;
        saved.declaration = *recordPointer;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*recordPointerPlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 238;
        auto wrongFloatPointer = *recordPointer->underlyingType.element->function;
        auto wrongPointee = *wrongFloatPointer.parameterTypes[1].element;
        wrongPointee.isConst = true;
        wrongFloatPointer.parameterTypes[1].element = std::make_shared<const CppType>(wrongPointee);
        if (abi_plan::isSysVCppRecordPointerRoundTrip(*recordPointerPlan, wrongFloatPointer)) return 235;
        for (const auto& probe : {std::pair{"S::PreserveNoneSmall", 8U}, std::pair{"S::PreserveNoneLargePtr", 44U}})
        {
            const auto alias = std::find_if(aliases->begin(), aliases->end(),
                [&](const auto& value) { return value.qualifiedName == probe.first; });
            if (alias == aliases->end() || !alias->clangUsr) return 324;
            const auto plan = observed->cppFunctionPointerAbiPlan(alias->qualifiedName, *alias->clangUsr,
                alias->underlyingType.canonicalSpelling);
            if (!plan || !abi_plan::isPreserveNoneWin64RecordRoundTrip(*plan, plan->symbol, probe.second, 4)) return 325;
            if (!alias->underlyingType.element || !alias->underlyingType.element->function ||
                !abi_plan::isPreserveNoneCppRecordPointerRoundTrip(*plan, *alias->underlyingType.element->function)) return 330;
            saved.declaration = *alias;
            saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
            if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
                !metadata_detail::decodeCDependencyAlias(encoded, restored, error) || !restored.declaration.abiPlan ||
                !abi_plan::isPreserveNoneCppRecordPointerRoundTrip(*restored.declaration.abiPlan,
                    *restored.declaration.underlyingType.element->function)) return 331;
            auto mismatch = *plan;
            mismatch.callingConvention = 92;
            if (abi_plan::isPreserveNoneWin64RecordRoundTrip(mismatch, mismatch.symbol, probe.second, 4)) return 326;
            altered = saved;
            altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(mismatch);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 332;
            mismatch = *plan;
            mismatch.variadic = true;
            if (abi_plan::isPreserveNoneWin64RecordRoundTrip(mismatch, mismatch.symbol, probe.second, 4)) return 327;
            mismatch = *plan;
            mismatch.physicalParameters.back().byValue = true;
            if (abi_plan::isPreserveNoneWin64RecordRoundTrip(mismatch, mismatch.symbol, probe.second, 4)) return 328;
            altered = saved;
            altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(mismatch);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 333;
            auto wrongSignature = *alias->underlyingType.element->function;
            wrongSignature.parameterTypes[0].recordIdentity->clangUsr = "other-record";
            if (abi_plan::isPreserveNoneCppRecordPointerRoundTrip(*plan, wrongSignature)) return 334;
            if (probe.second == 44 && abi_plan::isPreserveNoneWin64RecordRoundTrip(*plan, plan->symbol, probe.second, 8)) return 329;
        }
        const auto mixedRoundTrip = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::FloatFloatIntRoundTrip"; });
        if (mixedRoundTrip == aliases->end() || !mixedRoundTrip->clangUsr || !mixedRoundTrip->underlyingType.element ||
            !mixedRoundTrip->underlyingType.element->function) return 235;
        const auto mixedPlan = observed->cppFunctionPointerAbiPlan(mixedRoundTrip->qualifiedName, *mixedRoundTrip->clangUsr,
            mixedRoundTrip->underlyingType.canonicalSpelling);
        if (!mixedPlan || !abi_plan::isSysVCppRecordPointerRoundTrip(*mixedPlan,
                *mixedRoundTrip->underlyingType.element->function)) return 236;
        saved.declaration = *mixedRoundTrip;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*mixedPlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 237;
        wrong = *mixedPlan;
        wrong.physicalParameters[1].type.widthOrCount = 64;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 238;
        auto wrongMixedIdentity = *mixedRoundTrip->underlyingType.element->function;
        wrongMixedIdentity.parameterTypes[0].recordIdentity->clangUsr = "wrong-usr";
        if (abi_plan::isSysVCppRecordPointerRoundTrip(*mixedPlan, wrongMixedIdentity)) return 239;
        const auto mixedRecords = observed->recordsSnapshot();
        if (!mixedRecords) return 240;
        const auto mixedRecord = std::find_if(mixedRecords->begin(), mixedRecords->end(),
            [](const auto& value) { return value.qualifiedName == "S::FloatFloatInt"; });
        if (mixedRecord == mixedRecords->end() ||
            !abi_plan::sysVDirectFloatingFieldsMatch(mixedRecord->fields, *mixedPlan->logicalParameters[0].coercion)) return 241;
        auto wrongMixedFields = mixedRecord->fields;
        wrongMixedFields[1].type.kind = CppTypeKind::SignedInteger;
        wrongMixedFields[1].type.canonicalSpelling = "int";
        if (abi_plan::sysVDirectFloatingFieldsMatch(wrongMixedFields, *mixedPlan->logicalParameters[0].coercion)) return 242;
        for (const auto* name : {"S::FloatFloatIntFactory", "S::FloatFloatIntScalarFactory", "S::FloatFloatIntFromPairFloat", "S::FloatFloatIntFromSmall", "S::FloatFloatIntFromTriple", "S::FloatFloatIntFromFive", "S::FloatFloatIntFromReal", "S::FloatFloatIntFromMixedFive", "S::FloatFloatIntFromPadded", "S::FloatFloatIntFromPaddedNarrow", "S::FloatFloatIntFromPointerField", "S::FloatFloatIntFromNested", "S::FloatFloatIntFromArrayField", "S::FloatFloatIntFromBoolField", "S::FloatFloatIntFromBoolArrayField", "S::FloatFloatIntWithScalars", "S::FloatFloatIntTwoRecords", "S::FloatFloatIntDistinct", "S::FloatFloatIntFromOther", "S::FloatFloatIntFromIntIntFloat", "S::FloatFloatIntFromDoubleInt", "S::FloatFloatIntSixRecords", "S::TripleFloatRoundTrip", "S::PairDoubleRoundTrip", "S::DoubleIntRoundTrip",
                                 "S::IntDoubleRoundTrip", "S::IntIntFloatRoundTrip"})
        {
            const auto alias = std::find_if(aliases->begin(), aliases->end(),
                [&](const auto& value) { return value.qualifiedName == name; });
            if (alias == aliases->end() || !alias->clangUsr || !alias->underlyingType.element ||
                !alias->underlyingType.element->function) return 243;
            const auto plan = observed->cppFunctionPointerAbiPlan(alias->qualifiedName, *alias->clangUsr,
                alias->underlyingType.canonicalSpelling);
            if (!plan || !abi_plan::isSysVCppRecordPointerRoundTrip(*plan, *alias->underlyingType.element->function)) return 244;
            saved.declaration = *alias;
            saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
            if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
                !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 245;
            wrong = *plan;
            wrong.physicalReturn.elements[0].kind = abi_plan::TypeKind::Pointer;
            altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 246;
            if (plan->logicalParameters.size() > 1)
            {
                wrong = *plan;
                wrong.physicalParameters[wrong.logicalParameters[1].firstPhysical].type.widthOrCount = 64;
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 247;
            }
            if (std::string_view(name) == "S::FloatFloatIntTwoRecords")
            {
                wrong = *plan;
                wrong.physicalParameters[wrong.logicalParameters[2].firstPhysical + 1].type.widthOrCount = 64;
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 248;
                auto wrongIdentity = *alias->underlyingType.element->function;
                wrongIdentity.parameterTypes[2].recordIdentity->clangUsr = "wrong-usr";
                if (abi_plan::isSysVCppRecordPointerRoundTrip(*plan, wrongIdentity)) return 249;
            }
            if (std::string_view(name) == "S::FloatFloatIntSixRecords")
            {
                if (!abi_plan::isSysVCppSpilledFloatFloatInt(*plan, 6, 96, 32)) return 250;
                wrong = *plan;
                wrong.physicalParameters[wrong.logicalParameters[6].firstPhysical].pointee->elements[1].kind = abi_plan::TypeKind::Integer;
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 251;
            }
            if (std::string_view(name) == "S::FloatFloatIntDistinct")
            {
                auto wrongIdentity = *alias->underlyingType.element->function;
                wrongIdentity.parameterTypes[2].canonicalSpelling = wrongIdentity.returnType.canonicalSpelling;
                if (abi_plan::isSysVCppRecordPointerRoundTrip(*plan, wrongIdentity)) return 252;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromIntIntFloat")
            {
                wrong = *plan;
                wrong.physicalParameters[1].type.kind = abi_plan::TypeKind::Integer;
                wrong.physicalParameters[1].type.widthOrCount = 32;
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 253;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromDoubleInt")
            {
                auto wrongLayout = *alias->underlyingType.element->function;
                wrongLayout.parameterTypes[0].alignmentBits = 32;
                if (abi_plan::isSysVCppRecordPointerRoundTrip(*plan, wrongLayout)) return 254;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromTriple")
            {
                wrong = *plan;
                wrong.physicalParameters[1].type.widthOrCount = 64;
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 255;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromFive")
            {
                wrong = *plan;
                wrong.physicalParameters[0].pointee->elements.pop_back();
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 256;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromPadded")
            {
                const auto snapshots = observed->recordsSnapshot();
                if (!snapshots) return 257;
                const auto record = std::find_if(snapshots->begin(), snapshots->end(),
                    [](const auto& value) { return value.qualifiedName == "S::Padded"; });
                if (record == snapshots->end()) return 258;
                const auto& pointee = *plan->physicalParameters[0].pointee;
                std::uint64_t size = 0, alignment = 0;
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, record->fields) || size != 192 || alignment != 64) return 259;
                auto fields = record->fields;
                fields[1].offsetBits = 32;
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 260;
                fields = record->fields;
                fields[1].type.kind = CppTypeKind::SignedInteger;
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 261;
                fields = record->fields;
                const auto underlying = fields[0].type;
                fields[0].type.kind = CppTypeKind::Enumeration;
                fields[0].type.enumIdentity = CppEnumTypeIdentity{"Mode", "Mode", false, "test:Mode"};
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(underlying);
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 288;
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 289;
                fields[0].type.enumIdentity->clangUsr.reset();
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 290;
                fields[0].type.enumIdentity->clangUsr = "test:Mode";
                fields[0].type.enumIdentity->qualifiedName = "N::Mode";
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 291;
                fields[0].type.enumIdentity->qualifiedName.clear();
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 293;
                fields[0].type.enumIdentity->qualifiedName = "Mode";
                auto wrongBase = underlying;
                wrongBase.canonicalSpelling = "unsigned int";
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(wrongBase);
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 292;
                wrongBase.kind = CppTypeKind::UnsignedInteger;
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(wrongBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 294;
                wrongBase.kind = CppTypeKind::SignedInteger;
                wrongBase.canonicalSpelling = "long";
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(wrongBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 308;
                wrongBase.kind = CppTypeKind::UnsignedInteger;
                for (const auto* spelling : {"unsigned long", "char32_t"})
                {
                    wrongBase.canonicalSpelling = spelling;
                    fields[0].type.enumUnderlying = std::make_shared<const CppType>(wrongBase);
                    if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 309;
                }
                auto widePointee = pointee;
                widePointee.elements[0].widthOrCount = 64;
                fields = record->fields;
                auto wideBase = fields[0].type;
                wideBase.canonicalSpelling = "long long";
                wideBase.sizeBits = wideBase.alignmentBits = 64;
                fields[0].type.kind = CppTypeKind::Enumeration;
                fields[0].type.sizeBits = fields[0].type.alignmentBits = 64;
                fields[0].type.enumIdentity = CppEnumTypeIdentity{"Mode", "Mode", false, "test:wide"};
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(wideBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(widePointee, size, alignment, fields, true)) return 298;
                wideBase.kind = CppTypeKind::UnsignedInteger;
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(wideBase);
                if (abi_plan::sysVNaturalScalarRecordLayout(widePointee, size, alignment, fields, true)) return 299;
                wideBase.canonicalSpelling = "unsigned long long";
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(wideBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(widePointee, size, alignment, fields, true)) return 300;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromPaddedNarrow")
            {
                const auto snapshots = observed->recordsSnapshot();
                if (!snapshots) return 262;
                const auto record = std::find_if(snapshots->begin(), snapshots->end(),
                    [](const auto& value) { return value.qualifiedName == "S::PaddedNarrow"; });
                if (record == snapshots->end()) return 263;
                const auto& pointee = *plan->physicalParameters[0].pointee;
                std::uint64_t size = 0, alignment = 0;
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, record->fields) || size != 192 || alignment != 64) return 264;
                auto fields = record->fields;
                fields[2].offsetBits = 144;
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 265;
                fields = record->fields;
                fields[0].type.kind = CppTypeKind::Floating;
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 266;
                fields = record->fields;
                auto shortBase = fields[2].type;
                shortBase.kind = CppTypeKind::SignedInteger;
                shortBase.canonicalSpelling = "short";
                fields[2].type.kind = CppTypeKind::Enumeration;
                fields[2].type.enumIdentity = CppEnumTypeIdentity{"Mode", "Mode", false, "test:short"};
                fields[2].type.enumUnderlying = std::make_shared<const CppType>(shortBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 295;
                shortBase.sizeBits = 32;
                fields[2].type.enumUnderlying = std::make_shared<const CppType>(shortBase);
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 296;
                shortBase.sizeBits = 16;
                shortBase.kind = CppTypeKind::UnsignedInteger;
                shortBase.canonicalSpelling = "unsigned short";
                fields[2].type.enumUnderlying = std::make_shared<const CppType>(shortBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 297;
                shortBase.canonicalSpelling = "wchar_t";
                fields[2].type.enumUnderlying = std::make_shared<const CppType>(shortBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 306;
                shortBase.canonicalSpelling = "char16_t";
                fields[2].type.enumUnderlying = std::make_shared<const CppType>(shortBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 310;
                shortBase.canonicalSpelling = "wchar_t";
                shortBase.kind = CppTypeKind::SignedInteger;
                fields[2].type.enumUnderlying = std::make_shared<const CppType>(shortBase);
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 307;
                fields = record->fields;
                auto charBase = fields[0].type;
                fields[0].type.kind = CppTypeKind::Enumeration;
                fields[0].type.enumIdentity = CppEnumTypeIdentity{"Mode", "Mode", false, "test:char"};
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(charBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 301;
                charBase.kind = CppTypeKind::UnsignedInteger;
                charBase.canonicalSpelling = "unsigned char";
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(charBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 302;
                charBase.canonicalSpelling = "char8_t";
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(charBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 311;
                charBase.canonicalSpelling = "signed char";
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(charBase);
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 303;
                charBase.canonicalSpelling = "char";
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(charBase);
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 304;
                charBase.kind = CppTypeKind::SignedInteger;
                fields[0].type.enumUnderlying = std::make_shared<const CppType>(charBase);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, true)) return 305;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromNested")
            {
                const auto snapshots = observed->recordsSnapshot();
                if (!snapshots) return 315;
                const auto outer = std::find_if(snapshots->begin(), snapshots->end(),
                    [](const auto& value) { return value.qualifiedName == "S::NestedOuter"; });
                const auto inner = std::find_if(snapshots->begin(), snapshots->end(),
                    [](const auto& value) { return value.qualifiedName == "S::NestedInner"; });
                if (outer == snapshots->end() || inner == snapshots->end()) return 316;
                const auto& pointee = *plan->physicalParameters[0].pointee;
                std::uint64_t size = 0, alignment = 0;
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment) || size != 192 || alignment != 64) return 317;
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, outer->fields)) return 318;
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, outer->fields, false, true)) return 319;
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee.elements[0], size, alignment, inner->fields, false, false, true, false) || size != 64 || alignment != 32) return 320;
                auto fields = outer->fields;
                fields[0].type.recordIdentity->clangUsr.reset();
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields, false, true)) return 321;
                auto innerFields = inner->fields;
                innerFields[1].offsetBits = 0;
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee.elements[0], size, alignment, innerFields, false, false, true, false)) return 322;
                auto wrongPointee = pointee;
                wrongPointee.elements[0].elements[1].widthOrCount = 64;
                if (abi_plan::sysVNaturalScalarRecordLayout(wrongPointee.elements[0], size, alignment, inner->fields, false, false, true, false)) return 323;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromPointerField")
            {
                const auto snapshots = observed->recordsSnapshot();
                if (!snapshots) return 267;
                const auto record = std::find_if(snapshots->begin(), snapshots->end(),
                    [](const auto& value) { return value.qualifiedName == "S::PointerField"; });
                if (record == snapshots->end()) return 268;
                const auto& pointee = *plan->physicalParameters[0].pointee;
                std::uint64_t size = 0, alignment = 0;
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, record->fields) || size != 192 || alignment != 64) return 269;
                auto fields = record->fields;
                fields[2].type.element.reset();
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 270;
                fields = record->fields;
                auto pointed = *fields[2].type.element;
                pointed.isConst = true;
                fields[2].type.element = std::make_shared<const CppType>(pointed);
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 271;
                pointed.canonicalSpelling = "const int";
                pointed.unqualifiedCanonicalSpelling = "int";
                fields[2].type.canonicalSpelling = "const int *";
                fields[2].type.element = std::make_shared<const CppType>(pointed);
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 312;
                if (abi_plan::isSysVCppScalarPointer(fields[2].type)) return 313;
                pointed.isVolatile = true;
                fields[2].type.element = std::make_shared<const CppType>(pointed);
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 314;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromArrayField")
            {
                const auto snapshots = observed->recordsSnapshot();
                if (!snapshots) return 272;
                const auto record = std::find_if(snapshots->begin(), snapshots->end(),
                    [](const auto& value) { return value.qualifiedName == "S::ArrayField"; });
                if (record == snapshots->end()) return 273;
                const auto& pointee = *plan->physicalParameters[0].pointee;
                std::uint64_t size = 0, alignment = 0;
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, record->fields) || size != 192 || alignment != 64) return 274;
                auto fields = record->fields;
                fields[0].type.arrayCount = 4;
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 275;
                fields = record->fields;
                auto element = *fields[0].type.element;
                element.kind = CppTypeKind::Floating;
                fields[0].type.element = std::make_shared<const CppType>(element);
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 276;
                wrong = *plan;
                auto& physicalElement = wrong.physicalParameters[0].pointee->elements[0].elements[0];
                physicalElement.kind = abi_plan::TypeKind::Pointer;
                physicalElement.widthOrCount = 0;
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 277;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromBoolField")
            {
                const auto snapshots = observed->recordsSnapshot();
                if (!snapshots) return 278;
                const auto record = std::find_if(snapshots->begin(), snapshots->end(),
                    [](const auto& value) { return value.qualifiedName == "S::BoolField"; });
                if (record == snapshots->end()) return 279;
                const auto& pointee = *plan->physicalParameters[0].pointee;
                std::uint64_t size = 0, alignment = 0;
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, record->fields) || size != 192 || alignment != 64) return 280;
                auto fields = record->fields;
                fields[0].type.sizeBits = 32;
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 281;
                fields = record->fields;
                fields[0].type.canonicalSpelling = "other";
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 282;
            }
            if (std::string_view(name) == "S::FloatFloatIntFromBoolArrayField")
            {
                const auto snapshots = observed->recordsSnapshot();
                if (!snapshots) return 283;
                const auto record = std::find_if(snapshots->begin(), snapshots->end(),
                    [](const auto& value) { return value.qualifiedName == "S::BoolArrayField"; });
                if (record == snapshots->end()) return 284;
                const auto& pointee = *plan->physicalParameters[0].pointee;
                std::uint64_t size = 0, alignment = 0;
                if (!abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, record->fields) || size != 192 || alignment != 64) return 285;
                auto fields = record->fields;
                fields[0].type.arrayCount = 4;
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 286;
                fields = record->fields;
                auto element = *fields[0].type.element;
                element.canonicalSpelling = "other";
                fields[0].type.element = std::make_shared<const CppType>(element);
                if (abi_plan::sysVNaturalScalarRecordLayout(pointee, size, alignment, fields)) return 287;
            }
        }
        const auto intAlias = std::find_if(aliases->begin(), aliases->end(),
            [](const auto& value) { return value.qualifiedName == "S::IntRecordCallback"; });
        if (intAlias == aliases->end() || !intAlias->clangUsr || !intAlias->underlyingType.element ||
            !intAlias->underlyingType.element->function) return 254;
        const auto intPlan = observed->cppFunctionPointerAbiPlan(intAlias->qualifiedName, *intAlias->clangUsr,
            intAlias->underlyingType.canonicalSpelling);
        if (!intPlan || !abi_plan::isSysVCppRecordPointerScalarReturn(*intPlan,
                *intAlias->underlyingType.element->function)) return 255;
        saved.declaration = *intAlias;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*intPlan);
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 242;
        wrong = *intPlan;
        wrong.physicalReturn.widthOrCount = 64;
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 243;
        wrong = *intPlan;
        wrong.physicalParameters[wrong.logicalParameters[2].firstPhysical].pointee->elements.pop_back();
        altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
        if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 244;
        for (const auto* name : {"S::UintRecordCallback", "S::LongRecordCallback", "S::UlongRecordCallback",
                                 "S::FloatRecordCallback", "S::DoubleRecordCallback", "S::SbyteRecordCallback",
                                 "S::ByteRecordCallback", "S::ShortRecordCallback", "S::UshortRecordCallback",
                                 "S::BoolRecordCallback", "S::VoidRecordCallback", "S::PointerRecordCallback",
                                 "S::MixedScalarRecordCallback", "S::OtherPointerRecordCallback", "S::FloatPointerRecordCallback",
                                 "S::NestedPointerRecordCallback", "S::VoidPointerRecordCallback", "S::SameRecordPointerCallback",
                                 "S::RecordPointerResultCallback", "S::DistinctRecordPointerCallback", "S::NestedRecordPointerCallback",
                                 "S::SmallIntegerResultCallback", "S::OneIntegerResultCallback", "S::TwoIntegerResultCallback",
                                 "S::PairFloatResultCallback", "S::TripleFloatResultCallback", "S::PairDoubleResultCallback",
                                 "S::DoubleIntResultCallback", "S::IntDoubleResultCallback",
                                 "S::FloatFloatIntResultCallback", "S::IntIntFloatResultCallback"})
        {
            const auto scalarReturn = std::find_if(aliases->begin(), aliases->end(),
                [&](const auto& value) { return value.qualifiedName == name; });
            if (scalarReturn == aliases->end() || !scalarReturn->clangUsr || !scalarReturn->underlyingType.element ||
                !scalarReturn->underlyingType.element->function) return 245;
            const auto scalarReturnPlan = observed->cppFunctionPointerAbiPlan(scalarReturn->qualifiedName, *scalarReturn->clangUsr,
                scalarReturn->underlyingType.canonicalSpelling);
            if (!scalarReturnPlan || !abi_plan::isSysVCppRecordPointerScalarReturn(*scalarReturnPlan,
                    *scalarReturn->underlyingType.element->function)) return 246;
            saved.declaration = *scalarReturn;
            saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*scalarReturnPlan);
            if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
                !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 247;
            wrong = *scalarReturnPlan;
            wrong.physicalReturn.kind = scalarReturnPlan->physicalReturn.kind == abi_plan::TypeKind::Pointer ?
                abi_plan::TypeKind::Float : abi_plan::TypeKind::Pointer;
            altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
            if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 248;
            auto wrongSignature = *scalarReturn->underlyingType.element->function;
            if (wrongSignature.returnType.kind == CppTypeKind::Pointer)
            {
                auto pointee = *wrongSignature.returnType.element;
                pointee.isConst = true;
                wrongSignature.returnType.element = std::make_shared<const CppType>(pointee);
                if (abi_plan::isSysVCppRecordPointerScalarReturn(*scalarReturnPlan, wrongSignature)) return 240;
                if (pointee.kind == CppTypeKind::Record)
                {
                    auto mismatched = *scalarReturn->underlyingType.element->function;
                    auto alteredPointee = *mismatched.returnType.element;
                    alteredPointee.recordIdentity->clangUsr = "wrong-usr";
                    mismatched.returnType.element = std::make_shared<const CppType>(alteredPointee);
                    if (abi_plan::isSysVCppRecordPointerScalarReturn(*scalarReturnPlan, mismatched)) return 233;
                }
            }
            if (wrongSignature.parameterTypes[1].kind == CppTypeKind::Pointer)
            {
                auto pointee = *wrongSignature.parameterTypes[1].element;
                pointee.isConst = true;
                wrongSignature.parameterTypes[1].element = std::make_shared<const CppType>(pointee);
                if (abi_plan::isSysVCppRecordPointerScalarReturn(*scalarReturnPlan, wrongSignature)) return 241;
            }
            if (scalarReturn->underlyingType.element->function->parameterTypes[1].kind == CppTypeKind::Pointer &&
                scalarReturn->underlyingType.element->function->parameterTypes[1].element->kind == CppTypeKind::Record &&
                std::any_of(scalarReturn->underlyingType.element->function->parameterTypes.begin(),
                    scalarReturn->underlyingType.element->function->parameterTypes.end(), [&](const auto& candidate) {
                        return candidate.kind == CppTypeKind::Record && candidate.canonicalSpelling ==
                            scalarReturn->underlyingType.element->function->parameterTypes[1].element->canonicalSpelling;
                    }))
            {
                auto mismatched = *scalarReturn->underlyingType.element->function;
                auto pointee = *mismatched.parameterTypes[1].element;
                pointee.recordIdentity->clangUsr = "wrong-usr";
                mismatched.parameterTypes[1].element = std::make_shared<const CppType>(pointee);
                if (abi_plan::isSysVCppRecordPointerScalarReturn(*scalarReturnPlan, mismatched)) return 234;
            }
            if (wrongSignature.parameterTypes[1].canonicalSpelling == "signed char")
            {
                wrong = *scalarReturnPlan;
                const auto index = wrong.logicalParameters[1].firstPhysical;
                wrong.physicalParameters[index].signExtend = false;
                wrong.physicalParameters[index].zeroExtend = true;
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 239;
            }
            if (scalarReturnPlan->logicalParameters[0].passing == abi_plan::Passing::Direct &&
                scalarReturnPlan->logicalParameters[0].physicalCount == 2)
            {
                wrong = *scalarReturnPlan;
                wrong.physicalParameters[wrong.logicalParameters[0].firstPhysical + 1].type.widthOrCount = 16;
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 232;
            }
            if (scalarReturnPlan->logicalParameters[0].passing == abi_plan::Passing::Direct &&
                scalarReturnPlan->logicalParameters[0].physicalCount == 1 &&
                scalarReturn->underlyingType.element->function->parameterTypes[0].kind == CppTypeKind::Record)
            {
                wrong = *scalarReturnPlan;
                wrong.physicalParameters[wrong.logicalParameters[0].firstPhysical].type.widthOrCount = 16;
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 231;
            }
            const auto& firstLogical = scalarReturnPlan->logicalParameters[0];
            const auto& firstPhysical = scalarReturnPlan->physicalParameters[firstLogical.firstPhysical].type;
            const auto& firstRecordType = scalarReturn->underlyingType.element->function->parameterTypes[0];
            if (firstRecordType.kind == CppTypeKind::Record && firstRecordType.sizeBits && firstRecordType.alignmentBits &&
                abi_plan::isSysVCppDirectFloatingRecordArgument(*scalarReturnPlan, 0,
                    *firstRecordType.sizeBits, *firstRecordType.alignmentBits))
            {
                const auto records = observed->recordsSnapshot();
                if (!records) return 230;
                const auto& recordType = scalarReturn->underlyingType.element->function->parameterTypes[0];
                const auto pair = std::find_if(records->begin(), records->end(),
                    [&](const auto& value) { return recordType.recordIdentity &&
                        value.clangUsr == recordType.recordIdentity->clangUsr; });
                const auto& shape = firstLogical.physicalCount == 2 ? *firstLogical.coercion : firstPhysical;
                if (pair == records->end() || !abi_plan::sysVDirectFloatingFieldsMatch(pair->fields, shape)) return 229;
                auto wrongFields = pair->fields;
                wrongFields[1].offsetBits = 0;
                if (abi_plan::sysVDirectFloatingFieldsMatch(wrongFields, shape)) return 228;
                wrongFields = pair->fields;
                wrongFields[1].type.kind = CppTypeKind::Unsupported;
                if (abi_plan::sysVDirectFloatingFieldsMatch(wrongFields, shape)) return 227;
            }
            if (scalarReturnPlan->logicalReturn.passing == abi_plan::Passing::Extend)
            {
                wrong = *scalarReturnPlan;
                wrong.returnSignExtend = !wrong.returnSignExtend;
                wrong.returnZeroExtend = !wrong.returnZeroExtend;
                altered = saved; altered.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(wrong);
                if (metadata_detail::encodeCDependencyAlias(altered, encoded, error)) return 249;
            }
        }
    }
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
            !abi_plan::isRegCallWin64FloatRecordRoundTrip(*function.abiPlan, *function.externalSymbol, 8, 4)) return 39;
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
        if (!plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(
                *plan, "__regcall3____nsc_pointer_abi_probe", 8, 4) ||
            cppSession->cppFunctionPointerAbiPlan(alias.qualifiedName, "wrong-usr", alias.underlyingType.canonicalSpelling) ||
            cppSession->cppFunctionPointerAbiPlan("missing::Callback", *alias.clangUsr, alias.underlyingType.canonicalSpelling) ||
            cppSession->cppFunctionPointerAbiPlan(alias.qualifiedName, *alias.clangUsr, "wrong-type")) return 44;
        metadata_detail::CDependencyAliasSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.declaration = alias; saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        std::vector<std::uint8_t> encoded;
        std::string error;
        metadata_detail::CDependencyAliasSnapshot restored;
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error) || !restored.declaration.abiPlan ||
            !abi_plan::isRegCallWin64FloatRecordRoundTrip(*restored.declaration.abiPlan,
                "__regcall3____nsc_pointer_abi_probe", 8, 4)) return 48;
        encoded.pop_back();
        if (metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 49;
        auto wrong = *plan; wrong.logicalParameters[0].expandedOffsets[1] = 0;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(wrong));
        if (metadata_detail::encodeCDependencyAlias(saved, encoded, error)) return 50;
        ++callbackCount;
    }
    if (callbackCount != 2) return 45;
    {
        const auto found = std::find_if(cppAliases->begin(), cppAliases->end(),
            [](const auto& alias) { return alias.qualifiedName == "A::MixedCallback"; });
        if (found == cppAliases->end() || !found->clangUsr) return 64;
        const auto plan = cppSession->cppFunctionPointerAbiPlan(
            found->qualifiedName, *found->clangUsr, found->underlyingType.canonicalSpelling);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(
                *plan, "__regcall3____nsc_pointer_abi_probe", 8, 4, true)) return 65;
        metadata_detail::CDependencyAliasSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.declaration = *found; saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyAliasSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error) || !restored.declaration.abiPlan ||
            !abi_plan::isRegCallWin64FloatRecordRoundTrip(*restored.declaration.abiPlan,
                "__regcall3____nsc_pointer_abi_probe", 8, 4, true)) return 66;
        auto wrong = *plan; wrong.physicalParameters.back().type.widthOrCount = 64;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(wrong));
        if (metadata_detail::encodeCDependencyAlias(saved, encoded, error)) return 67;
    }
    {
        const auto found = std::find_if(cppAliases->begin(), cppAliases->end(),
            [](const auto& alias) { return alias.qualifiedName == "A::TwoCallback"; });
        if (found == cppAliases->end() || !found->clangUsr) return 122;
        const auto plan = cppSession->cppFunctionPointerAbiPlan(
            found->qualifiedName, *found->clangUsr, found->underlyingType.canonicalSpelling);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordRoundTrip(
                *plan, "__regcall3____nsc_pointer_abi_probe", 8, 4, 1, true, 2)) return 123;
        metadata_detail::CDependencyAliasSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.declaration = *found; saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyAliasSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error) || !restored.declaration.abiPlan ||
            !abi_plan::isRegCallWin64FloatRecordRoundTrip(*restored.declaration.abiPlan,
                "__regcall3____nsc_pointer_abi_probe", 8, 4, 1, true, 2)) return 124;
        auto wrong = *plan; wrong.logicalParameters[1].expandedOffsets[1] = 0;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(wrong));
        if (metadata_detail::encodeCDependencyAlias(saved, encoded, error)) return 125;
    }
    {
        const auto found = std::find_if(cppAliases->begin(), cppAliases->end(),
            [](const auto& alias) { return alias.qualifiedName == "A::VariedCallback"; });
        if (found == cppAliases->end() || !found->clangUsr) return 138;
        const auto plan = cppSession->cppFunctionPointerAbiPlan(
            found->qualifiedName, *found->clangUsr, found->underlyingType.canonicalSpelling);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordSequence(
                *plan, "__regcall3____nsc_pointer_abi_probe", 8, 4)) return 139;
        metadata_detail::CDependencyAliasSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.declaration = *found; saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyAliasSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 140;
        auto wrong = *plan; wrong.logicalParameters[1].expandedOffsets[2] = 4;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(wrong));
        if (metadata_detail::encodeCDependencyAlias(saved, encoded, error)) return 141;
    }
    {
        const auto found = std::find_if(cppAliases->begin(), cppAliases->end(),
            [](const auto& alias) { return alias.qualifiedName == "A::ReversedCallback"; });
        if (found == cppAliases->end() || !found->clangUsr) return 147;
        const auto plan = cppSession->cppFunctionPointerAbiPlan(
            found->qualifiedName, *found->clangUsr, found->underlyingType.canonicalSpelling);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordSequence(
                *plan, "__regcall3____nsc_pointer_abi_probe", 12, 4)) return 148;
        metadata_detail::CDependencyAliasSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.declaration = *found; saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyAliasSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 149;
        auto wrong = *plan; wrong.logicalParameters[0].expandedOffsets[1] = 0;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(wrong));
        if (metadata_detail::encodeCDependencyAlias(saved, encoded, error)) return 150;
    }
    {
        const auto found = std::find_if(cppAliases->begin(), cppAliases->end(),
            [](const auto& alias) { return alias.qualifiedName == "A::InterleavedCallback"; });
        if (found == cppAliases->end() || !found->clangUsr) return 158;
        const auto plan = cppSession->cppFunctionPointerAbiPlan(
            found->qualifiedName, *found->clangUsr, found->underlyingType.canonicalSpelling);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordInterleaved(
                *plan, "__regcall3____nsc_pointer_abi_probe", 8, 4)) return 159;
        metadata_detail::CDependencyAliasSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.declaration = *found; saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyAliasSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 160;
        auto wrong = *plan; wrong.physicalParameters[2].type.widthOrCount = 64;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(wrong));
        if (metadata_detail::encodeCDependencyAlias(saved, encoded, error)) return 161;
    }
    {
        const auto found = std::find_if(cppAliases->begin(), cppAliases->end(),
            [](const auto& alias) { return alias.qualifiedName == "A::LeadingCallback"; });
        if (found == cppAliases->end() || !found->clangUsr) return 162;
        const auto plan = cppSession->cppFunctionPointerAbiPlan(
            found->qualifiedName, *found->clangUsr, found->underlyingType.canonicalSpelling);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordInterleaved(
                *plan, "__regcall3____nsc_pointer_abi_probe", 8, 4)) return 163;
        metadata_detail::CDependencyAliasSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.declaration = *found; saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyAliasSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 164;
        auto wrong = *plan; wrong.physicalParameters[0].type.widthOrCount = 64;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(wrong));
        if (metadata_detail::encodeCDependencyAlias(saved, encoded, error)) return 165;
    }
    {
        const auto found = std::find_if(cppAliases->begin(), cppAliases->end(),
            [](const auto& alias) { return alias.qualifiedName == "A::ReturnIntCallback"; });
        if (found == cppAliases->end() || !found->clangUsr) return 173;
        const auto plan = cppSession->cppFunctionPointerAbiPlan(
            found->qualifiedName, *found->clangUsr, found->underlyingType.canonicalSpelling);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningInt(
                *plan, "__regcall3____nsc_pointer_abi_probe")) return 174;
        metadata_detail::CDependencyAliasSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.declaration = *found; saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyAliasSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 175;
        auto wrong = *plan; wrong.physicalReturn.widthOrCount = 64;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(wrong));
        if (metadata_detail::encodeCDependencyAlias(saved, encoded, error)) return 176;
    }
    {
        const auto found = std::find_if(cppAliases->begin(), cppAliases->end(),
            [](const auto& alias) { return alias.qualifiedName == "A::ReturnDoubleCallback"; });
        if (found == cppAliases->end() || !found->clangUsr) return 183;
        const auto plan = cppSession->cppFunctionPointerAbiPlan(
            found->qualifiedName, *found->clangUsr, found->underlyingType.canonicalSpelling);
        if (!plan || !abi_plan::isRegCallWin64FloatRecordsReturningDouble(
                *plan, "__regcall3____nsc_pointer_abi_probe")) return 184;
        metadata_detail::CDependencyAliasSnapshot saved;
        saved.moduleName = "PlanProbe"; saved.referenceIdentity = "reference:0";
        saved.declaration = *found; saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(*plan);
        metadata_detail::CDependencyAliasSnapshot restored;
        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!metadata_detail::encodeCDependencyAlias(saved, encoded, error) ||
            !metadata_detail::decodeCDependencyAlias(encoded, restored, error)) return 185;
        auto wrong = *plan; wrong.physicalReturn.kind = abi_plan::TypeKind::Float;
        saved.declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(wrong));
        if (metadata_detail::encodeCDependencyAlias(saved, encoded, error)) return 186;
    }
    return 0;
}
