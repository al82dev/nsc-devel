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
#include <map>
#include <string>
#include <vector>
#include "../../nativesharp-interop/pruebas-tanda-09-sysv/ClangAbiSnapshot.h"

int main() {
    const char* source = R"c(
        typedef struct R3 { unsigned char x, y, z; } R3;
        typedef struct R16 { long long x, y; } R16;
        typedef struct R24 { long long x, y, z; } R24;
        typedef struct RF { double x; int y; } RF;
        typedef struct __attribute__((packed)) RP { char c; double d; } RP;
        typedef float Vec4 __attribute__((vector_size(16)));
        R3 __attribute__((sysv_abi)) p3(R3);
        R16 __attribute__((sysv_abi)) p16(R16);
        RF __attribute__((sysv_abi)) pf(RF);
        R24 __attribute__((sysv_abi)) p24(R24);
        signed char __attribute__((sysv_abi)) ps8(signed char);
        Vec4 __attribute__((sysv_abi)) pv(Vec4);
        R24 __attribute__((sysv_abi)) pm(int, RF, R3, R24);
        int __attribute__((sysv_abi)) pvariadic(int, ...);
        RP __attribute__((sysv_abi)) pp(RP);
    )c";
    auto ast = clang::tooling::buildASTFromCodeWithArgs(source,
        {"-x", "c", "-std=c11", "-target", "x86_64-pc-windows-msvc"}, "probe.c");
    if (!ast) return 1;
    auto& context = ast->getASTContext();
    llvm::LLVMContext llvmContext;
    auto generator = clang::CreateLLVMCodeGen(ast->getDiagnostics(), "probe",
        ast->getVirtualFileSystemPtr(), ast->getHeaderSearchOpts(), ast->getPreprocessorOpts(),
        ast->getCodeGenOpts(), llvmContext);
    if (!generator) return 2;
    generator->Initialize(context);
    unsigned matched = 0;
    bool unsupportedRejected = false;
    bool variadicRejected = false;
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
        if (name != "p3" && name != "p16" && name != "pf" && name != "p24" && name != "ps8" && name != "pm" && name != "pp") continue;
        auto* physical = clang::CodeGen::convertFreeFunctionType(generator->CGM(), function);
        clang::CanQual<clang::FunctionProtoType> prototype =
            context.getCanonicalType(function->getType()).getAs<clang::FunctionProtoType>();
        if (!prototype) return 12;
        const auto& plan = clang::CodeGen::arrangeFreeFunctionType(generator->CGM(), prototype);
        if (plan.arg_size() != (name == "pm" ? 4U : 1U)) return 13;
        std::string copyError;
        auto copied = t09_probe::copyPlan(*generator, *function, copyError);
        if (!copied) { llvm::errs() << copyError << "\n"; return 19; }
        copiedPlans.emplace(name, std::move(*copied));
        if (!physical || !generator->GetAddrOfGlobal(clang::GlobalDecl(function), false)) return 3;
        auto* llvmFunction = generator->GetModule()->getFunction(generator->GetMangledName(clang::GlobalDecl(function)));
        if (!llvmFunction || llvmFunction->getFunctionType() != physical ||
            llvmFunction->getCallingConv() != llvm::CallingConv::X86_64_SysV) return 4;
        llvmFunction->print(llvm::outs());
        llvm::outs() << "\n";
        if (plan.arguments()[0].info.isDirect()) {
            auto* coercion = plan.arguments()[0].info.getCoerceToType();
            if (!coercion) return 17;
            llvm::outs() << "argumento " << name << ": ";
            coercion->print(llvm::outs());
            llvm::outs() << ", descomponible=" << plan.arguments()[0].info.getCanBeFlattened() << "\n";
        }
        if (name == "p3" && (!physical->getReturnType()->isIntegerTy(24) || physical->getNumParams() != 1 ||
                              !physical->getParamType(0)->isIntegerTy(24))) return 5;
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
    if (matched != 7 || copiedPlans.size() != 7 || !unsupportedRejected || !variadicRejected) return 7;
    generator.reset();
    ast.reset();
    const auto& p3 = copiedPlans.at("p3");
    if (p3.targetTriple.find("x86_64-pc-windows-msvc") != 0 || p3.dataLayout.empty()) return 28;
    if (p3.physicalReturn.kind != t09_probe::TypeKind::Integer || p3.physicalReturn.widthOrCount != 24 ||
        p3.logicalParameters.size() != 1 || p3.logicalParameters[0].passing != t09_probe::Passing::Direct ||
        !p3.logicalParameters[0].coercion || p3.logicalParameters[0].coercion->widthOrCount != 24) return 20;
    const auto& p16 = copiedPlans.at("p16");
    if (p16.physicalReturn.kind != t09_probe::TypeKind::Structure || p16.physicalReturn.elements.size() != 2 ||
        p16.physicalParameters.size() != 2 || p16.physicalParameters[0].type.widthOrCount != 64 ||
        p16.physicalParameters[1].type.widthOrCount != 64) return 21;
    const auto& pf = copiedPlans.at("pf");
    if (pf.physicalParameters.size() != 2 || pf.physicalParameters[0].type.kind != t09_probe::TypeKind::Double ||
        pf.physicalParameters[1].type.widthOrCount != 32) return 22;
    const auto& p24 = copiedPlans.at("p24");
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
    return 0;
}
