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
#include <string>
#include <vector>

int main() {
    const char* source = R"c(
        typedef struct R3 { unsigned char x, y, z; } R3;
        typedef struct R16 { long long x, y; } R16;
        typedef struct R24 { long long x, y, z; } R24;
        typedef struct RF { double x; int y; } RF;
        R3 __attribute__((sysv_abi)) p3(R3);
        R16 __attribute__((sysv_abi)) p16(R16);
        RF __attribute__((sysv_abi)) pf(RF);
        R24 __attribute__((sysv_abi)) p24(R24);
        signed char __attribute__((sysv_abi)) ps8(signed char);
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
    for (const auto* decl : context.getTranslationUnitDecl()->decls()) {
        const auto* function = llvm::dyn_cast<clang::FunctionDecl>(decl);
        if (!function || !function->getIdentifier()) continue;
        const std::string name = function->getNameAsString();
        if (name != "p3" && name != "p16" && name != "pf" && name != "p24" && name != "ps8") continue;
        auto* physical = clang::CodeGen::convertFreeFunctionType(generator->CGM(), function);
        if (!physical || !generator->GetAddrOfGlobal(clang::GlobalDecl(function), false)) return 3;
        auto* llvmFunction = generator->GetModule()->getFunction(generator->GetMangledName(clang::GlobalDecl(function)));
        if (!llvmFunction || llvmFunction->getFunctionType() != physical ||
            llvmFunction->getCallingConv() != llvm::CallingConv::X86_64_SysV) return 4;
        llvmFunction->print(llvm::outs());
        llvm::outs() << "\n";
        if (name == "p3" && (!physical->getReturnType()->isIntegerTy(24) || physical->getNumParams() != 1 ||
                              !physical->getParamType(0)->isIntegerTy(24))) return 5;
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
        if (name == "ps8" && (!llvmFunction->hasRetAttribute(llvm::Attribute::SExt) ||
            !llvmFunction->hasParamAttribute(0, llvm::Attribute::SExt))) return 9;
        ++matched;
    }
    return matched == 5 ? 0 : 7;
}
