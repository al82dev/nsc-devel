#include "nativesharp/Interop/CppAbiPlanQueries.h"
#include "nativesharp/Interop/CppCRecordAbiReplay.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/GlobalDecl.h>
#include <clang/CodeGen/CodeGenABITypes.h>
#include <clang/CodeGen/ModuleBuilder.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "../../src/Interop/Internal/ClangAbiPlan.h"
#include "../../src/Interop/Internal/CppCRecordMetadataCodec.h"
#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppInteropSession.h"
#include "nativesharp/Interop/CppVariadicAbiReconciliation.h"
#include "nativesharp/Interop/CppVariadicAbiCodec.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

namespace t09_probe = nativesharp::interop::abi_plan;

int main(int argc, char** argv) {
    if (argc != 3) return 34;
    std::error_code directoryError;
    std::filesystem::create_directories(argv[2], directoryError);
    if (directoryError) { llvm::errs() << "cannot create test work directory: " << directoryError.message() << "\n"; return 42; }
    const char* source = R"c(
        typedef struct R3 { unsigned char x, y, z; } R3;
        typedef struct R1 { unsigned char x; } R1;
        typedef struct R2 { unsigned char x, y; } R2;
        typedef struct R4 { unsigned char x, y, z, w; } R4;
        typedef struct R5 { unsigned char x, y, z, w, a; } R5;
        typedef struct R6 { unsigned char x, y, z, w, a, b; } R6;
        typedef struct R7 { unsigned char x, y, z, w, a, b, c; } R7;
        typedef struct R8 { unsigned char x, y, z, w, a, b, c, d; } R8;
        typedef struct R9 { unsigned char x[9]; } R9;
        typedef struct R10 { unsigned char x[10]; } R10;
        typedef struct R11 { unsigned char x[11]; } R11;
        typedef struct R12 { unsigned char x[12]; } R12;
        typedef struct R13 { unsigned char x[13]; } R13;
        typedef struct R14 { unsigned char x[14]; } R14;
        typedef struct R15 { unsigned char x[15]; } R15;
        typedef struct R16 { long long x, y; } R16;
        typedef struct R24 { long long x, y, z; } R24;
        typedef struct R24B { unsigned char x[24]; } R24B;
        typedef struct R24I { int x[6]; } R24I;
        typedef struct R20S { short x[10]; } R20S;
        typedef struct R20I { int x[5]; } R20I;
        typedef struct R17B { unsigned char x[17]; } R17B;
        typedef struct R20B { unsigned char x[20]; } R20B;
        typedef struct R32B { unsigned char x[32]; } R32B;
        typedef struct RF { double x; int y; } RF;
        typedef int (__attribute__((sysv_abi)) *RFMixedCallback)(RF);
        typedef struct RID { int x, y; double z; } RID;
        typedef struct RDD { double x, y; } RDD;
        typedef struct RDF { double x; float y; } RDF;
        typedef struct RFD { float x; double y; } RFD;
        typedef struct RVD { float x, y; double z; } RVD;
        typedef struct RDV { double x; float y, z; } RDV;
        typedef struct RIF { int x, y; float z; } RIF;
        typedef struct RIV { int x, y; float z, w; } RIV;
        typedef struct RVI { float z, w; int x, y; } RVI;
        typedef struct RVS { float z, w; short x, y; } RVS;
        typedef struct RVV { float x, y, z, w; } RVV;
        typedef struct __declspec(align(16)) RVV16 { float x, y, z, w; } RVV16;
        typedef struct __declspec(align(32)) RVV32 { float x, y, z, w; } RVV32;
        typedef struct __declspec(align(64)) RVV64 { float x, y, z, w; } RVV64;
        typedef struct __declspec(align(128)) RVV128 { float x, y, z, w; } RVV128;
        typedef struct __declspec(align(256)) RVV256 { float x, y, z, w; } RVV256;
        typedef struct __declspec(align(16)) RVV16I { float x, y, z, w; int a; } RVV16I;
        typedef struct RVF { float x, y, z; } RVF;
        typedef struct RV { float x, y; } RV;
        typedef struct ROneF { float x; } ROneF;
        typedef struct ROneD { double x; } ROneD;
        typedef struct __attribute__((packed)) RP { char c; double d; } RP;
        typedef float Vec4 __attribute__((vector_size(16)));
        R3 __attribute__((sysv_abi)) p3(R3);
        R4 __attribute__((sysv_abi)) p4(R4);
        R4 __attribute__((sysv_abi)) ptwo(R4, R4);
        R4 __attribute__((sysv_abi)) phetero(R4, R8);
        R4 __attribute__((sysv_abi)) pscalar(R4, int);
        R4 __attribute__((sysv_abi)) pscalarfirst(int, R4);
        R4 __attribute__((sysv_abi)) pmixed8(R4, R4, int, R4, int, R4, int, R4);
        R4 __attribute__((sysv_abi)) pscalarfloat(R4, float);
        R4 __attribute__((sysv_abi)) pscalardouble(R4, double);
        R4 __attribute__((sysv_abi)) pfloatfirst(float, R4);
        R4 __attribute__((sysv_abi)) pdoublefirst(double, R4);
        R4 __attribute__((sysv_abi)) pfloats(float, R4, float);
        R4 __attribute__((sysv_abi)) pfloatstack(R4, float, double, float, double, float, double, float, double, float);
        R4 __attribute__((sysv_abi)) precordptr(R4, int*);
        R12 __attribute__((sysv_abi)) ppairmix(R12, int, float);
        R12 __attribute__((sysv_abi)) ppairints(R12, unsigned int, long long, unsigned long long);
        R12 __attribute__((sysv_abi)) ptwopairs(R12, R12, int);
        R12 __attribute__((sysv_abi)) pdifferentpairs(R12, R16, int);
        R12 __attribute__((sysv_abi)) pscalarpairs(int, R12, R16);
        R12 __attribute__((sysv_abi)) ppairsmall(R12, R4, int);
        R24 __attribute__((sysv_abi)) pindirectmix(float, R24, int, R24);
        R20I __attribute__((sysv_abi)) pindirectdifferent(float, R24B, int, R24);
        R20I __attribute__((sysv_abi)) pdirectindirect(float, R4, int, R24);
        R20I __attribute__((sysv_abi)) ppairindirect(float, R12, int, R24);
        R20I __attribute__((sysv_abi)) pmixedindirect(float, RF, int, R24);
        R20I __attribute__((sysv_abi)) pidindirect(float, RID, int, R24);
        R20I __attribute__((sysv_abi)) pddindirect(float, RDD, int, R24);
        R20I __attribute__((sysv_abi)) pvfindirect(float, RVF, int, R24);
        R20I __attribute__((sysv_abi)) psretempty(void);
        R20I __attribute__((sysv_abi)) psretmixed(float, RF, int);
        R20I __attribute__((sysv_abi)) psretonef(float, ROneF, int);
        R20I __attribute__((sysv_abi)) psretoned(float, ROneD, int);
        R20I __attribute__((sysv_abi)) psretonev(float, RV, int);
        R4 __attribute__((sysv_abi)) peight(R4, R4, R4, R4, R4, R4, R4, R4);
        R1 __attribute__((sysv_abi)) p1(R1);
        R2 __attribute__((sysv_abi)) p2(R2);
        R8 __attribute__((sysv_abi)) p8(R8);
        R5 __attribute__((sysv_abi)) p5(R5);
        R6 __attribute__((sysv_abi)) p6(R6);
        R7 __attribute__((sysv_abi)) p7(R7);
        R9 __attribute__((sysv_abi)) p9(R9);
        R10 __attribute__((sysv_abi)) p10(R10);
        R11 __attribute__((sysv_abi)) p11(R11);
        R12 __attribute__((sysv_abi)) p12(R12);
        R13 __attribute__((sysv_abi)) p13(R13);
        R14 __attribute__((sysv_abi)) p14(R14);
        R15 __attribute__((sysv_abi)) p15(R15);
        R3 __attribute__((sysv_abi)) p3(R3);
        R3* __attribute__((sysv_abi)) p3ptr(R3*);
        R16 __attribute__((sysv_abi)) p16(R16);
        RF __attribute__((sysv_abi)) pf(RF);
        RID __attribute__((sysv_abi)) pid(RID);
        RDD __attribute__((sysv_abi)) pdd(RDD);
        RDF __attribute__((sysv_abi)) pdf(RDF);
        RFD __attribute__((sysv_abi)) pfd(RFD);
        RVD __attribute__((sysv_abi)) pvd(RVD);
        RDV __attribute__((sysv_abi)) pdv(RDV);
        RIF __attribute__((sysv_abi)) pif(RIF);
        RIV __attribute__((sysv_abi)) piv(RIV);
        RVI __attribute__((sysv_abi)) pvi(RVI);
        RVS __attribute__((sysv_abi)) pvs(RVS);
        RVV __attribute__((sysv_abi)) pvv(RVV);
        RVV16 __attribute__((sysv_abi)) pvv16(RVV16);
        RVV32 __attribute__((sysv_abi)) pvv32(RVV32);
        RVV64 __attribute__((sysv_abi)) pvv64(RVV64);
        RVV128 __attribute__((sysv_abi)) pvv128(RVV128);
        RVV256 __attribute__((sysv_abi)) pvv256(RVV256);
        RVV16I __attribute__((sysv_abi)) pvv16i(RVV16I);
        RVF __attribute__((sysv_abi)) pvf(RVF);
        RV __attribute__((sysv_abi)) ponev(RV);
        ROneF __attribute__((sysv_abi)) ponef(ROneF);
        ROneD __attribute__((sysv_abi)) poned(ROneD);
        R24 __attribute__((sysv_abi)) p24(R24);
        R24B __attribute__((sysv_abi)) p24b(R24B);
        R24I __attribute__((sysv_abi)) p24i(R24I);
        R20S __attribute__((sysv_abi)) p20s(R20S);
        R17B __attribute__((sysv_abi)) p17b(R17B);
        R20B __attribute__((sysv_abi)) p20b(R20B);
        R32B __attribute__((sysv_abi)) p32b(R32B);
        signed char __attribute__((sysv_abi)) ps8(signed char);
        Vec4 __attribute__((sysv_abi)) pv(Vec4);
        R24 __attribute__((sysv_abi)) pm(int, RF, R3, R24);
        int __attribute__((sysv_abi)) pvariadic(int, ...);
        int __attribute__((sysv_abi)) pvariadic_fixed(R24, signed char, ...);
        R24 __attribute__((sysv_abi)) pvariadic_sret(RF, signed char, ...);
        int pvariadic_cdecl(int, ...);
        RP __attribute__((sysv_abi)) pp(RP);
        int __vectorcall pvec(int);
        void __attribute__((sysv_abi)) pvoid(void);
        int __attribute__((sysv_abi)) pold();
        void* __attribute__((sysv_abi, returns_nonnull)) pnonnull(void*);
        struct Incomplete;
        int __attribute__((sysv_abi)) pincomplete(struct Incomplete);
    )c";
    auto ast = clang::tooling::buildASTFromCodeWithArgs(source,
        {"-x", "c", "-std=c11", "-target", "x86_64-pc-windows-msvc"}, "probe.c");
    if (!ast) return 1;
    auto& context = ast->getASTContext();
    auto llvmContext = std::make_unique<llvm::LLVMContext>();
    auto generator = clang::CreateLLVMCodeGen(ast->getDiagnostics(), "probe",
        ast->getVirtualFileSystemPtr(), ast->getHeaderSearchOpts(), ast->getPreprocessorOpts(),
        ast->getCodeGenOpts(), *llvmContext);
    if (!generator) return 2;
    generator->Initialize(context);
    unsigned matched = 0;
    bool unsupportedRejected = false;
    bool variadicRejected = false;
    bool noPrototypeRejected = false;
    bool attributeRejected = false;
    std::map<std::string, t09_probe::Plan> copiedPlans;
    std::optional<t09_probe::VariadicCallPlan> copiedVariadicNormal;
    std::optional<t09_probe::VariadicCallPlan> copiedVariadicPressure;
    std::optional<t09_probe::VariadicCallPlan> copiedVariadicFloatPressure;
    for (const auto* decl : context.getTranslationUnitDecl()->decls()) {
        const auto* function = llvm::dyn_cast<clang::FunctionDecl>(decl);
        if (!function || !function->getIdentifier()) continue;
        const std::string name = function->getNameAsString();
        if (name == "pvariadic_cdecl") {
            std::string error;
            if (t09_probe::copyVariadicCallPlan(*generator, *function,
                std::vector<clang::QualType>{context.IntTy}, error) ||
                error != "variadic call target or convention not represented") return 141;
            continue;
        }
        if (name == "pvariadic_fixed" || name == "pvariadic_sret") {
            std::string error;
            auto actual = t09_probe::copyVariadicCallPlan(*generator, *function,
                std::vector<clang::QualType>{function->getParamDecl(0)->getType(),
                    function->getParamDecl(1)->getType(), context.DoubleTy}, error);
            if (!actual) { llvm::errs() << "variadic fixed/return plan: " << error << "\n"; return 145; }
            if (!nativesharp::interop::validateCppVariadicSysVCallPlan(*actual, name, 2)) {
                llvm::errs() << "variadic validation failed: " << name << " result passing="
                             << static_cast<int>(actual->result.passing) << " byvalue=" << actual->result.byValue
                             << " alignment=" << actual->result.alignment << "\n";
                return 170;
            }
            const auto cached = t09_probe::encodeVariadicCallPlanForCache(*actual);
            const auto restored = cached ? t09_probe::decodeVariadicCallPlanFromCache(*cached) : std::nullopt;
            if (!restored || !nativesharp::interop::validateCppVariadicSysVCallPlan(*restored, name, 2) ||
                t09_probe::encodeVariadicCallPlanForCache(*restored) != cached) return 180;
            if (name == "pvariadic_sret") {
                auto invalid = *actual;
                invalid.physicalParameters[0].alignment = 4;
                if (nativesharp::interop::validateCppVariadicSysVCallPlan(invalid, name, 2)) return 171;
            }
            if (actual->requiredArguments != 2 || actual->arguments.size() != 3 ||
                actual->arguments[2].physicalCount != 1 ||
                actual->physicalParameters.back().type.kind != t09_probe::TypeKind::Double) return 146;
            if (name == "pvariadic_fixed") {
                if (actual->physicalParameters.size() != 3 || !actual->physicalParameters[0].byValue ||
                    actual->physicalParameters[0].alignment != 8 || !actual->physicalParameters[0].pointee ||
                    !actual->physicalParameters[1].signExtend || actual->physicalParameters[1].type.widthOrCount != 8)
                    return 147;
            } else if (actual->physicalReturn.kind != t09_probe::TypeKind::Void ||
                actual->physicalParameters.size() != 5 || !actual->physicalParameters[0].structureReturn ||
                !actual->physicalParameters[0].pointee || actual->physicalParameters[0].alignment != 8 ||
                actual->arguments[0].physicalCount != 2 || !actual->physicalParameters[3].signExtend)
                return 148;
            continue;
        }
        if (name == "pv") {
            std::string error;
            if (t09_probe::copyPlan(*generator, *function, error) || error.empty()) return 25;
            unsupportedRejected = true;
            continue;
        }
        if (name == "pvariadic") {
            std::string error;
            if (t09_probe::copyPlan(*generator, *function, error) ||
                error != "variadic call plan not represented") return 27;
            const clang::RecordDecl* mixed = nullptr;
            const clang::RecordDecl* large = nullptr;
            const clang::RecordDecl* small = nullptr;
            for (const auto* candidate : context.getTranslationUnitDecl()->decls()) {
                const auto* record = llvm::dyn_cast<clang::RecordDecl>(candidate);
                if (!record || !record->isCompleteDefinition()) continue;
                if (record->getName() == "RF") mixed = record;
                else if (record->getName() == "R24") large = record;
                else if (record->getName() == "R8") small = record;
            }
            if (!mixed || !large || !small) return 133;
            clang::CanQual<clang::FunctionProtoType> prototype =
                context.getCanonicalType(function->getType()).getAs<clang::FunctionProtoType>();
            if (!prototype || !prototype->isVariadic()) return 134;
            const auto integer = context.getCanonicalType(context.IntTy);
            const auto floating = context.getCanonicalType(context.DoubleTy);
            const auto mixedType = context.getCanonicalType(context.getTypeDeclType(static_cast<const clang::TypeDecl*>(mixed)));
            const auto largeType = context.getCanonicalType(context.getTypeDeclType(static_cast<const clang::TypeDecl*>(large)));
            const auto smallType = context.getCanonicalType(context.getTypeDeclType(static_cast<const clang::TypeDecl*>(small)));
            auto arrange = [&](const std::vector<clang::CanQualType>& arguments) -> const clang::CodeGen::CGFunctionInfo& {
                return clang::CodeGen::arrangeFreeFunctionCall(generator->CGM(), integer, arguments,
                    prototype->getExtInfo(), clang::CodeGen::RequiredArgs::forPrototype(prototype));
            };
            const auto& normal = arrange({integer, smallType, mixedType, largeType});
            if (normal.arg_size() != 4 || !normal.arguments()[1].info.isDirect() ||
                !normal.arguments()[1].info.getCoerceToType()->isIntegerTy(64) ||
                !normal.arguments()[2].info.isDirect() || !normal.arguments()[2].info.getCanBeFlattened() ||
                !normal.arguments()[3].info.isIndirect() || !normal.arguments()[3].info.getIndirectByVal()) return 135;
            const auto* mixedCoercion = llvm::dyn_cast<llvm::StructType>(normal.arguments()[2].info.getCoerceToType());
            if (!mixedCoercion || mixedCoercion->getNumElements() != 2 ||
                !mixedCoercion->getElementType(0)->isDoubleTy() ||
                !mixedCoercion->getElementType(1)->isIntegerTy(32)) return 136;
            const auto& integerPressure = arrange({integer, integer, integer, integer, integer, integer, mixedType});
            if (integerPressure.arg_size() != 7 || !integerPressure.arguments()[6].info.isIndirect() ||
                !integerPressure.arguments()[6].info.getIndirectByVal()) return 137;
            const auto& floatingPressure = arrange({integer, floating, floating, floating, floating,
                                                    floating, floating, floating, floating, mixedType});
            if (floatingPressure.arg_size() != 10 || !floatingPressure.arguments()[9].info.isIndirect() ||
                !floatingPressure.arguments()[9].info.getIndirectByVal()) return 138;
            copiedVariadicNormal = t09_probe::copyVariadicCallPlan(*generator, *function,
                std::vector<clang::QualType>{integer, smallType, mixedType, largeType}, error);
            copiedVariadicPressure = t09_probe::copyVariadicCallPlan(*generator, *function,
                std::vector<clang::QualType>{integer, integer, integer, integer, integer, integer, mixedType}, error);
            copiedVariadicFloatPressure = t09_probe::copyVariadicCallPlan(*generator, *function,
                std::vector<clang::QualType>{integer, floating, floating, floating, floating,
                    floating, floating, floating, floating, mixedType}, error);
            if (!copiedVariadicNormal || !copiedVariadicPressure || !copiedVariadicFloatPressure ||
                t09_probe::copyVariadicCallPlan(*generator, *function,
                    std::vector<clang::QualType>{integer, context.FloatTy}, error) ||
                error != "variadic argument has not undergone default promotions" ||
                t09_probe::copyVariadicCallPlan(*generator, *function,
                    std::vector<clang::QualType>{integer, context.ShortTy}, error) ||
                error != "variadic argument has not undergone default promotions" ||
                t09_probe::copyVariadicCallPlan(*generator, *function,
                    std::vector<clang::QualType>{}, error) ||
                error != "variadic prototype or required arguments unavailable" ||
                t09_probe::copyVariadicCallPlan(*generator, *function,
                    std::vector<clang::QualType>{context.FloatTy}, error) ||
                error != "fixed argument type mismatch") return 139;
            llvm::outs() << "variadic SysV: direct small/mixed, indirect large and register-pressure fallback verified\n";
            variadicRejected = true;
            continue;
        }
        if (name == "pold") {
            std::string error;
            if (t09_probe::copyPlan(*generator, *function, error) || error != "function prototype unavailable") return 31;
            noPrototypeRejected = true;
            continue;
        }
        if (name == "pnonnull") {
            std::string error;
            if (t09_probe::copyPlan(*generator, *function, error) || error != "return attribute not represented") return 33;
            attributeRejected = true;
            continue;
        }
        if (name != "p1" && name != "p2" && name != "p3" && name != "p4" && name != "p5" &&
            name != "p6" && name != "p7" && name != "p8" && name != "p9" && name != "p10" &&
            name != "p11" && name != "p12" && name != "p13" && name != "p14" && name != "p15" &&
            name != "p16" && name != "pf" && name != "p24" && name != "p24b" && name != "p24i" && name != "p17b" &&
            name != "p20b" && name != "p20s" && name != "p32b" && name != "pid" && name != "pdd" && name != "pdf" && name != "pfd" && name != "pvd" && name != "pdv" && name != "pif" && name != "piv" && name != "pvi" && name != "pvs" && name != "pvv" && name != "pvv16" && name != "pvv32" && name != "pvv64" && name != "pvv128" && name != "pvv256" && name != "pvv16i" && name != "pvf" && name != "ponev" && name != "ponef" && name != "poned" && name != "ptwo" && name != "peight" && name != "ps8" && name != "pm" &&
            name != "pp" && name != "pvec" && name != "pvoid" && name != "phetero" && name != "pscalar" && name != "pscalarfirst" && name != "pmixed8" && name != "pscalarfloat" && name != "pscalardouble" && name != "pfloatfirst" && name != "pdoublefirst" && name != "pfloats" && name != "pfloatstack" && name != "precordptr" && name != "ppairmix" && name != "ppairints" && name != "ptwopairs" && name != "pdifferentpairs" && name != "pscalarpairs" && name != "ppairsmall" && name != "pindirectmix" && name != "pindirectdifferent" && name != "pdirectindirect" && name != "ppairindirect" && name != "pmixedindirect" && name != "pidindirect" && name != "pddindirect" && name != "pvfindirect" && name != "psretempty" && name != "psretmixed" && name != "psretonef" && name != "psretoned" && name != "psretonev") continue;
        if (name == "p3" && copiedPlans.contains(name)) continue;
        auto* physical = clang::CodeGen::convertFreeFunctionType(generator->CGM(), function);
        clang::CanQual<clang::FunctionProtoType> prototype =
            context.getCanonicalType(function->getType()).getAs<clang::FunctionProtoType>();
        if (!prototype) return 12;
        const auto& plan = clang::CodeGen::arrangeFreeFunctionType(generator->CGM(), prototype);
        if (plan.arg_size() != ((name == "pm" || name == "ppairints" || name == "pindirectmix" || name == "pindirectdifferent" || name == "pdirectindirect" || name == "ppairindirect" || name == "pmixedindirect" || name == "pidindirect" || name == "pddindirect" || name == "pvfindirect") ? 4U : (name == "pvoid" || name == "psretempty") ? 0U : (name == "pfloats" || name == "ppairmix" || name == "ptwopairs" || name == "pdifferentpairs" || name == "pscalarpairs" || name == "ppairsmall" || name == "psretmixed" || name == "psretonef" || name == "psretoned" || name == "psretonev") ? 3U : name == "pfloatstack" ? 10U : (name == "ptwo" || name == "phetero" || name == "pscalar" || name == "pscalarfirst" || name == "pscalarfloat" || name == "pscalardouble" || name == "pfloatfirst" || name == "pdoublefirst" || name == "precordptr") ? 2U : (name == "peight" || name == "pmixed8") ? 8U : 1U)) return 13;
        std::string copyError;
        auto copied = t09_probe::copyPlan(*generator, *function, copyError);
        if (!copied) { llvm::errs() << copyError << "\n"; return 19; }
        copiedPlans.emplace(name, std::move(*copied));
        if (!physical || !generator->GetAddrOfGlobal(clang::GlobalDecl(function), false)) return 3;
        auto* llvmFunction = generator->GetModule()->getFunction(generator->GetMangledName(clang::GlobalDecl(function)));
        const auto expectedConvention = name == "pvec" ? llvm::CallingConv::X86_VectorCall :
            llvm::CallingConv::X86_64_SysV;
        if (!llvmFunction || llvmFunction->getFunctionType() != physical ||
            llvmFunction->getCallingConv() != expectedConvention) return 4;
        llvmFunction->print(llvm::outs());
        llvm::outs() << "\n";
        if (plan.arg_size() != 0 && plan.arguments()[0].info.isDirect()) {
            auto* coercion = plan.arguments()[0].info.getCoerceToType();
            if (!coercion) return 17;
            llvm::outs() << "argumento " << name << ": ";
            coercion->print(llvm::outs());
            llvm::outs() << ", descomponible=" << plan.arguments()[0].info.getCanBeFlattened() << "\n";
        }
        if (name == "p3" && (!physical->getReturnType()->isIntegerTy(24) || physical->getNumParams() != 1 ||
                              !physical->getParamType(0)->isIntegerTy(24))) return 5;
        if (name == "p4" && (!physical->getReturnType()->isIntegerTy(32) || physical->getNumParams() != 1 ||
                              !physical->getParamType(0)->isIntegerTy(32))) return 73;
        if ((name == "p1" || name == "p2" || name == "p8") &&
            (!physical->getReturnType()->isIntegerTy(name == "p1" ? 8 : name == "p2" ? 16 : 64) ||
             physical->getNumParams() != 1 || physical->getParamType(0) != physical->getReturnType())) return 75;
        if ((name == "p5" || name == "p6" || name == "p7") &&
            (!physical->getReturnType()->isIntegerTy(name == "p5" ? 40 : name == "p6" ? 48 : 56) ||
             physical->getNumParams() != 1 || physical->getParamType(0) != physical->getReturnType())) return 77;
        if ((name == "p3" || name == "p16" || name == "pf") &&
            (!plan.getReturnInfo().isDirect() || !plan.arguments()[0].info.isDirect())) return 14;
        if ((name == "p3" || name == "p16" || name == "pf") &&
            (plan.arguments()[0].info.getCoerceToType() != physical->getReturnType() ||
             !plan.arguments()[0].info.getCanBeFlattened() ||
             plan.arguments()[0].info.getDirectOffset() != 0)) return 18;
        if ((name == "p16" || name == "pf") && physical->getNumParams() != 2) return 6;
        if (name == "p16") {
            const auto* result = llvm::dyn_cast<llvm::StructType>(physical->getReturnType());
            if (!result || result->getNumElements() != 2 || !result->getElementType(0)->isIntegerTy(64) ||
                !result->getElementType(1)->isIntegerTy(64) || !physical->getParamType(0)->isIntegerTy(64) ||
                !physical->getParamType(1)->isIntegerTy(64)) return 10;
        }
        if (name == "pf") {
            const auto* result = llvm::dyn_cast<llvm::StructType>(physical->getReturnType());
            if (!result || result->getNumElements() != 2 || !result->getElementType(0)->isDoubleTy() ||
                !result->getElementType(1)->isIntegerTy(32) || !physical->getParamType(0)->isDoubleTy() ||
                !physical->getParamType(1)->isIntegerTy(32)) return 11;
        }
        if (name == "p24" && (physical->getNumParams() != 2 || !physical->getReturnType()->isVoidTy() ||
            !llvmFunction->hasParamAttribute(0, llvm::Attribute::StructRet) ||
            !llvmFunction->hasParamAttribute(1, llvm::Attribute::ByVal))) return 8;
        if (name == "p24" && (!plan.getReturnInfo().isIndirect() ||
            !plan.arguments()[0].info.isIndirect() || !plan.arguments()[0].info.getIndirectByVal() ||
            plan.arguments()[0].info.getIndirectAlign().getQuantity() != 8)) return 15;
        if (name == "ps8" && (!llvmFunction->hasRetAttribute(llvm::Attribute::SExt) ||
            !llvmFunction->hasParamAttribute(0, llvm::Attribute::SExt))) return 9;
        if (name == "ps8" && (!plan.getReturnInfo().isExtend() || !plan.arguments()[0].info.isExtend())) return 16;
        ++matched;
    }
    if (matched != 82 || copiedPlans.size() != 82 || !unsupportedRejected || !variadicRejected ||
        !noPrototypeRejected || !attributeRejected) return 7;
    generator.reset();
    ast.reset();
    llvmContext.reset();
    if (!copiedVariadicNormal || !copiedVariadicPressure || !copiedVariadicFloatPressure ||
        copiedVariadicNormal->symbol != "pvariadic" ||
        !copiedVariadicNormal->targetTriple.starts_with("x86_64-pc-windows-msvc") ||
        copiedVariadicNormal->dataLayout.empty() ||
        copiedVariadicNormal->callingConvention != llvm::CallingConv::X86_64_SysV ||
        copiedVariadicNormal->requiredArguments != 1 || copiedVariadicNormal->arguments.size() != 4 ||
        copiedVariadicNormal->arguments[1].firstPhysical != 1 || copiedVariadicNormal->arguments[1].physicalCount != 1 ||
        copiedVariadicNormal->arguments[2].firstPhysical != 2 || copiedVariadicNormal->arguments[2].physicalCount != 2 ||
        !copiedVariadicNormal->arguments[2].coercion ||
        copiedVariadicNormal->arguments[2].coercion->elements[0].kind != t09_probe::TypeKind::Double ||
        copiedVariadicNormal->arguments[3].firstPhysical != 4 || !copiedVariadicNormal->arguments[3].byValue ||
        copiedVariadicNormal->physicalReturn.kind != t09_probe::TypeKind::Integer ||
        copiedVariadicNormal->physicalReturn.widthOrCount != 32 || copiedVariadicNormal->physicalParameters.size() != 5 ||
        copiedVariadicNormal->physicalParameters[1].type.widthOrCount != 64 ||
        copiedVariadicNormal->physicalParameters[2].type.kind != t09_probe::TypeKind::Double ||
        copiedVariadicNormal->physicalParameters[3].type.widthOrCount != 32 ||
        !copiedVariadicNormal->physicalParameters[4].byValue || copiedVariadicNormal->physicalParameters[4].alignment != 8 ||
        !copiedVariadicNormal->physicalParameters[4].pointee ||
        copiedVariadicNormal->physicalParameters[4].pointee->kind != t09_probe::TypeKind::Structure ||
        copiedVariadicPressure->arguments[6].firstPhysical != 6 || !copiedVariadicPressure->arguments[6].byValue ||
        !copiedVariadicPressure->physicalParameters[6].byValue || copiedVariadicPressure->physicalParameters[6].alignment != 8 ||
        copiedVariadicFloatPressure->arguments[9].firstPhysical != 9 || !copiedVariadicFloatPressure->arguments[9].byValue ||
        !copiedVariadicFloatPressure->physicalParameters[9].byValue ||
        copiedVariadicFloatPressure->physicalParameters[9].alignment != 8)
        return 140;
    const auto& p3 = copiedPlans.at("p3");
    if (!t09_probe::isSysVSmallRecordRoundTrip(copiedPlans.at("p4"), "p4") ||
        t09_probe::isSysVThreeByteRoundTrip(copiedPlans.at("p4"), "p4")) return 74;
    for (const char* name : {"p1", "p2", "p5", "p6", "p7", "p8"})
        if (!t09_probe::isSysVSmallRecordRoundTrip(copiedPlans.at(name), name)) return 76;
    for (const char* name : {"p9", "p10", "p11", "p12", "p13", "p14", "p15"})
        if (!t09_probe::isSysVTwoIntegerRecordRoundTrip(copiedPlans.at(name), name)) return 80;
    if (p3.targetTriple.find("x86_64-pc-windows-msvc") != 0 || p3.dataLayout.empty()) return 28;
    if (!t09_probe::isSysVThreeByteRoundTrip(p3, "p3") ||
        t09_probe::isSysVThreeByteRoundTrip(p3, "other")) return 70;
    auto alteredPlan = p3;
    alteredPlan.physicalParameters[0].type.widthOrCount = 32;
    if (t09_probe::isSysVThreeByteRoundTrip(alteredPlan, "p3")) return 71;
    alteredPlan = p3;
    alteredPlan.targetTriple = "x86_64-unknown-linux-gnu";
    if (t09_probe::isSysVThreeByteRoundTrip(alteredPlan, "p3")) return 72;
    if (p3.physicalReturn.kind != t09_probe::TypeKind::Integer || p3.physicalReturn.widthOrCount != 24 ||
        p3.logicalParameters.size() != 1 || p3.logicalParameters[0].passing != t09_probe::Passing::Direct ||
        !p3.logicalParameters[0].coercion || p3.logicalParameters[0].coercion->widthOrCount != 24) return 20;
    const auto& p16 = copiedPlans.at("p16");
    if (!t09_probe::isSysVTwoI64RecordRoundTrip(p16, "p16") ||
        t09_probe::isSysVTwoI64RecordRoundTrip(p16, "other")) return 78;
    auto badPair = p16;
    badPair.physicalParameters[1].type.kind = t09_probe::TypeKind::Double;
    if (t09_probe::isSysVTwoI64RecordRoundTrip(badPair, "p16")) return 79;
    if (p16.physicalReturn.kind != t09_probe::TypeKind::Structure || p16.physicalReturn.elements.size() != 2 ||
        p16.physicalParameters.size() != 2 || p16.physicalParameters[0].type.widthOrCount != 64 ||
        p16.physicalParameters[1].type.widthOrCount != 64) return 21;
    const auto& pf = copiedPlans.at("pf");
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pid"), "pid") ||
        t09_probe::isSysVMixedDoubleI32RecordRoundTrip(copiedPlans.at("pid"), "pid")) return 84;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pdd"), "pdd") ||
        copiedPlans.at("pdd").physicalReturn.elements[1].kind != t09_probe::TypeKind::Double) return 93;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pdf"), "pdf") ||
        copiedPlans.at("pdf").physicalReturn.elements[1].kind != t09_probe::TypeKind::Float) return 94;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pfd"), "pfd") ||
        copiedPlans.at("pfd").physicalReturn.elements[0].kind != t09_probe::TypeKind::Float) return 95;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pvd"), "pvd") ||
        !t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pdv"), "pdv")) return 96;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pif"), "pif") ||
        copiedPlans.at("pif").physicalReturn.elements[1].kind != t09_probe::TypeKind::Float) return 85;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("piv"), "piv") ||
        copiedPlans.at("piv").physicalReturn.elements[1].kind != t09_probe::TypeKind::Vector) return 86;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pvi"), "pvi") ||
        copiedPlans.at("pvi").physicalReturn.elements[0].kind != t09_probe::TypeKind::Vector) return 87;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pvs"), "pvs") ||
        copiedPlans.at("pvs").physicalReturn.elements[1].widthOrCount != 32) return 88;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pvv"), "pvv") ||
        copiedPlans.at("pvv").physicalReturn.elements[0].kind != t09_probe::TypeKind::Vector ||
        copiedPlans.at("pvv").physicalReturn.elements[1].kind != t09_probe::TypeKind::Vector) return 89;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pvv16"), "pvv16")) return 97;
    if (!t09_probe::isSysVIndirectRecordRoundTrip(copiedPlans.at("pvv32"), "pvv32")) return 98;
    if (!t09_probe::isSysVIndirectRecordRoundTrip(copiedPlans.at("pvv16i"), "pvv16i")) return 100;
    for (const char* name : {"pvv64", "pvv128", "pvv256"})
        if (!t09_probe::isSysVIndirectRecordRoundTrip(copiedPlans.at(name), name)) return 101;
    if (!t09_probe::isSysVIndirectRecordRoundTrip(copiedPlans.at("p20s"), "p20s")) return 99;
    if (!t09_probe::isSysVMixedRecordRoundTrip(copiedPlans.at("pvf"), "pvf") ||
        copiedPlans.at("pvf").physicalReturn.elements[1].kind != t09_probe::TypeKind::Float) return 90;
    if (!t09_probe::isSysVSingleVector2FRecordRoundTrip(copiedPlans.at("ponev"), "ponev")) return 91;
    if (!t09_probe::isSysVTwoSmallRecordsRoundTrip(copiedPlans.at("ptwo"), "ptwo")) return 102;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("peight"), "peight")) return 103;
    const auto& phetero = copiedPlans.at("phetero");
    if (!t09_probe::isSysVSmallRecordsRoundTrip(phetero, "phetero") ||
        phetero.physicalReturn.widthOrCount != 32 || phetero.physicalParameters[1].type.widthOrCount != 64) return 104;
    auto badHetero = phetero;
    badHetero.physicalParameters[1].type.widthOrCount = 32;
    if (t09_probe::isSysVSmallRecordsRoundTrip(badHetero, "phetero")) return 105;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("pscalar"), "pscalar") ||
        copiedPlans.at("pscalar").physicalParameters[1].type.widthOrCount != 32) return 106;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("pscalarfirst"), "pscalarfirst") ||
        copiedPlans.at("pscalarfirst").physicalParameters[0].type.widthOrCount != 32) return 107;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("pmixed8"), "pmixed8") ||
        copiedPlans.at("pmixed8").physicalParameters.size() != 8) return 108;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("pscalarfloat"), "pscalarfloat") ||
        copiedPlans.at("pscalarfloat").physicalParameters[1].type.kind != t09_probe::TypeKind::Float) return 109;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("pscalardouble"), "pscalardouble") ||
        copiedPlans.at("pscalardouble").physicalParameters[1].type.kind != t09_probe::TypeKind::Double) return 110;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("pfloatfirst"), "pfloatfirst") ||
        copiedPlans.at("pfloatfirst").physicalParameters[0].type.kind != t09_probe::TypeKind::Float) return 111;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("pdoublefirst"), "pdoublefirst") ||
        copiedPlans.at("pdoublefirst").physicalParameters[0].type.kind != t09_probe::TypeKind::Double) return 112;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("pfloats"), "pfloats") ||
        copiedPlans.at("pfloats").physicalParameters[2].type.kind != t09_probe::TypeKind::Float) return 113;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("pfloatstack"), "pfloatstack") ||
        copiedPlans.at("pfloatstack").physicalParameters.size() != 10 ||
        copiedPlans.at("pfloatstack").physicalParameters[8].type.kind != t09_probe::TypeKind::Double) return 114;
    if (!t09_probe::isSysVSmallRecordsRoundTrip(copiedPlans.at("precordptr"), "precordptr") ||
        copiedPlans.at("precordptr").physicalParameters[1].type.kind != t09_probe::TypeKind::Pointer) return 115;
    auto badPointer = copiedPlans.at("precordptr");
    badPointer.logicalParameters[1].coercion->widthOrCount = 1;
    badPointer.physicalParameters[1].type.widthOrCount = 1;
    if (t09_probe::isSysVSmallRecordsRoundTrip(badPointer, "precordptr")) return 116;
    if (!t09_probe::isSysVTwoIntegerRecordWithScalars(copiedPlans.at("ppairmix"), "ppairmix") ||
        copiedPlans.at("ppairmix").physicalParameters.size() != 4) return 117;
    auto badPairMix = copiedPlans.at("ppairmix");
    badPairMix.logicalParameters[2].firstPhysical = 2;
    if (t09_probe::isSysVTwoIntegerRecordWithScalars(badPairMix, "ppairmix")) return 118;
    if (!t09_probe::isSysVTwoIntegerRecordWithScalars(copiedPlans.at("ppairints"), "ppairints") ||
        copiedPlans.at("ppairints").physicalParameters[3].type.widthOrCount != 64) return 119;
    if (!t09_probe::isSysVTwoIntegerRecordWithScalars(copiedPlans.at("ptwopairs"), "ptwopairs") ||
        copiedPlans.at("ptwopairs").physicalParameters.size() != 5 ||
        copiedPlans.at("ptwopairs").logicalParameters[2].firstPhysical != 4) return 120;
    if (!t09_probe::isSysVTwoIntegerRecordWithScalars(copiedPlans.at("pdifferentpairs"), "pdifferentpairs") ||
        copiedPlans.at("pdifferentpairs").physicalParameters[1].type.widthOrCount != 32 ||
        copiedPlans.at("pdifferentpairs").physicalParameters[3].type.widthOrCount != 64) return 121;
    if (!t09_probe::isSysVTwoIntegerRecordWithScalars(copiedPlans.at("pscalarpairs"), "pscalarpairs") ||
        copiedPlans.at("pscalarpairs").logicalParameters[1].firstPhysical != 1 ||
        copiedPlans.at("pscalarpairs").logicalParameters[2].firstPhysical != 3) return 122;
    if (!t09_probe::isSysVTwoIntegerRecordWithScalars(copiedPlans.at("ppairsmall"), "ppairsmall") ||
        copiedPlans.at("ppairsmall").logicalParameters[1].physicalCount != 1) return 123;
    if (!t09_probe::isSysVIndirectRecordWithArguments(copiedPlans.at("pindirectmix"), "pindirectmix") ||
        !copiedPlans.at("pindirectmix").physicalParameters[2].byValue ||
        !copiedPlans.at("pindirectmix").physicalParameters[4].byValue) return 124;
    auto badIndirectMix = copiedPlans.at("pindirectmix");
    badIndirectMix.physicalParameters[4].alignment = 1;
    if (t09_probe::isSysVIndirectRecordWithArguments(badIndirectMix, "pindirectmix")) return 125;
    if (!t09_probe::isSysVIndirectRecordWithArguments(copiedPlans.at("pindirectdifferent"), "pindirectdifferent") ||
        copiedPlans.at("pindirectdifferent").logicalReturn.alignment != 4 ||
        copiedPlans.at("pindirectdifferent").physicalParameters[2].alignment != 8 ||
        copiedPlans.at("pindirectdifferent").physicalParameters[4].alignment != 8) return 126;
    if (!t09_probe::isSysVIndirectRecordWithArguments(copiedPlans.at("pdirectindirect"), "pdirectindirect") ||
        copiedPlans.at("pdirectindirect").logicalParameters[1].passing != t09_probe::Passing::Direct ||
        !copiedPlans.at("pdirectindirect").physicalParameters[4].byValue) return 127;
    if (!t09_probe::isSysVIndirectRecordWithArguments(copiedPlans.at("ppairindirect"), "ppairindirect") ||
        copiedPlans.at("ppairindirect").logicalParameters[1].physicalCount != 2 ||
        !copiedPlans.at("ppairindirect").physicalParameters[5].byValue) return 128;
    if (!t09_probe::isSysVIndirectRecordWithArguments(copiedPlans.at("pmixedindirect"), "pmixedindirect") ||
        copiedPlans.at("pmixedindirect").physicalParameters[2].type.kind != t09_probe::TypeKind::Double ||
        copiedPlans.at("pmixedindirect").physicalParameters[3].type.widthOrCount != 32) return 129;
    for (const char* name : {"pidindirect", "pddindirect", "pvfindirect"})
        if (!t09_probe::isSysVIndirectRecordWithArguments(copiedPlans.at(name), name)) return 130;
    if (!t09_probe::isSysVIndirectRecordWithArguments(copiedPlans.at("psretempty"), "psretempty") ||
        copiedPlans.at("psretempty").physicalParameters.size() != 1 ||
        !t09_probe::isSysVIndirectRecordWithArguments(copiedPlans.at("psretmixed"), "psretmixed")) return 131;
    for (const char* name : {"psretonef", "psretoned", "psretonev"})
        if (!t09_probe::isSysVIndirectRecordWithArguments(copiedPlans.at(name), name)) return 132;
    if (!t09_probe::isSysVSingleFloatingRecordRoundTrip(copiedPlans.at("ponef"), "ponef") ||
        !t09_probe::isSysVSingleFloatingRecordRoundTrip(copiedPlans.at("poned"), "poned")) return 92;
    if (!t09_probe::isSysVMixedDoubleI32RecordRoundTrip(pf, "pf") ||
        t09_probe::isSysVMixedDoubleI32RecordRoundTrip(p16, "p16")) return 83;
    if (pf.physicalParameters.size() != 2 || pf.physicalParameters[0].type.kind != t09_probe::TypeKind::Double ||
        pf.physicalParameters[1].type.widthOrCount != 32) return 22;
    const auto& p24 = copiedPlans.at("p24");
    if (!t09_probe::isSysVIndirectRecordRoundTrip(copiedPlans.at("p24b"), "p24b") ||
        !t09_probe::isSysVIndirectRecordRoundTrip(copiedPlans.at("p24i"), "p24i") ||
        !t09_probe::isSysVIndirectRecordRoundTrip(p24, "p24")) return 81;
    for (const char* name : {"p17b", "p20b", "p32b"})
        if (!t09_probe::isSysVIndirectRecordRoundTrip(copiedPlans.at(name), name)) return 82;
    if (p24.logicalParameters[0].passing != t09_probe::Passing::Indirect ||
        !p24.logicalParameters[0].byValue || p24.logicalParameters[0].alignment != 8 ||
        p24.physicalParameters.size() != 2 || !p24.physicalParameters[0].structureReturn ||
        !p24.physicalParameters[1].byValue || !p24.physicalParameters[1].pointee ||
        p24.physicalParameters[1].alignment != 8) return 23;
    const auto& ps8 = copiedPlans.at("ps8");
    if (!ps8.returnSignExtend || !ps8.logicalParameters[0].signExtend ||
        !ps8.physicalParameters[0].signExtend) return 24;
    const auto& pm = copiedPlans.at("pm");
    if (pm.physicalParameters.size() != 6 || !pm.physicalParameters[0].structureReturn ||
        pm.logicalParameters.size() != 4 || pm.logicalParameters[0].firstPhysical != 1 ||
        pm.logicalParameters[0].physicalCount != 1 || pm.logicalParameters[1].firstPhysical != 2 ||
        pm.logicalParameters[1].physicalCount != 2 || pm.logicalParameters[2].firstPhysical != 4 ||
        pm.logicalParameters[2].physicalCount != 1 || pm.logicalParameters[3].firstPhysical != 5 ||
        pm.logicalParameters[3].physicalCount != 1 || !pm.physicalParameters[5].byValue) return 26;
    const auto& pp = copiedPlans.at("pp");
    if (pp.physicalParameters.size() != 2 || !pp.physicalParameters[0].structureReturn ||
        pp.physicalParameters[0].alignment != 1 || !pp.physicalParameters[1].byValue ||
        pp.physicalParameters[1].alignment != 8 || !pp.physicalParameters[1].pointee ||
        !pp.physicalParameters[1].pointee->packed) return 29;
    const auto& pvec = copiedPlans.at("pvec");
    if (pvec.symbol != "\1pvec@@8" || pvec.callingConvention != llvm::CallingConv::X86_VectorCall) return 30;
    const auto& pvoid = copiedPlans.at("pvoid");
    if (pvoid.physicalReturn.kind != t09_probe::TypeKind::Void ||
        pvoid.logicalReturn.passing != t09_probe::Passing::Ignore ||
        !pvoid.logicalParameters.empty() || !pvoid.physicalParameters.empty()) return 32;
    using namespace nativesharp::interop;
    CppSessionOptions options;
    options.language = CppLanguageMode::C;
    options.languageStandard = "c11";
    options.targetTriple = "x86_64-pc-windows-msvc";
    options.driverReferencePath = "clang.exe";
    options.resourceDirectory = argv[1];
    options.workingDirectory = argv[2];
    options.input = CppBufferInput{"abi-probe.c", source};
    CppClangSessionFactory factory;
    auto opened = factory.open(std::move(options), {});
    if (const auto* failure = std::get_if<CppSessionOpenFailure>(&opened)) {
        llvm::errs() << "session open failed: " << failure->phase << ": " << failure->message << "\n";
        for (const auto& diagnostic : failure->diagnostics) llvm::errs() << diagnostic.message << "\n";
        return 35;
    }
    auto session = std::move(std::get<std::unique_ptr<CppInteropSession>>(opened));
    if (!session) return 36;
    const auto functions = session->freeFunctionsSnapshot();
    if (!functions) return 37;
    const auto findType = [&](const std::string& name) -> std::string {
        for (const auto& function : *functions)
            if (function.name == name) return function.canonicalFunctionType;
        return {};
    };
    const std::string p3Type = findType("p3");
    const std::string p3ptrType = findType("p3ptr");
    const std::string pvecType = findType("pvec");
    const std::string vectorType = findType("pv");
    const std::string p24Type = findType("p24");
    const std::string ps8Type = findType("ps8");
    const std::string pmType = findType("pm");
    const std::string oldType = findType("pold");
    const std::string variadicType = findType("pvariadic");
    const std::string nonnullType = findType("pnonnull");
    const std::string incompleteType = findType("pincomplete");
    if (p3Type.empty() || p3ptrType.empty() || pvecType.empty() || vectorType.empty() || p24Type.empty() || ps8Type.empty() || pmType.empty() ||
        oldType.empty() || variadicType.empty() || nonnullType.empty() || incompleteType.empty()) return 38;
    const auto findParameter = [&](const std::string& name, std::size_t index) -> std::optional<CppType> {
        for (const auto& function : *functions)
            if (function.name == name && index < function.parameters.size()) return function.parameters[index].type;
        return std::nullopt;
    };
    const auto smallActual = findParameter("p8", 0);
    const auto mixedActual = findParameter("pf", 0);
    const auto largeActual = findParameter("p24", 0);
    const auto integerActual = findParameter("pscalar", 1);
    const auto floatingActual = findParameter("pscalardouble", 1);
    const auto narrowActual = findParameter("ps8", 0);
    const auto pointerActual = findParameter("p3ptr", 0);
    if (!smallActual || !mixedActual || !largeActual || !integerActual || !floatingActual || !narrowActual || !pointerActual) return 142;
    auto variadicSessionPlan = session->cVariadicCallAbiPlan("pvariadic", variadicType,
        {*smallActual, *mixedActual, *largeActual});
    auto variadicPressurePlan = session->cVariadicCallAbiPlan("pvariadic", variadicType,
        {*integerActual, *integerActual, *integerActual, *integerActual, *integerActual, *mixedActual});
    auto variadicFloatPressurePlan = session->cVariadicCallAbiPlan("pvariadic", variadicType,
        {*floatingActual, *floatingActual, *floatingActual, *floatingActual,
         *floatingActual, *floatingActual, *floatingActual, *floatingActual, *mixedActual});
    auto variadicPointerPlan = session->cVariadicCallAbiPlan("pvariadic", variadicType, {*pointerActual});
    auto wrongRecord = *mixedActual;
    if (!wrongRecord.recordIdentity || !wrongRecord.recordIdentity->clangUsr) return 143;
    wrongRecord.recordIdentity->clangUsr = "incorrect-usr";
    auto wrongSize = *mixedActual;
    wrongSize.sizeBits = 8;
    auto wrongKind = *integerActual;
    wrongKind.kind = CppTypeKind::Floating;
    if (!variadicSessionPlan || !variadicPressurePlan || !variadicFloatPressurePlan || !variadicPointerPlan ||
        variadicSessionPlan->arguments.size() != 4 || variadicSessionPlan->arguments[2].physicalCount != 2 ||
        !variadicPointerPlan->arguments[1].coercion ||
        variadicPointerPlan->arguments[1].coercion->kind != abi_plan::TypeKind::Pointer ||
        !variadicSessionPlan->arguments[3].byValue || !variadicPressurePlan->arguments[6].byValue ||
        !variadicFloatPressurePlan->arguments[9].byValue ||
        session->cVariadicCallAbiPlan("pvariadic", variadicType, {wrongRecord}) ||
        session->cVariadicCallAbiPlan("pvariadic", variadicType, {wrongSize}) ||
        session->cVariadicCallAbiPlan("pvariadic", variadicType, {wrongKind}) ||
        session->cVariadicCallAbiPlan("pvariadic", variadicType, {*narrowActual}) ||
        session->cVariadicCallAbiPlan("pvariadic", "different", {}) ||
        session->cVariadicCallAbiPlan("p3", p3Type, {})) return 144;
    const auto openRecordSession = [&](const char* text) -> std::unique_ptr<CppInteropSession> {
        CppSessionOptions recordOptions;
        recordOptions.language = CppLanguageMode::C;
        recordOptions.languageStandard = "c11";
        recordOptions.targetTriple = "x86_64-pc-windows-msvc";
        recordOptions.driverReferencePath = "clang.exe";
        recordOptions.resourceDirectory = argv[1];
        recordOptions.workingDirectory = argv[2];
        recordOptions.input = CppBufferInput{"variadic-origin.c", text};
        auto result = factory.open(std::move(recordOptions), {});
        if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(result)) return {};
        return std::move(std::get<std::unique_ptr<CppInteropSession>>(result));
    };
    auto pointerOrigin = openRecordSession(
        "typedef struct PointerRecord { int x, y, z; } PointerRecord; "
        "typedef int (__attribute__((sysv_abi)) *PointerFn)(PointerRecord); "
        "typedef int (__attribute__((sysv_abi)) *PointerVarFn)(int, ...); "
        "typedef int (*PointerCdeclFn)(PointerRecord); "
        "typedef struct PointerIncomplete PointerIncomplete; "
        "typedef int (__attribute__((sysv_abi)) *PointerIncompleteFn)(PointerIncomplete); "
        "int __attribute__((sysv_abi)) pointer_real(PointerRecord);");
    if (!pointerOrigin) return 251;
    const auto pointerAliases = pointerOrigin->typeAliasesSnapshot();
    const auto pointerFunctions = pointerOrigin->freeFunctionsSnapshot();
    if (!pointerAliases || !pointerFunctions) return 252;
    std::string pointerSpelling, variadicPointerSpelling, cdeclPointerSpelling, incompletePointerSpelling, realSpelling;
    const CppType* observedPointer = nullptr;
    for (const auto& alias : *pointerAliases)
    {
        if (alias.name == "PointerFn") { pointerSpelling = alias.underlyingType.canonicalSpelling; observedPointer = &alias.underlyingType; }
        if (alias.name == "PointerVarFn") variadicPointerSpelling = alias.underlyingType.canonicalSpelling;
        if (alias.name == "PointerCdeclFn") cdeclPointerSpelling = alias.underlyingType.canonicalSpelling;
        if (alias.name == "PointerIncompleteFn") incompletePointerSpelling = alias.underlyingType.canonicalSpelling;
    }
    for (const auto& function : *pointerFunctions)
        if (function.name == "pointer_real") realSpelling = function.canonicalFunctionType;
    if (pointerSpelling.empty() || variadicPointerSpelling.empty() || cdeclPointerSpelling.empty() ||
        incompletePointerSpelling.empty() || realSpelling.empty()) return 253;
    const auto indirectPlan = pointerOrigin->cFunctionPointerAbiPlan("PointerFn", pointerSpelling);
    const auto realPlan = pointerOrigin->cFunctionAbiPlan("pointer_real", realSpelling);
    if (!indirectPlan || !realPlan || indirectPlan->symbol != "__nsc_pointer_abi_probe" ||
        indirectPlan->callingConvention != realPlan->callingConvention ||
        indirectPlan->targetTriple != realPlan->targetTriple || indirectPlan->dataLayout != realPlan->dataLayout ||
        indirectPlan->physicalReturn.kind != realPlan->physicalReturn.kind ||
        indirectPlan->physicalReturn.widthOrCount != realPlan->physicalReturn.widthOrCount ||
        indirectPlan->logicalParameters.size() != 1 || indirectPlan->physicalParameters.size() != 2 ||
        realPlan->logicalParameters.size() != 1 || realPlan->physicalParameters.size() != 2 ||
        indirectPlan->logicalParameters[0].physicalCount != realPlan->logicalParameters[0].physicalCount ||
        indirectPlan->physicalParameters[0].type.widthOrCount != 64 ||
        indirectPlan->physicalParameters[1].type.widthOrCount != 32 ||
        indirectPlan->physicalParameters[0].type.kind != realPlan->physicalParameters[0].type.kind ||
        indirectPlan->physicalParameters[1].type.kind != realPlan->physicalParameters[1].type.kind) return 254;
    if (pointerOrigin->cFunctionPointerAbiPlan("PointerFn", "different") ||
        pointerOrigin->cFunctionPointerAbiPlan("Missing", pointerSpelling) ||
        pointerOrigin->cFunctionPointerAbiPlan("PointerVarFn", variadicPointerSpelling) ||
        pointerOrigin->cFunctionPointerAbiPlan("PointerCdeclFn", cdeclPointerSpelling) ||
        pointerOrigin->cFunctionPointerAbiPlan("PointerIncompleteFn", incompletePointerSpelling)) return 255;
    const auto repeatedPointerPlan = pointerOrigin->cFunctionPointerAbiPlan("PointerFn", pointerSpelling);
    const auto afterPointerFunctions = pointerOrigin->freeFunctionsSnapshot();
    if (!repeatedPointerPlan || repeatedPointerPlan->physicalParameters.size() != 2 ||
        !afterPointerFunctions || afterPointerFunctions->size() != pointerFunctions->size()) return 257;
    if (!observedPointer || !observedPointer->element || !observedPointer->element->function ||
        observedPointer->element->function->parameterTypes.size() != 1) return 258;
    const auto& pointerSignature = *observedPointer->element->function;
    CppVariadicArgumentOrigin pointerResult{pointerSignature.returnType, pointerOrigin.get()};
    std::vector<CppVariadicArgumentOrigin> pointerArguments{{pointerSignature.parameterTypes[0], pointerOrigin.get()}};
    if (!reconcileCppFunctionPointerAbiPlan(*pointerOrigin, "PointerFn", *observedPointer,
        pointerResult, pointerArguments)) return 259;
    auto equivalentPointerOrigin = openRecordSession("struct PointerRecord { int x, y, z; };");
    auto incompatiblePointerOrigin = openRecordSession("struct PointerRecord { unsigned x; int y, z; };");
    if (!equivalentPointerOrigin || !incompatiblePointerOrigin) return 260;
    pointerArguments[0].origin = equivalentPointerOrigin.get();
    if (!reconcileCppFunctionPointerAbiPlan(*pointerOrigin, "PointerFn", *observedPointer,
        pointerResult, pointerArguments)) return 261;
    pointerArguments[0].origin = incompatiblePointerOrigin.get();
    if (reconcileCppFunctionPointerAbiPlan(*pointerOrigin, "PointerFn", *observedPointer,
        pointerResult, pointerArguments)) return 262;
    pointerArguments[0].origin = nullptr;
    if (reconcileCppFunctionPointerAbiPlan(*pointerOrigin, "PointerFn", *observedPointer,
        pointerResult, pointerArguments)) return 263;
    pointerArguments[0].origin = pointerOrigin.get();
    pointerArguments[0].type.sizeBits = 64;
    if (reconcileCppFunctionPointerAbiPlan(*pointerOrigin, "PointerFn", *observedPointer,
        pointerResult, pointerArguments)) return 264;
    pointerArguments[0].type = pointerSignature.parameterTypes[0];
    auto forgedPointer = *observedPointer;
    auto forgedElement = std::make_shared<CppType>(*forgedPointer.element);
    auto forgedSignature = std::make_shared<CppFunctionType>(*forgedElement->function);
    forgedSignature->parameterTypes[0].sizeBits = 64;
    forgedElement->function = forgedSignature;
    forgedPointer.element = forgedElement;
    if (reconcileCppFunctionPointerAbiPlan(*pointerOrigin, "PointerFn", forgedPointer,
        pointerResult, pointerArguments)) return 266;
    if (!equivalentPointerOrigin->close().succeeded || !incompatiblePointerOrigin->close().succeeded) return 265;
    if (!pointerOrigin->close().succeeded || pointerOrigin->cFunctionPointerAbiPlan("PointerFn", pointerSpelling)) return 256;
    auto equivalentOrigin = openRecordSession(
        "struct RF { double x; int y; }; struct R3 { unsigned char x, y, z; };");
    auto incompatibleOrigin = openRecordSession("struct RF { long long x; int y; };");
    if (!equivalentOrigin || !incompatibleOrigin) return 149;
    auto attributeOrigin = openRecordSession(
        "typedef int Number;\n"
        "typedef int NumberAligned __attribute__((aligned(8)));\n"
        "struct Plain { double x; int y; };\n"
        "struct __attribute__((packed)) Packed { double x; int y; };\n"
        "struct __attribute__((packed, aligned(2))) PackedAligned { double x; int y; };\n"
        "struct FieldAttribute { double x; int y __attribute__((aligned(8))); };\n"
        "struct AliasField { Number x; };\n"
        "struct AliasAligned { NumberAligned x; };\n"
        "struct QualifiedField { const int x; };\n"
        "struct Bits { unsigned int x : 1; };\n"
        "struct UnnamedBits { unsigned int : 1; };\n"
        "struct ZeroBits { unsigned int x : 3; unsigned int : 0; unsigned int y : 4; };\n"
        "struct PointerField { int *x; };\n"
        "struct ArrayField { int x[2]; };\n"
        "struct AliasArray { Number x[2]; };\n"
        "struct Leaf { int x; }; struct Parent { struct Leaf leaf; };\n"
        "typedef struct Leaf LeafAlias; struct AliasRecord { LeafAlias leaf; };\n"
        "struct Forward;\n"
        "#pragma pack(push, 1)\n"
        "struct PragmaPacked { int x; };\n"
        "#pragma pack(pop)\n");
    if (!attributeOrigin || attributeOrigin->cRecordHasPlainReplaySyntax("Plain") != true ||
        attributeOrigin->cRecordHasPlainReplaySyntax("Packed") != false ||
        attributeOrigin->cRecordHasPlainReplaySyntax("FieldAttribute") != false ||
        attributeOrigin->cRecordHasPlainReplaySyntax("AliasField") != true ||
        attributeOrigin->cRecordHasPlainReplaySyntax("AliasAligned") != false ||
        attributeOrigin->cRecordHasPlainReplaySyntax("QualifiedField") != false ||
        attributeOrigin->cRecordHasPlainReplaySyntax("Bits") != true ||
        attributeOrigin->cRecordHasPlainReplaySyntax("UnnamedBits") != true ||
        attributeOrigin->cRecordHasPlainReplaySyntax("ZeroBits") != true ||
        attributeOrigin->cRecordHasPlainReplaySyntax("PointerField") != false ||
        attributeOrigin->cRecordHasPlainReplaySyntax("ArrayField") != true ||
        attributeOrigin->cRecordHasPlainReplaySyntax("AliasArray") != true ||
        attributeOrigin->cRecordHasPlainReplaySyntax("Parent") != true ||
        attributeOrigin->cRecordHasPlainReplaySyntax("AliasRecord") != true ||
        attributeOrigin->cRecordHasPlainReplaySyntax("Forward") ||
        attributeOrigin->cRecordHasPlainReplaySyntax("PragmaPacked") != false ||
        attributeOrigin->cRecordHasPackedReplaySyntax("Plain") != false ||
        attributeOrigin->cRecordHasPackedReplaySyntax("Packed") != true ||
        attributeOrigin->cRecordHasPackedReplaySyntax("PackedAligned") != false ||
        attributeOrigin->cRecordHasPackedReplaySyntax("FieldAttribute") != false ||
        attributeOrigin->cRecordHasPackedReplaySyntax("PragmaPacked") != false ||
        attributeOrigin->cRecordHasPlainReplaySyntax("Missing")) return 209;
    if (!attributeOrigin->close().succeeded || attributeOrigin->cRecordHasPlainReplaySyntax("Plain")) return 210;
    auto nestedOrigin = openRecordSession("struct Leaf { int x; int y; }; struct Parent { double x; struct Leaf leaf; };");
    if (!nestedOrigin || nestedOrigin->cRecordHasPlainReplaySyntax("Parent") != true) return 217;
    const auto nestedRecords = nestedOrigin->recordsSnapshot();
    if (!nestedRecords) return 218;
    CppCRecordSnapshot nestedLeaf, nestedParent;
    for (const auto& item : *nestedRecords)
    {
        auto* snapshot = item.name == "Leaf" ? &nestedLeaf : item.name == "Parent" ? &nestedParent : nullptr;
        if (!snapshot) continue;
        snapshot->targetTriple = "x86_64-pc-windows-msvc";
        snapshot->kind = item.kind;
        snapshot->complete = item.complete;
        snapshot->valueLayoutUsable = true;
        snapshot->plainReplaySyntax = true;
        snapshot->clangUsr = item.clangUsr;
        snapshot->sizeBits = item.sizeBits;
        snapshot->alignmentBits = item.alignmentBits;
        snapshot->fields = item.fields;
    }
    const CppReplayRecordResolver resolveLeaf = [&](const CppType& type) -> std::optional<std::string> {
        if (!type.recordIdentity || type.recordIdentity->clangUsr != nestedLeaf.clangUsr ||
            type.recordIdentity->name != "Leaf") return std::nullopt;
        return "struct __nsc_abi_record_1";
    };
    const auto leafText = makeSimpleCRecordAbiReplay(nestedLeaf, 1);
    const auto parentText = makeSimpleCRecordAbiReplay(nestedParent, 0, resolveLeaf);
    if (!leafText || !parentText) return 219;
    auto nestedReplay = openRecordSession((*leafText + *parentText).c_str());
    if (!nestedReplay) return 220;
    const auto rebuiltRecords = nestedReplay->recordsSnapshot();
    if (!rebuiltRecords) return 221;
    const CppRecordDeclaration* rebuiltParent = nullptr;
    for (const auto& item : *rebuiltRecords)
        if (item.name == "__nsc_abi_record") rebuiltParent = &item;
    if (!rebuiltParent || !matchesSimpleCRecordAbiReplay(nestedParent, *rebuiltParent, 0, resolveLeaf)) return 222;
    auto wrongNested = nestedParent;
    if (wrongNested.fields.size() != 2 || !wrongNested.fields[1].type.recordIdentity) return 223;
    wrongNested.fields[1].type.recordIdentity->clangUsr = "wrong-origin";
    if (matchesSimpleCRecordAbiReplay(wrongNested, *rebuiltParent, 0, resolveLeaf)) return 224;
    if (!nestedReplay->close().succeeded || !nestedOrigin->close().succeeded) return 225;
    auto bitsOrigin = openRecordSession("struct BitsLayout { unsigned a:3; unsigned b:5; unsigned c:24; };");
    if (!bitsOrigin || bitsOrigin->cRecordHasPlainReplaySyntax("BitsLayout") != true) return 226;
    const auto bitsRecords = bitsOrigin->recordsSnapshot();
    if (!bitsRecords) return 227;
    CppCRecordSnapshot bitsSnapshot;
    for (const auto& item : *bitsRecords)
        if (item.name == "BitsLayout")
        {
            bitsSnapshot.targetTriple = "x86_64-pc-windows-msvc";
            bitsSnapshot.kind = item.kind;
            bitsSnapshot.complete = item.complete;
            bitsSnapshot.valueLayoutUsable = true;
            bitsSnapshot.plainReplaySyntax = true;
            bitsSnapshot.sizeBits = item.sizeBits;
            bitsSnapshot.alignmentBits = item.alignmentBits;
            bitsSnapshot.fields = item.fields;
        }
    const auto bitsText = makeSimpleCRecordAbiReplay(bitsSnapshot);
    if (!bitsText) return 228;
    auto bitsReplay = openRecordSession(bitsText->c_str());
    if (!bitsReplay) return 229;
    const auto rebuiltBits = bitsReplay->recordsSnapshot();
    if (!rebuiltBits || rebuiltBits->size() != 1 ||
        !matchesSimpleCRecordAbiReplay(bitsSnapshot, rebuiltBits->front())) return 230;
    auto brokenBits = bitsSnapshot;
    brokenBits.fields[1].bitWidth = 4;
    if (matchesSimpleCRecordAbiReplay(brokenBits, rebuiltBits->front())) return 231;
    brokenBits = bitsSnapshot;
    brokenBits.fields[1].offsetBits = 4;
    if (matchesSimpleCRecordAbiReplay(brokenBits, rebuiltBits->front())) return 232;
    brokenBits = bitsSnapshot;
    brokenBits.fields[0].name.clear();
    if (!makeSimpleCRecordAbiReplay(brokenBits) ||
        matchesSimpleCRecordAbiReplay(brokenBits, rebuiltBits->front())) return 233;
    if (!bitsReplay->close().succeeded || !bitsOrigin->close().succeeded) return 234;
    auto gapOrigin = openRecordSession("struct Gap { unsigned a:3; unsigned :5; unsigned b:8; unsigned :0; unsigned c:16; };");
    if (!gapOrigin || gapOrigin->cRecordHasPlainReplaySyntax("Gap") != true) return 235;
    const auto gapRecords = gapOrigin->recordsSnapshot();
    if (!gapRecords || gapRecords->size() != 1) return 236;
    CppCRecordSnapshot gapSnapshot;
    gapSnapshot.targetTriple = "x86_64-pc-windows-msvc";
    gapSnapshot.kind = gapRecords->front().kind;
    gapSnapshot.complete = gapRecords->front().complete;
    gapSnapshot.valueLayoutUsable = true;
    gapSnapshot.plainReplaySyntax = true;
    gapSnapshot.sizeBits = gapRecords->front().sizeBits;
    gapSnapshot.alignmentBits = gapRecords->front().alignmentBits;
    gapSnapshot.fields = gapRecords->front().fields;
    const auto gapText = makeSimpleCRecordAbiReplay(gapSnapshot);
    if (!gapText) return 237;
    auto gapReplay = openRecordSession(gapText->c_str());
    if (!gapReplay) return 238;
    const auto rebuiltGap = gapReplay->recordsSnapshot();
    if (!rebuiltGap || rebuiltGap->size() != 1 ||
        !matchesSimpleCRecordAbiReplay(gapSnapshot, rebuiltGap->front())) return 239;
    auto wrongGap = gapSnapshot;
    wrongGap.fields[3].bitWidth = 1;
    if (matchesSimpleCRecordAbiReplay(wrongGap, rebuiltGap->front())) return 240;
    wrongGap = gapSnapshot;
    wrongGap.fields[3].offsetBits = 0;
    if (matchesSimpleCRecordAbiReplay(wrongGap, rebuiltGap->front())) return 241;
    if (!gapReplay->close().succeeded || !gapOrigin->close().succeeded) return 242;
    auto boolOrigin = openRecordSession("struct BooleanFields { _Bool x:1; _Bool y:1; _Bool z; };");
    if (!boolOrigin || boolOrigin->cRecordHasPlainReplaySyntax("BooleanFields") != true) return 243;
    const auto boolRecords = boolOrigin->recordsSnapshot();
    if (!boolRecords || boolRecords->size() != 1) return 244;
    CppCRecordSnapshot boolSnapshot;
    boolSnapshot.targetTriple = "x86_64-pc-windows-msvc";
    boolSnapshot.kind = boolRecords->front().kind;
    boolSnapshot.complete = boolRecords->front().complete;
    boolSnapshot.valueLayoutUsable = true;
    boolSnapshot.plainReplaySyntax = true;
    boolSnapshot.sizeBits = boolRecords->front().sizeBits;
    boolSnapshot.alignmentBits = boolRecords->front().alignmentBits;
    boolSnapshot.fields = boolRecords->front().fields;
    const auto boolText = makeSimpleCRecordAbiReplay(boolSnapshot);
    if (!boolText) return 245;
    auto boolReplay = openRecordSession(boolText->c_str());
    if (!boolReplay) return 246;
    const auto rebuiltBool = boolReplay->recordsSnapshot();
    if (!rebuiltBool || rebuiltBool->size() != 1 ||
        !matchesSimpleCRecordAbiReplay(boolSnapshot, rebuiltBool->front())) return 247;
    auto wrongBool = boolSnapshot;
    wrongBool.fields[0].bitWidth = 2;
    if (makeSimpleCRecordAbiReplay(wrongBool) || matchesSimpleCRecordAbiReplay(wrongBool, rebuiltBool->front())) return 248;
    wrongBool = boolSnapshot;
    wrongBool.fields[2].offsetBits = 0;
    if (matchesSimpleCRecordAbiReplay(wrongBool, rebuiltBool->front())) return 249;
    if (!boolReplay->close().succeeded || !boolOrigin->close().succeeded) return 250;
    const auto originalRecords = equivalentOrigin->recordsSnapshot();
    if (!originalRecords) return 192;
    std::vector<CppSemanticRecordEntry> replayEntries;
    for (const auto& record : *originalRecords)
        if (record.name == "RF")
            replayEntries.push_back({{}, 0, 0, "x86_64-pc-windows-msvc", record,
                                     projectCppRecordIdentity(record, "AbiProbe"), std::nullopt});
    const auto replayGroups = groupCppSemanticRecords(replayEntries);
    if (replayGroups.groups.size() != 1) return 202;
    const auto capturedReplay = captureCppCRecordSnapshot(replayEntries, replayGroups.groups[0]);
    const auto* capturedRecord = std::get_if<CppCRecordSnapshot>(&capturedReplay);
    if (!capturedRecord) return 203;
    if (equivalentOrigin->cRecordHasPlainReplaySyntax("RF") != true) return 212;
    auto observedReplay = *capturedRecord;
    observedReplay.plainReplaySyntax = true;
    std::vector<std::uint8_t> replayBytes;
    std::vector<CppCRecordSnapshot> restoredRecords;
    std::string replayError;
    if (!metadata_detail::encodeCRecordCatalog(std::span(&observedReplay, 1), replayBytes, replayError) ||
        !metadata_detail::decodeCRecordCatalog(replayBytes, restoredRecords, replayError) ||
        restoredRecords.size() != 1 || !restoredRecords[0].plainReplaySyntax) return 204;
    auto inconsistentObservation = observedReplay;
    inconsistentObservation.fields[0].bitWidth = 1;
    std::vector<std::uint8_t> rejectedBytes;
    if (metadata_detail::encodeCRecordCatalog(std::span(&inconsistentObservation, 1), rejectedBytes, replayError)) return 213;
    std::vector<std::uint8_t> unobservedBytes;
    if (!metadata_detail::encodeCRecordCatalog(std::span(capturedRecord, 1), unobservedBytes, replayError) ||
        unobservedBytes.size() != replayBytes.size()) return 215;
    auto corruptBytes = replayBytes;
    std::size_t differences = 0;
    for (std::size_t i = 0; i < replayBytes.size(); ++i)
        if (replayBytes[i] != unobservedBytes[i]) { ++differences; corruptBytes[i] = 2; }
    std::vector<CppCRecordSnapshot> corruptOutput;
    if (differences != 1 || metadata_detail::decodeCRecordCatalog(corruptBytes, corruptOutput, replayError)) return 216;
    metadata_detail::CRecordContractComparison replayComparison{};
    if (!metadata_detail::compareCRecordContracts(observedReplay, *capturedRecord, replayComparison, replayError) ||
        replayComparison != metadata_detail::CRecordContractComparison::Matching) return 214;
    const auto& replayRecord = restoredRecords[0];
    const auto replayText = makeSimpleCRecordAbiReplay(replayRecord);
    if (!replayText) return 193;
    auto replay = openRecordSession((*replayText +
        "int __attribute__((sysv_abi)) pvariadic(int, ...); "
        "int replay_type(struct __nsc_abi_record);").c_str());
    if (!replay) return 194;
    const auto replayRecords = replay->recordsSnapshot();
    const auto replayFunctions = replay->freeFunctionsSnapshot();
    if (!replayRecords || !replayFunctions) return 195;
    const CppRecordDeclaration* replayDeclaration = nullptr;
    for (const auto& record : *replayRecords)
        if (record.name == "__nsc_abi_record") replayDeclaration = &record;
    const CppFunctionDeclaration* replayFunction = nullptr;
    const CppType* replayType = nullptr;
    for (const auto& function : *replayFunctions)
    {
        if (function.name == "pvariadic") replayFunction = &function;
        if (function.name == "replay_type" && function.parameters.size() == 1)
            replayType = &function.parameters[0].type;
    }
    if (!replayDeclaration || !replayFunction || !replayType ||
        !matchesSimpleCRecordAbiReplay(replayRecord, *replayDeclaration)) return 196;
    const auto replayPlan = replay->cVariadicCallAbiPlan("pvariadic", replayFunction->canonicalFunctionType,
        {*replayType});
    const auto replayPressure = replay->cVariadicCallAbiPlan("pvariadic", replayFunction->canonicalFunctionType,
        {*integerActual, *integerActual, *integerActual, *integerActual, *integerActual, *replayType});
    const auto replayFloatPressure = replay->cVariadicCallAbiPlan("pvariadic", replayFunction->canonicalFunctionType,
        {*floatingActual, *floatingActual, *floatingActual, *floatingActual,
         *floatingActual, *floatingActual, *floatingActual, *floatingActual, *replayType});
    const auto originalPlan = session->cVariadicCallAbiPlan("pvariadic", variadicType, {*mixedActual});
    if (!replayPlan || !replayPressure || !replayFloatPressure || !originalPlan ||
        abi_plan::encodeVariadicCallPlanForCache(*replayPlan) != abi_plan::encodeVariadicCallPlanForCache(*originalPlan) ||
        abi_plan::encodeVariadicCallPlanForCache(*replayPressure) != abi_plan::encodeVariadicCallPlanForCache(*variadicPressurePlan) ||
        abi_plan::encodeVariadicCallPlanForCache(*replayFloatPressure) != abi_plan::encodeVariadicCallPlanForCache(*variadicFloatPressurePlan)) return 197;
    auto wrongReplay = replayRecord;
    wrongReplay.fields[1].offsetBits = 0;
    if (matchesSimpleCRecordAbiReplay(wrongReplay, *replayDeclaration)) return 198;
    wrongReplay = replayRecord;
    wrongReplay.fields[0].type.kind = CppTypeKind::SignedInteger;
    if (makeSimpleCRecordAbiReplay(wrongReplay)) return 199;
    wrongReplay.fields[0].type.canonicalSpelling = "long long";
    if (!makeSimpleCRecordAbiReplay(wrongReplay) ||
        matchesSimpleCRecordAbiReplay(wrongReplay, *replayDeclaration)) return 208;
    wrongReplay = replayRecord;
    wrongReplay.fields[0].bitWidth = 1;
    if (makeSimpleCRecordAbiReplay(wrongReplay)) return 200;
    wrongReplay = replayRecord;
    wrongReplay.fields[0].type.canonicalSpelling = "double; int injected";
    if (makeSimpleCRecordAbiReplay(wrongReplay)) return 205;
    wrongReplay = replayRecord;
    wrongReplay.alignmentBits = 32;
    if (matchesSimpleCRecordAbiReplay(wrongReplay, *replayDeclaration)) return 206;
    wrongReplay = replayRecord;
    wrongReplay.targetTriple = "x86_64-unknown-linux-gnu";
    if (makeSimpleCRecordAbiReplay(wrongReplay)) return 207;
    if (!replay->close().succeeded) return 201;
    const auto conflictingRecords = incompatibleOrigin->recordsSnapshot();
    if (!conflictingRecords) return 160;
    bool sameIdentityAndLayout = false;
    for (const auto& record : *conflictingRecords)
        if (record.name == "RF" && record.clangUsr == mixedActual->recordIdentity->clangUsr &&
            record.sizeBits == mixedActual->sizeBits && record.alignmentBits == mixedActual->alignmentBits)
            sameIdentityAndLayout = true;
    if (!sameIdentityAndLayout) return 161;
    std::vector<CppVariadicArgumentOrigin> origins{{*mixedActual, equivalentOrigin.get()}};
    auto reconciledPlan = reconcileCppVariadicCallAbiPlan(*session, "pvariadic", variadicType, origins);
    if (!reconciledPlan || reconciledPlan->arguments.size() != 2 ||
        reconciledPlan->arguments[1].physicalCount != 2) return 150;
    if (!validateCppVariadicSysVCallPlan(*reconciledPlan, "pvariadic", 1) ||
        !validateCppVariadicSysVCallPlan(*variadicSessionPlan, "pvariadic", 1) ||
        !validateCppVariadicSysVCallPlan(*variadicPressurePlan, "pvariadic", 1) ||
        !validateCppVariadicSysVCallPlan(*variadicFloatPressurePlan, "pvariadic", 1) ||
        validateCppVariadicSysVCallPlan(*reconciledPlan, "other", 1)) return 162;
    const auto encodedCall = abi_plan::encodeVariadicCallPlanForCache(*variadicSessionPlan);
    if (!encodedCall) return 175;
    const auto decodedCall = abi_plan::decodeVariadicCallPlanFromCache(*encodedCall);
    if (!decodedCall || !validateCppVariadicSysVCallPlan(*decodedCall, "pvariadic", 1) ||
        abi_plan::encodeVariadicCallPlanForCache(*decodedCall) != encodedCall) return 176;
    for (std::size_t size = 0; size < encodedCall->size(); ++size)
        if (abi_plan::decodeVariadicCallPlanFromCache(std::string_view(*encodedCall).substr(0, size))) return 177;
    if (abi_plan::decodeVariadicCallPlanFromCache(*encodedCall + "x")) return 178;
    auto invalidLength = *encodedCall;
    for (std::size_t i = 0; i < 8; ++i) invalidLength[i] = static_cast<char>(0xff);
    if (abi_plan::decodeVariadicCallPlanFromCache(invalidLength)) return 179;
    auto brokenPlan = *variadicSessionPlan;
    brokenPlan.arguments[2].firstPhysical++;
    if (validateCppVariadicSysVCallPlan(brokenPlan, "pvariadic", 1)) return 163;
    brokenPlan = *variadicSessionPlan;
    brokenPlan.physicalParameters[2].type.kind = abi_plan::TypeKind::Integer;
    if (validateCppVariadicSysVCallPlan(brokenPlan, "pvariadic", 1)) return 164;
    brokenPlan = *variadicSessionPlan;
    brokenPlan.physicalParameters[4].alignment = 4;
    if (validateCppVariadicSysVCallPlan(brokenPlan, "pvariadic", 1)) return 165;
    brokenPlan = *variadicSessionPlan;
    brokenPlan.physicalParameters[4].pointee.reset();
    if (validateCppVariadicSysVCallPlan(brokenPlan, "pvariadic", 1)) return 166;
    brokenPlan = *variadicSessionPlan;
    brokenPlan.physicalReturn.widthOrCount = 64;
    if (validateCppVariadicSysVCallPlan(brokenPlan, "pvariadic", 1)) return 167;
    brokenPlan = *variadicSessionPlan;
    brokenPlan.callingConvention = 0;
    if (validateCppVariadicSysVCallPlan(brokenPlan, "pvariadic", 1)) return 168;
    brokenPlan = *variadicSessionPlan;
    brokenPlan.requiredArguments = brokenPlan.arguments.size() + 1;
    if (validateCppVariadicSysVCallPlan(brokenPlan, "pvariadic", 1)) return 169;
    brokenPlan = *variadicSessionPlan;
    brokenPlan.arguments[1].coercion = abi_plan::Type{};
    brokenPlan.physicalParameters[1].type = abi_plan::Type{};
    if (validateCppVariadicSysVCallPlan(brokenPlan, "pvariadic", 1)) return 172;
    brokenPlan = *variadicSessionPlan;
    brokenPlan.physicalParameters[0].alignment = 3;
    if (validateCppVariadicSysVCallPlan(brokenPlan, "pvariadic", 1)) return 173;
    brokenPlan = *variadicSessionPlan;
    brokenPlan.requiredArguments = 0;
    if (validateCppVariadicSysVCallPlan(brokenPlan, "pvariadic", 1)) return 174;
    std::vector<CppVariadicArgumentOrigin> pointerOrigins{{*pointerActual, equivalentOrigin.get()}};
    auto reconciledPointer = reconcileCppVariadicCallAbiPlan(*session, "pvariadic", variadicType, pointerOrigins);
    if (!reconciledPointer || reconciledPointer->physicalParameters[1].type.kind != abi_plan::TypeKind::Pointer) return 156;
    pointerOrigins[0].origin = nullptr;
    if (reconcileCppVariadicCallAbiPlan(*session, "pvariadic", variadicType, pointerOrigins)) return 157;
    origins[0].origin = incompatibleOrigin.get();
    if (reconcileCppVariadicCallAbiPlan(*session, "pvariadic", variadicType, origins)) return 151;
    origins[0].origin = nullptr;
    if (reconcileCppVariadicCallAbiPlan(*session, "pvariadic", variadicType, origins)) return 152;
    std::vector<CppVariadicArgumentOrigin> invalidOrigin{{wrongSize, session.get()}};
    if (reconcileCppVariadicCallAbiPlan(*session, "pvariadic", variadicType, invalidOrigin)) return 158;
    std::vector<CppVariadicArgumentOrigin> scalarOrigin{{*integerActual, nullptr}};
    if (!reconcileCppVariadicCallAbiPlan(*session, "pvariadic", variadicType, scalarOrigin)) return 159;
    origins[0].origin = equivalentOrigin.get();
    if (!equivalentOrigin->close().succeeded ||
        reconcileCppVariadicCallAbiPlan(*session, "pvariadic", variadicType, origins)) return 153;
    if (!incompatibleOrigin->close().succeeded) return 154;
    equivalentOrigin.reset();
    incompatibleOrigin.reset();
    if (reconciledPlan->symbol != "pvariadic" || reconciledPlan->physicalParameters.size() != 3 ||
        reconciledPlan->physicalParameters[1].type.kind != abi_plan::TypeKind::Double) return 155;
    const auto diagnosticsBefore = session->diagnosticsSnapshot().size();
    auto sessionPlan = session->cFunctionAbiPlan("p3", p3Type);
    auto vectorcallPlan = session->cFunctionAbiPlan("pvec", pvecType);
    auto aggregatePlan = session->cFunctionAbiPlan("p24", p24Type);
    auto mixedPlan = session->cFunctionAbiPlan("pm", pmType);
    auto scalarPlan = session->cFunctionAbiPlan("ps8", ps8Type);
    auto pointerPlan = session->cFunctionAbiPlan("p3ptr", p3ptrType);
    if (!sessionPlan || !vectorcallPlan || !aggregatePlan || !mixedPlan || !scalarPlan || !pointerPlan || session->cFunctionAbiPlan("p3", "different") ||
        session->cFunctionAbiPlan("missing", p3Type) || session->cFunctionAbiPlan("pv", vectorType) ||
        session->cFunctionAbiPlan("pold", oldType) || session->cFunctionAbiPlan("pvariadic", variadicType) ||
        session->cFunctionAbiPlan("pnonnull", nonnullType) || session->cFunctionAbiPlan("pincomplete", incompleteType) ||
        session->diagnosticsSnapshot().size() != diagnosticsBefore) return 39;
    const CppFunctionDeclaration* scalarDeclaration = nullptr;
    for (const auto& function : *functions) if (function.name == "ps8") scalarDeclaration = &function;
    if (!scalarDeclaration) return 47;
    auto observation = *scalarDeclaration;
    observation.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*scalarPlan));
    auto projection = projectCppSemanticFunction(observation, 0, "AbiProbe");
    const auto* projected = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projection);
    if (!projected || !projected->cppAbiPlan || projected->cppAbiPlan->symbol != "ps8" ||
        !projected->cppAbiPlan->returnSignExtend) return 48;
    const auto recordDeclarations = session->recordsSnapshot();
    if (!recordDeclarations) return 49;
    std::vector<CppSemanticRecordEntry> recordEntries;
    for (const auto& record : *recordDeclarations)
        recordEntries.push_back({{}, 0, 0, "x86_64-pc-windows-msvc", record,
                                 projectCppRecordIdentity(record, "AbiProbe"), std::nullopt});
    const auto recordGroups = groupCppSemanticRecords(recordEntries);
    const CppFunctionDeclaration* recordDeclaration = nullptr;
    for (const auto& function : *functions) if (function.name == "p3ptr") recordDeclaration = &function;
    if (!recordDeclaration) return 50;
    auto recordObservation = *recordDeclaration;
    recordObservation.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*pointerPlan));
    auto recordProjection = projectCppSemanticFunctionWithRecords(recordObservation, 0, "AbiProbe", recordGroups);
    if (const auto* failure = std::get_if<CppSemanticFunctionFailure>(&recordProjection))
        llvm::errs() << "record projection failed: " << failure->message << "\n";
    const auto* projectedRecord = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&recordProjection);
    if (!projectedRecord || !projectedRecord->cppAbiPlan || projectedRecord->cppAbiPlan->symbol != "p3ptr") return 51;
    if (!session->close().succeeded || session->cFunctionAbiPlan("p3", p3Type) ||
        session->cVariadicCallAbiPlan("pvariadic", variadicType, {*mixedActual})) return 40;
    session.reset();
    if (sessionPlan->symbol != "p3" || sessionPlan->physicalReturn.widthOrCount != 24 ||
        vectorcallPlan->symbol != "\1pvec@@8" ||
        vectorcallPlan->callingConvention != llvm::CallingConv::X86_VectorCall ||
        aggregatePlan->physicalParameters.size() != 2 || !aggregatePlan->physicalParameters[1].byValue ||
        mixedPlan->logicalParameters.size() != 4 || mixedPlan->logicalParameters[1].physicalCount != 2 ||
        projected->cppAbiPlan->symbol != "ps8" || projectedRecord->cppAbiPlan->symbol != "p3ptr" ||
        variadicSessionPlan->symbol != "pvariadic" || variadicSessionPlan->arguments[2].physicalCount != 2 ||
        variadicSessionPlan->physicalParameters.size() != 5 ||
        variadicSessionPlan->physicalParameters[2].type.kind != abi_plan::TypeKind::Double ||
        !variadicSessionPlan->physicalParameters[4].pointee ||
        !variadicPressurePlan->arguments[6].byValue || !variadicFloatPressurePlan->arguments[9].byValue) return 41;
    CppSessionOptions inventoryOptions;
    inventoryOptions.language = CppLanguageMode::C;
    inventoryOptions.languageStandard = "c11";
    inventoryOptions.targetTriple = "x86_64-pc-windows-msvc";
    inventoryOptions.driverReferencePath = "clang.exe";
    inventoryOptions.resourceDirectory = argv[1];
    inventoryOptions.workingDirectory = argv[2];
    inventoryOptions.input = CppBufferInput{"abi-inventory.c", source};
    auto inventoryOpened = factory.open(std::move(inventoryOptions), {});
    if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(inventoryOpened)) return 52;
    auto inventorySession = std::move(std::get<std::unique_ptr<CppInteropSession>>(inventoryOpened));
    if (!inventorySession) return 53;
    namespace compilation = nativesharp::compilation;
    const auto uniqueDirectory = std::filesystem::path(argv[2]) /
        ("abi-inventory-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code uniqueError;
    if (!std::filesystem::create_directory(uniqueDirectory, uniqueError) || uniqueError) return 57;
    const auto projectPath = uniqueDirectory / "abi-inventory.nsproj";
    { std::ofstream projectFile(projectPath); if (!projectFile) return 58; projectFile << "<Project/>"; }
    std::vector<compilation::Project> projects;
    projects.emplace_back(projectPath, "AbiProbe", compilation::ProjectOutputKind::Library,
        compilation::ProjectSourceSet(std::filesystem::path(argv[2])), std::vector<std::filesystem::path>{},
        std::vector<compilation::LibraryReference>{}, std::vector<compilation::SdkManifest>{});
    compilation::ProjectGraph graph(projectPath, std::move(projects));
    auto preparedResult = prepareCppRecordObservation(recordEntries, recordGroups);
    auto* prepared = std::get_if<CppRecordObservationInputs>(&preparedResult);
    if (!prepared) return 60;
    nativesharp::frontend::ProjectNameLookup lookup;
    if (!lookup.setCppImportedTypes(std::move(prepared->types))) return 61;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> units;
    if (!lookup.build(graph, units, diagnostics) || diagnostics.hasErrors()) return 62;
    const CppFunctionDeclaration* valueDeclaration = nullptr;
    for (const auto& function : *functions) if (function.name == "p3") valueDeclaration = &function;
    if (!valueDeclaration) return 63;
    auto valueObservation = *valueDeclaration;
    valueObservation.abiPlan = std::make_shared<const abi_plan::Plan>(*sessionPlan);
    auto valueProjection = projectCppSemanticFunctionWithRecords(valueObservation, 0, "AbiProbe", recordGroups, &lookup);
    const auto* projectedValue = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&valueProjection);
    if (!projectedValue || !projectedValue->cppAbiPlan || projectedValue->cppAbiPlan->symbol != "p3") return 64;
    const auto transportProject = uniqueDirectory / "transport.nsproj";
    { std::ofstream file(transportProject); if (!file) return 65;
      file << "<Project><PropertyGroup><Name>AbiProbe</Name></PropertyGroup><ItemGroup>"
              "<Compile Include=\"transport.ns\"/></ItemGroup></Project>"; }
    { std::ofstream file(uniqueDirectory / "transport.ns"); if (!file) return 66;
      file << "int Main() { if (ps8((sbyte)-7) != (sbyte)-7) return 1; return 0; }"; }
    nativesharp::frontend::ProjectCompilation transportCompilation;
    std::string transportError;
    if (!transportCompilation.setCppImportedFunctions({*projected}, transportError) ||
        !transportCompilation.compileProjectOopMir(transportProject, transportError)) {
        llvm::errs() << "ABI transport project failed: " << transportError << "\n";
        return 67;
    }
    const auto* hir = transportCompilation.oopHirModel();
    const auto* mir = transportCompilation.oopMirModel();
    if (!hir || !mir) return 68;
    bool foundHirPlan = false, foundMirPlan = false;
    for (const auto& function : hir->functions())
        if (function.mangledName == "ps8") foundHirPlan = function.cppAbiPlan && function.cppAbiPlan->symbol == "ps8";
    for (const auto& function : mir->functions())
        if (function.mangledName == "ps8") foundMirPlan = function.cppAbiPlan && function.cppAbiPlan->symbol == "ps8";
    if (!foundHirPlan || !foundMirPlan) return 69;
    std::vector<CppProjectSessions> sessionGroups(1);
    sessionGroups[0].projectFile = projectPath;
    sessionGroups[0].sessions.push_back(std::move(inventorySession));
    const CppFunctionDeclaration* variadicDeclaration = nullptr;
    for (const auto& function : *functions) if (function.name == "pvariadic") variadicDeclaration = &function;
    if (!variadicDeclaration) return 181;
    const auto variadicProjection = projectCppSemanticFunction(*variadicDeclaration, 0, "AbiProbe");
    const auto* variadicSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&variadicProjection);
    if (!variadicSymbol) return 182;
    const std::vector<std::string> scalarKeys{"builtin:int", "builtin:double", "builtin:long"};
    auto associated = planCppVariadicCall(graph, sessionGroups, lookup, *variadicSymbol, scalarKeys);
    if (!associated || associated->symbol != "pvariadic" || associated->arguments.size() != 4 ||
        associated->physicalParameters[1].type.widthOrCount != 32 ||
        associated->physicalParameters[2].type.kind != abi_plan::TypeKind::Double ||
        associated->physicalParameters[3].type.widthOrCount != 64) return 183;
    const std::vector<std::string> unsupportedKeys{"type:AbiProbe::NotPresent"};
    auto wrongSymbol = *variadicSymbol;
    wrongSymbol.cppClangUsr = "incorrect-usr";
    auto wrongSignature = *variadicSymbol;
    if (!wrongSignature.cppReturnType) return 187;
    wrongSignature.cppReturnType->canonicalSpelling = "different";
    auto wrongProject = *variadicSymbol;
    wrongProject.projectOrdinal = graph.projects().size();
    if (planCppVariadicCall(graph, sessionGroups, lookup, *variadicSymbol, unsupportedKeys) ||
        planCppVariadicCall(graph, sessionGroups, lookup, wrongSymbol, scalarKeys) ||
        planCppVariadicCall(graph, sessionGroups, lookup, wrongSignature, scalarKeys) ||
        planCppVariadicCall(graph, sessionGroups, lookup, wrongProject, scalarKeys)) return 184;
    std::string mixedKey;
    for (const auto ordinal : lookup.typeOrdinalsForName("RF"))
    {
        const auto* imported = lookup.cppImportedType(ordinal);
        if (imported && imported->clangUsr == mixedActual->recordIdentity->clangUsr)
            mixedKey = "type:" + imported->identity.key();
    }
    if (mixedKey.empty()) return 188;
    const auto liveAliases = sessionGroups[0].sessions[0]->typeAliasesSnapshot();
    if (!liveAliases) return 267;
    const CppTypeAliasDeclaration* selectedAlias = nullptr;
    for (const auto& alias : *liveAliases) if (alias.name == "RFMixedCallback") selectedAlias = &alias;
    if (!selectedAlias || !selectedAlias->clangUsr) return 268;
    nativesharp::frontend::CppImportedTypeAlias importedAlias{
        0, projectPath, 0, "x86_64-pc-windows-msvc", *selectedAlias, {}};
    const auto mappedAlias = projectCppStoredFunctionPointer(importedAlias.declaration.underlyingType, 0, "AbiProbe", &lookup);
    if (!mappedAlias) return 269;
    const auto selectedPointerPlan = planCppFunctionPointerForAlias(graph, sessionGroups, lookup, importedAlias,
        mappedAlias->nativeSharpTypeKey);
    if (!selectedPointerPlan || selectedPointerPlan->logicalParameters.size() != 1 ||
        selectedPointerPlan->logicalParameters[0].physicalCount != 2 ||
        selectedPointerPlan->physicalParameters.size() != 2 ||
        selectedPointerPlan->physicalParameters[0].type.kind != abi_plan::TypeKind::Double ||
        selectedPointerPlan->physicalParameters[1].type.widthOrCount != 32) return 270;
    auto wrongAlias = importedAlias;
    wrongAlias.referenceIndex = 1;
    if (planCppFunctionPointerForAlias(graph, sessionGroups, lookup, wrongAlias, mappedAlias->nativeSharpTypeKey) ||
        planCppFunctionPointerForAlias(graph, sessionGroups, lookup, importedAlias, "wrong-type")) return 271;
    wrongAlias = importedAlias;
    wrongAlias.declaration.clangUsr = "wrong-usr";
    if (planCppFunctionPointerForAlias(graph, sessionGroups, lookup, wrongAlias, mappedAlias->nativeSharpTypeKey)) return 272;
    wrongAlias = importedAlias;
    wrongAlias.fromPrecompiledCatalog = true;
    if (planCppFunctionPointerForAlias(graph, sessionGroups, lookup, wrongAlias, mappedAlias->nativeSharpTypeKey)) return 273;
    wrongAlias = importedAlias;
    wrongAlias.projectFile = uniqueDirectory / "different.nsproj";
    if (planCppFunctionPointerForAlias(graph, sessionGroups, lookup, wrongAlias, mappedAlias->nativeSharpTypeKey)) return 274;
    wrongAlias = importedAlias;
    wrongAlias.targetTriple = "x86_64-unknown-linux-gnu";
    if (planCppFunctionPointerForAlias(graph, sessionGroups, lookup, wrongAlias, mappedAlias->nativeSharpTypeKey)) return 275;
    sessionGroups.emplace_back();
    sessionGroups.back().projectFile = projectPath;
    if (planCppFunctionPointerForAlias(graph, sessionGroups, lookup, importedAlias, mappedAlias->nativeSharpTypeKey)) return 276;
    sessionGroups.pop_back();
    const std::vector<std::string> recordKeys{"builtin:int", mixedKey};
    auto recordCall = planCppVariadicCall(graph, sessionGroups, lookup, *variadicSymbol, recordKeys);
    if (!recordCall || recordCall->arguments.size() != 3 || recordCall->arguments[2].physicalCount != 2 ||
        recordCall->physicalParameters[2].type.kind != abi_plan::TypeKind::Double ||
        recordCall->physicalParameters[3].type.widthOrCount != 32) return 189;
    auto conflictingRecordOrigin = openRecordSession("struct RF { long long x; int y; };");
    if (!conflictingRecordOrigin) return 190;
    sessionGroups[0].sessions.push_back(std::move(conflictingRecordOrigin));
    if (planCppVariadicCall(graph, sessionGroups, lookup, *variadicSymbol, recordKeys) ||
        planCppFunctionPointerForAlias(graph, sessionGroups, lookup, importedAlias, mappedAlias->nativeSharpTypeKey)) return 191;
    sessionGroups[0].sessions.pop_back();
    CppSessionOptions duplicateOptions;
    duplicateOptions.language = CppLanguageMode::C;
    duplicateOptions.languageStandard = "c11";
    duplicateOptions.targetTriple = "x86_64-pc-windows-msvc";
    duplicateOptions.driverReferencePath = "clang.exe";
    duplicateOptions.resourceDirectory = argv[1];
    duplicateOptions.workingDirectory = argv[2];
    duplicateOptions.input = CppBufferInput{"duplicate.c", "int __attribute__((sysv_abi)) pvariadic(int, ...);"};
    auto duplicateOpened = factory.open(std::move(duplicateOptions), {});
    if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(duplicateOpened)) return 185;
    sessionGroups[0].sessions.push_back(std::move(std::get<std::unique_ptr<CppInteropSession>>(duplicateOpened)));
    if (planCppVariadicCall(graph, sessionGroups, lookup, *variadicSymbol, scalarKeys)) return 186;
    sessionGroups[0].sessions.pop_back();
    auto inventoryResult = collectCppSemanticFunctions(graph, sessionGroups);
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&inventoryResult))
        llvm::errs() << "inventory failed: " << failure->message << "\n";
    const auto* inventory = std::get_if<std::vector<CppSemanticInventoryEntry>>(&inventoryResult);
    if (!inventory) return 54;
    bool observedRecordPlan = false;
    bool scalarWasSkipped = false;
    bool pointerWasSkipped = false;
    std::shared_ptr<const abi_plan::Plan> retainedInventoryPlan;
    for (const auto& entry : *inventory) {
        if (entry.declaration.name == "p3") {
            observedRecordPlan = entry.declaration.abiPlan && entry.declaration.abiPlan->symbol == "p3";
            retainedInventoryPlan = entry.declaration.abiPlan;
        }
        if (entry.declaration.name == "ps8") scalarWasSkipped = !entry.declaration.abiPlan;
        if (entry.declaration.name == "p3ptr") pointerWasSkipped = !entry.declaration.abiPlan;
    }
    if (!observedRecordPlan || !scalarWasSkipped || !pointerWasSkipped) return 55;
    if (!sessionGroups[0].sessions[0]->close().succeeded) return 56;
    sessionGroups.clear();
    if (!retainedInventoryPlan || retainedInventoryPlan->physicalReturn.widthOrCount != 24) return 59;
    CppSessionOptions cppOptions;
    cppOptions.language = CppLanguageMode::Cpp;
    cppOptions.languageStandard = "c++20";
    cppOptions.targetTriple = "x86_64-pc-windows-msvc";
    cppOptions.driverReferencePath = "clang.exe";
    cppOptions.resourceDirectory = argv[1];
    cppOptions.workingDirectory = argv[2];
    cppOptions.input = CppBufferInput{"abi-probe.cpp", R"cpp(extern "C" int cpp_probe(int); struct T09Value { double x; int y; }; struct T09Bad { ~T09Bad(); int x; }; void record_probe(T09Value); void bad_probe(T09Bad); namespace T09Cpp { int __attribute__((sysv_abi)) sum(int,...); } namespace A { int __attribute__((sysv_abi)) ambiguous(int,...); } namespace B { int __attribute__((sysv_abi)) ambiguous(int,...); })cpp"};
    auto cppOpened = factory.open(std::move(cppOptions), {});
    if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(cppOpened)) return 43;
    auto cppSession = std::move(std::get<std::unique_ptr<CppInteropSession>>(cppOpened));
    if (!cppSession) return 44;
    if (cppSession->cRecordHasPlainReplaySyntax("Plain")) return 211;
    const auto cppFunctions = cppSession->freeFunctionsSnapshot();
    if (!cppFunctions || cppFunctions->empty() ||
        cppSession->cFunctionAbiPlan("cpp_probe", cppFunctions->front().canonicalFunctionType)) return 45;
    const CppFunctionDeclaration* cppVariadic = nullptr;
    const CppFunctionDeclaration* cppRecord = nullptr;
    const CppFunctionDeclaration* cppBad = nullptr;
    const CppFunctionDeclaration* cppAmbiguous = nullptr;
    for (const auto& f : *cppFunctions) {
        if (f.name == "sum") cppVariadic = &f;
        if (f.name == "record_probe") cppRecord = &f;
        if (f.name == "bad_probe") cppBad = &f;
        if (f.name == "ambiguous") cppAmbiguous = &f;
    }
    if (!cppVariadic || !cppRecord || !cppBad || !cppAmbiguous ||
        cppRecord->parameters.size() != 1 || cppBad->parameters.size() != 1) return 212;
    const auto cppPlan = cppSession->cVariadicCallAbiPlan(
        "sum", cppVariadic->canonicalFunctionType, {cppRecord->parameters[0].type});
    if (!cppPlan || cppPlan->symbol != cppVariadic->externalSymbol || cppPlan->arguments.size() != 2 ||
        cppPlan->arguments[1].physicalCount != 2) return 213;
    auto invalidCppRecord = cppRecord->parameters[0].type;
    if (!invalidCppRecord.recordIdentity) return 214;
    invalidCppRecord.recordIdentity->clangUsr = "incorrect-usr";
    if (cppSession->cVariadicCallAbiPlan("sum", cppVariadic->canonicalFunctionType, {invalidCppRecord}) ||
        cppSession->cVariadicCallAbiPlan("sum", cppVariadic->canonicalFunctionType, {cppBad->parameters[0].type}) ||
        cppSession->cVariadicCallAbiPlan("ambiguous", cppAmbiguous->canonicalFunctionType, {})) return 215;
    if (!cppSession->close().succeeded) return 46;
    return 0;
}
