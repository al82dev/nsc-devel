#include "nativesharp/Interop/CppClangSessionFactory.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTStructuralEquivalence.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/DeclGroup.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/ODRHash.h>
#include <clang/AST/RecordLayout.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/ModuleBuilder.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <set>
#include <string_view>
#include <utility>

namespace nativesharp::interop
{
namespace
{
std::string utf8(const std::filesystem::path& path)
{
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

std::filesystem::path pathFromUtf8(const char* text)
{
    const std::string_view bytes(text);
    return std::filesystem::path(std::u8string(
        reinterpret_cast<const char8_t*>(bytes.data()), bytes.size()));
}

}
}
