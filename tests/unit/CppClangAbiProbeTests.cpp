#include "nativesharp/Interop/CppAbiPlanQueries.h"
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
#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppInteropSession.h"
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
        typedef struct R17B { unsigned char x[17]; } R17B;
        typedef struct R20B { unsigned char x[20]; } R20B;
        typedef struct R32B { unsigned char x[32]; } R32B;
        typedef struct RF { double x; int y; } RF;
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
    for (const auto* decl : context.getTranslationUnitDecl()->decls()) {
        const auto* function = llvm::dyn_cast<clang::FunctionDecl>(decl);
        if (!function || !function->getIdentifier()) continue;
        const std::string name = function->getNameAsString();
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
            name != "pp" && name != "pvec" && name != "pvoid" && name != "phetero" && name != "pscalar" && name != "pscalarfirst" && name != "pmixed8" && name != "pscalarfloat" && name != "pscalardouble" && name != "pfloatfirst" && name != "pdoublefirst" && name != "pfloats" && name != "pfloatstack" && name != "precordptr" && name != "ppairmix" && name != "ppairints") continue;
        if (name == "p3" && copiedPlans.contains(name)) continue;
        auto* physical = clang::CodeGen::convertFreeFunctionType(generator->CGM(), function);
        clang::CanQual<clang::FunctionProtoType> prototype =
            context.getCanonicalType(function->getType()).getAs<clang::FunctionProtoType>();
        if (!prototype) return 12;
        const auto& plan = clang::CodeGen::arrangeFreeFunctionType(generator->CGM(), prototype);
        if (plan.arg_size() != ((name == "pm" || name == "ppairints") ? 4U : name == "pvoid" ? 0U : (name == "pfloats" || name == "ppairmix") ? 3U : name == "pfloatstack" ? 10U : (name == "ptwo" || name == "phetero" || name == "pscalar" || name == "pscalarfirst" || name == "pscalarfloat" || name == "pscalardouble" || name == "pfloatfirst" || name == "pdoublefirst" || name == "precordptr") ? 2U : (name == "peight" || name == "pmixed8") ? 8U : 1U)) return 13;
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
    if (matched != 65 || copiedPlans.size() != 65 || !unsupportedRejected || !variadicRejected ||
        !noPrototypeRejected || !attributeRejected) return 7;
    generator.reset();
    ast.reset();
    llvmContext.reset();
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
    if (!session->close().succeeded || session->cFunctionAbiPlan("p3", p3Type)) return 40;
    session.reset();
    if (sessionPlan->symbol != "p3" || sessionPlan->physicalReturn.widthOrCount != 24 ||
        vectorcallPlan->symbol != "\1pvec@@8" ||
        vectorcallPlan->callingConvention != llvm::CallingConv::X86_VectorCall ||
        aggregatePlan->physicalParameters.size() != 2 || !aggregatePlan->physicalParameters[1].byValue ||
        mixedPlan->logicalParameters.size() != 4 || mixedPlan->logicalParameters[1].physicalCount != 2 ||
        projected->cppAbiPlan->symbol != "ps8" || projectedRecord->cppAbiPlan->symbol != "p3ptr") return 41;
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
    cppOptions.input = CppBufferInput{"abi-probe.cpp", "extern \"C\" int cpp_probe(int);"};
    auto cppOpened = factory.open(std::move(cppOptions), {});
    if (!std::holds_alternative<std::unique_ptr<CppInteropSession>>(cppOpened)) return 43;
    auto cppSession = std::move(std::get<std::unique_ptr<CppInteropSession>>(cppOpened));
    if (!cppSession) return 44;
    const auto cppFunctions = cppSession->freeFunctionsSnapshot();
    if (!cppFunctions || cppFunctions->empty() ||
        cppSession->cFunctionAbiPlan("cpp_probe", cppFunctions->front().canonicalFunctionType)) return 45;
    if (!cppSession->close().succeeded) return 46;
    return 0;
}
