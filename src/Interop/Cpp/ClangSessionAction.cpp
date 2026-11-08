#include "ClangSessionSupport.h"

#include <clang/Basic/FileManager.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Serialization/PCHContainerOperations.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>

#include <utility>

namespace nativesharp::interop::detail
{
namespace
{
struct PreprocessingTrace
{
    llvm::SHA256 hash;
    void record(llvm::StringRef kind, llvm::StringRef value)
    {
        hash.update(std::to_string(kind.size())); hash.update(":"); hash.update(kind);
        hash.update(std::to_string(value.size())); hash.update(":"); hash.update(value);
    }
};

class TraceCallbacks final : public clang::PPCallbacks
{
public:
    explicit TraceCallbacks(std::shared_ptr<PreprocessingTrace> trace) : trace_(std::move(trace)) {}
    void If(clang::SourceLocation, clang::SourceRange, ConditionValueKind value) override
    { trace_->record("if", std::to_string(static_cast<int>(value))); }
    void Elif(clang::SourceLocation, clang::SourceRange, ConditionValueKind value,
              clang::SourceLocation) override
    { trace_->record("elif", std::to_string(static_cast<int>(value))); }
    void Ifdef(clang::SourceLocation, const clang::Token&, const clang::MacroDefinition& macro) override
    { trace_->record("ifdef", macro.getMacroInfo() ? "defined" : "undefined"); }
    void Ifndef(clang::SourceLocation, const clang::Token&, const clang::MacroDefinition& macro) override
    { trace_->record("ifndef", macro.getMacroInfo() ? "defined" : "undefined"); }
    void Elifdef(clang::SourceLocation, const clang::Token&, const clang::MacroDefinition& macro) override
    { trace_->record("elifdef", macro.getMacroInfo() ? "defined" : "undefined"); }
    void Elifdef(clang::SourceLocation, clang::SourceRange, clang::SourceLocation) override
    { trace_->record("elifdef", "skipped"); }
    void Elifndef(clang::SourceLocation, const clang::Token&, const clang::MacroDefinition& macro) override
    { trace_->record("elifndef", macro.getMacroInfo() ? "defined" : "undefined"); }
    void Elifndef(clang::SourceLocation, clang::SourceRange, clang::SourceLocation) override
    { trace_->record("elifndef", "skipped"); }
    void HasInclude(clang::SourceLocation, llvm::StringRef name, bool angled,
                    clang::OptionalFileEntryRef file, clang::SrcMgr::CharacteristicKind) override
    {
        trace_->record(angled ? "has-include-angled" : "has-include-quoted", name);
        trace_->record("has-include-result", file ? file->getName() : llvm::StringRef());
    }
    void HasEmbed(clang::SourceLocation, llvm::StringRef name, bool angled,
                  clang::OptionalFileEntryRef file) override
    {
        trace_->record(angled ? "has-embed-angled" : "has-embed-quoted", name);
        trace_->record("has-embed-result", file ? file->getName() : llvm::StringRef());
    }
private:
    std::shared_ptr<PreprocessingTrace> trace_;
};

class SessionFrontendAction final : public clang::SyntaxOnlyAction
{
public:
    explicit SessionFrontendAction(std::shared_ptr<PreprocessingTrace> trace) : trace_(std::move(trace)) {}
protected:
    bool BeginSourceFileAction(clang::CompilerInstance& compiler) override
    {
        if (!clang::SyntaxOnlyAction::BeginSourceFileAction(compiler)) return false;
        if (!trace_) return true;
        auto& preprocessor = compiler.getPreprocessor();
        preprocessor.addPPCallbacks(std::make_unique<TraceCallbacks>(trace_));
        preprocessor.setTokenWatcher([trace = trace_, &preprocessor](const clang::Token& token) {
            if (token.isAnnotation()) return;
            trace->record("token-kind", std::to_string(static_cast<int>(token.getKind())));
            trace->record("token-spelling", preprocessor.getSpelling(token));
        });
        return true;
    }
private:
    std::shared_ptr<PreprocessingTrace> trace_;
};

class SessionAction final : public clang::tooling::ToolAction
{
public:
    SessionAction(std::unique_ptr<clang::ASTUnit>& ast, clang::FrontendAction* actionForTest,
                  std::shared_ptr<PreprocessingTrace> trace)
        : ast_(ast), actionForTest_(actionForTest), trace_(std::move(trace)) {}

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
        SessionFrontendAction frontend(trace_);
        if (clang::ASTUnit::LoadFromCompilerInvocationAction(
                std::move(invocation), std::move(pch), nullptr, std::move(diagnostics),
                actionForTest_ ? actionForTest_ : &frontend, unit.get(), true) != unit.get()) return false;
        ast_ = std::move(unit);
        return true;
    }

private:
    std::unique_ptr<clang::ASTUnit>& ast_;
    clang::FrontendAction* actionForTest_;
    std::shared_ptr<PreprocessingTrace> trace_;
};
}

std::unique_ptr<clang::ASTUnit> runClangSessionAction(
    std::string_view source, const std::vector<std::string>& arguments,
    std::string_view inputPath, std::string_view driverPath,
    clang::DiagnosticConsumer* consumer, llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem,
    CppCancellationToken cancellation, clang::FrontendAction* actionForTest,
    std::optional<basic::Sha256Digest>* preprocessingFingerprint)
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
    auto trace = preprocessingFingerprint && !actionForTest ? std::make_shared<PreprocessingTrace>() : nullptr;
    if (preprocessingFingerprint) preprocessingFingerprint->reset();
    SessionAction action(ast, actionForTest, trace);
    clang::tooling::ToolInvocation invocation(std::move(args), &action, files.get(),
        std::make_shared<clang::PCHContainerOperations>());
    invocation.setDiagnosticConsumer(consumer);
    if (!invocation.run()) return nullptr;
    if (trace) *preprocessingFingerprint = trace->hash.result();
    return ast;
}
}
