#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppSessionOptions.h"
#include "nativesharp/Interop/CppInteropSessionFactory.h"

#include <clang/Basic/Diagnostic.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clang { class ASTUnit; class FrontendAction; }

namespace nativesharp::interop::detail
{
std::vector<std::string> buildClangInvocationArguments(const CppSessionOptions& options);
llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> makeCancellationFileSystem(
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> underlying, CppCancellationToken cancellation);
// EN: T19-003. Preprocesses the source like runClangSessionAction parses it and returns Clang's `-E -dD` output, with
//     line markers and the macro definitions kept; nullopt when Clang reports an error.
// ES: T19-003. Preprocesa la fuente como runClangSessionAction la analiza y devuelve la salida `-E -dD` de Clang, con
//     marcas de linea y las definiciones de macros; nullopt si Clang informa un error.
std::optional<std::string> preprocessClangSessionSource(
    std::string_view source, const std::vector<std::string>& arguments,
    std::string_view inputPath, std::string_view driverPath,
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem);
std::unique_ptr<clang::ASTUnit> runClangSessionAction(
    std::string_view source, const std::vector<std::string>& arguments,
    std::string_view inputPath, std::string_view driverPath,
    clang::DiagnosticConsumer* consumer, llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem,
    CppCancellationToken cancellation, clang::FrontendAction* actionForTest = nullptr,
    std::optional<basic::Sha256Digest>* preprocessingFingerprint = nullptr);

class DiagnosticCollector final : public clang::DiagnosticConsumer
{
public:
    explicit DiagnosticCollector(std::vector<CppSessionDiagnostic>& diagnostics);
    void HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& info) override;

private:
    std::vector<CppSessionDiagnostic>& diagnostics_;
};
}
