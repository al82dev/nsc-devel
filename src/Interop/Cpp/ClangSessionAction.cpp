#include "ClangSessionSupport.h"

#include <clang/Basic/FileManager.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Serialization/PCHContainerOperations.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/MemoryBuffer.h>

#include <utility>

namespace nativesharp::interop::detail
{
namespace
{
class SessionFrontendAction final : public clang::SyntaxOnlyAction
{
};

class SessionAction final : public clang::tooling::ToolAction
{
public:
    explicit SessionAction(std::unique_ptr<clang::ASTUnit>& ast) : ast_(ast) {}

    bool runInvocation(std::shared_ptr<clang::CompilerInvocation> invocation,
                       clang::FileManager* files,
                       std::shared_ptr<clang::PCHContainerOperations> pch,
                       clang::DiagnosticConsumer* consumer) override
    {
        auto diagnostics = clang::CompilerInstance::createDiagnostics(
            files->getVirtualFileSystem(), invocation->getDiagnosticOpts(), consumer, false);
        auto unit = clang::ASTUnit::create(invocation, nullptr, diagnostics,
                                           clang::CaptureDiagsKind::None, false);
        if (!unit) return false;
        unit->getFileManager().setVirtualFileSystem(files->getVirtualFileSystemPtr());
        SessionFrontendAction frontend;
        if (clang::ASTUnit::LoadFromCompilerInvocationAction(
                std::move(invocation), std::move(pch), nullptr, std::move(diagnostics),
                &frontend, unit.get(), true) != unit.get()) return false;
        ast_ = std::move(unit);
        return true;
    }

private:
    std::unique_ptr<clang::ASTUnit>& ast_;
};
}

std::unique_ptr<clang::ASTUnit> runClangSessionAction(
    std::string_view source, const std::vector<std::string>& arguments,
    std::string_view inputPath, std::string_view driverPath,
    clang::DiagnosticConsumer* consumer, llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem,
    CppCancellationToken cancellation)
{
    const std::string input(inputPath);
    auto overlay = llvm::makeIntrusiveRefCnt<llvm::vfs::OverlayFileSystem>(std::move(filesystem));
    auto memory = llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>();
    overlay->pushOverlay(memory);
    memory->addFile(input, 0, llvm::MemoryBuffer::getMemBufferCopy(
        llvm::StringRef(source.data(), source.size())));
    auto cancellable = makeCancellationFileSystem(overlay, std::move(cancellation));
    auto files = llvm::makeIntrusiveRefCnt<clang::FileManager>(clang::FileSystemOptions(), cancellable);

    std::vector<std::string> args{std::string(driverPath), "-fsyntax-only"};
    const auto adjusted = clang::tooling::getClangStripDependencyFileAdjuster()(arguments, input);
    args.insert(args.end(), adjusted.begin(), adjusted.end());
    args.push_back(input);

    std::unique_ptr<clang::ASTUnit> ast;
    SessionAction action(ast);
    clang::tooling::ToolInvocation invocation(std::move(args), &action, files.get(),
        std::make_shared<clang::PCHContainerOperations>());
    invocation.setDiagnosticConsumer(consumer);
    if (!invocation.run()) return nullptr;
    return ast;
}
}
