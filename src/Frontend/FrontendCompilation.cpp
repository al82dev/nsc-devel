#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Frontend/ComptimeEngine.h"

#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/CFG/CfgBuilder.h"
#include "nativesharp/Dominance/DominanceBuilder.h"
#include "nativesharp/MIR/MirBuilder.h"
#include "nativesharp/HIR/HirBuilder.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/SSA/SsaBuilder.h"
#include "nativesharp/Passes/OptimizationBuilder.h"
#include "nativesharp/Passes/PassManager.h"

#include <utility>

namespace nativesharp::frontend
{
FrontendCompilation::FrontendCompilation(language::LanguageEdition languageEdition)
    : dataLayout_(target::TargetDataLayout::WindowsX64()),
      languageEdition_(languageEdition)
{
}

FrontendCompilation::~FrontendCompilation()
{
    delete semanticAnalyzer_;
}

bool FrontendCompilation::compileFile(
    const std::filesystem::path& path,
    std::string& loadError,
    bool requireMain)
{
    std::optional<std::string> buffer = source::SourceManager::readFileBuffer(path, loadError);
    if (!buffer.has_value())
    {
        return false;
    }
    ComptimeResult comptime = ComptimeEngine().execute(*buffer);
    if (!comptime.success)
    {
        loadError = comptime.error;
        return false;
    }
    const FileId fileId = sourceManager_.addBuffer(path, std::move(comptime.source));
    for (const ComptimeAuthoredDiagnostic& diagnostic : comptime.authoredDiagnostics)
    {
        const auto severity = diagnostic.severity == ComptimeDiagnosticSeverity::Error
            ? diagnostics::DiagnosticSeverity::Error
            : diagnostic.severity == ComptimeDiagnosticSeverity::Warning
                ? diagnostics::DiagnosticSeverity::Warning : diagnostics::DiagnosticSeverity::Note;
        diagnostics_.report(diagnostics::DiagnosticId::ExpectedDeclaration, severity,
            SourceRange(SourceLocation(fileId, static_cast<std::uint32_t>(diagnostic.offset)),
                SourceLocation(fileId, static_cast<std::uint32_t>(diagnostic.offset + diagnostic.length))),
            "[" + diagnostic.code + "] " + diagnostic.message);
    }
    return run(fileId, requireMain);
}

bool FrontendCompilation::compileBuffer(
    std::filesystem::path path,
    std::string buffer,
    bool requireMain)
{
    ComptimeResult comptime = ComptimeEngine().execute(buffer);
    if (!comptime.success) return false;
    const FileId fileId = sourceManager_.addBuffer(std::move(path), std::move(comptime.source));
    for (const ComptimeAuthoredDiagnostic& diagnostic : comptime.authoredDiagnostics)
    {
        const auto severity = diagnostic.severity == ComptimeDiagnosticSeverity::Error
            ? diagnostics::DiagnosticSeverity::Error
            : diagnostic.severity == ComptimeDiagnosticSeverity::Warning
                ? diagnostics::DiagnosticSeverity::Warning : diagnostics::DiagnosticSeverity::Note;
        diagnostics_.report(diagnostics::DiagnosticId::ExpectedDeclaration, severity,
            SourceRange(SourceLocation(fileId, static_cast<std::uint32_t>(diagnostic.offset)),
                SourceLocation(fileId, static_cast<std::uint32_t>(diagnostic.offset + diagnostic.length))),
            "[" + diagnostic.code + "] " + diagnostic.message);
    }
    return run(fileId, requireMain);
}

const source::SourceManager& FrontendCompilation::sourceManager() const noexcept { return sourceManager_; }
diagnostics::DiagnosticEngine& FrontendCompilation::diagnostics() noexcept { return diagnostics_; }
const diagnostics::DiagnosticEngine& FrontendCompilation::diagnostics() const noexcept { return diagnostics_; }
const std::vector<lexer::Token>& FrontendCompilation::tokens() const noexcept { return tokens_; }
ast::AstNode* FrontendCompilation::syntaxTree() const noexcept { return syntaxTree_; }
const sema::SemanticAnalyzer* FrontendCompilation::semanticAnalyzer() const noexcept { return semanticAnalyzer_; }
const hir::HirNode* FrontendCompilation::hirRoot() const noexcept { return hirRoot_; }
const std::string& FrontendCompilation::hirError() const noexcept { return hirError_; }
const mir::MirModule* FrontendCompilation::mirModule() const noexcept { return mirModule_; }
const std::string& FrontendCompilation::mirError() const noexcept { return mirError_; }
const cfg::CfgModule* FrontendCompilation::cfgModule() const noexcept { return cfgModule_; }
const std::string& FrontendCompilation::cfgError() const noexcept { return cfgError_; }
const dominance::DominanceModule* FrontendCompilation::dominanceModule() const noexcept { return dominanceModule_; }
const std::string& FrontendCompilation::dominanceError() const noexcept { return dominanceError_; }
const ssa::SsaModule* FrontendCompilation::ssaModule() const noexcept { return ssaModule_; }
const std::string& FrontendCompilation::ssaError() const noexcept { return ssaError_; }
const passes::OptimizationModule* FrontendCompilation::optimizationModule() const noexcept
{
    return optimizationModule_;
}
const std::string& FrontendCompilation::optimizationError() const noexcept
{
    return optimizationError_;
}
const std::vector<StageBoundary>& FrontendCompilation::verifiedStageBoundaries() const noexcept
{
    return verifiedStageBoundaries_;
}
language::LanguageEdition FrontendCompilation::languageEdition() const noexcept
{
    return languageEdition_;
}
FileId FrontendCompilation::primaryFileId() const noexcept { return primaryFileId_; }

bool FrontendCompilation::run(FileId fileId, bool requireMain)
{
    verifiedStageBoundaries_.clear();
    StageBoundaryVerifier boundaryVerifier;
    primaryFileId_ = fileId;
    const source::SourceFile* sourceFile = sourceManager_.getFile(fileId);
    if (sourceFile == nullptr)
    {
        return false;
    }

    lexer::Lexer lexer(*sourceFile, diagnostics_);
    tokens_ = lexer.lexAll();
    if (diagnostics_.hasErrors())
    {
        return false;
    }

    parser::Parser parser(tokens_, arena_, diagnostics_, {}, languageEdition_);
    syntaxTree_ = parser.parseCompilationUnit();
    if (diagnostics_.hasErrors())
    {
        return false;
    }

    semanticAnalyzer_ = new sema::SemanticAnalyzer(typeContext_, dataLayout_, diagnostics_);
    const bool semanticsOk = semanticAnalyzer_->analyze(syntaxTree_);
    if (!semanticsOk)
    {
        return false;
    }
    if (requireMain && !semanticAnalyzer_->validateExecutableEntryPoint(syntaxTree_))
    {
        return false;
    }

    hir::HirBuilder hirBuilder(hirArena_, typeContext_, semanticAnalyzer_->model());
    hirRoot_ = hirBuilder.build(syntaxTree_);
    if (hirRoot_ == nullptr)
    {
        hirError_ = hirBuilder.error();
        return false;
    }

    if (!boundaryVerifier.verify(StageBoundary::HirToMir, hirRoot_))
    {
        hirError_ = boundaryVerifier.error();
        return false;
    }
    verifiedStageBoundaries_.push_back(StageBoundary::HirToMir);

    mir::MirBuilder mirBuilder(mirArena_);
    mirModule_ = mirBuilder.build(hirRoot_);
    if (mirModule_ == nullptr)
    {
        mirError_ = mirBuilder.error();
        return false;
    }

    if (!boundaryVerifier.verify(StageBoundary::MirToCfg, mirModule_))
    {
        mirError_ = boundaryVerifier.error();
        return false;
    }
    verifiedStageBoundaries_.push_back(StageBoundary::MirToCfg);

    cfg::CfgBuilder cfgBuilder(cfgArena_);
    cfgModule_ = cfgBuilder.build(mirModule_);
    if (cfgModule_ == nullptr)
    {
        cfgError_ = cfgBuilder.error();
        return false;
    }

    if (!boundaryVerifier.verify(StageBoundary::CfgToDominance, cfgModule_))
    {
        cfgError_ = boundaryVerifier.error();
        return false;
    }
    verifiedStageBoundaries_.push_back(StageBoundary::CfgToDominance);

    dominance::DominanceBuilder dominanceBuilder(dominanceArena_);
    dominanceModule_ = dominanceBuilder.build(cfgModule_);
    if (dominanceModule_ == nullptr)
    {
        dominanceError_ = dominanceBuilder.error();
        return false;
    }

    if (!boundaryVerifier.verify(StageBoundary::DominanceToSsa, dominanceModule_))
    {
        dominanceError_ = boundaryVerifier.error();
        return false;
    }
    verifiedStageBoundaries_.push_back(StageBoundary::DominanceToSsa);

    ssa::SsaBuilder ssaBuilder(ssaArena_);
    ssaModule_ = ssaBuilder.build(mirModule_, cfgModule_, dominanceModule_);
    if (ssaModule_ == nullptr)
    {
        ssaError_ = ssaBuilder.error();
        return false;
    }

    if (!boundaryVerifier.verify(StageBoundary::SsaToOptimization, ssaModule_))
    {
        ssaError_ = boundaryVerifier.error();
        return false;
    }
    verifiedStageBoundaries_.push_back(StageBoundary::SsaToOptimization);

    passes::OptimizationBuilder optimizationBuilder(optimizationArena_);
    optimizationModule_ = optimizationBuilder.build(ssaModule_);
    if (optimizationModule_ == nullptr)
    {
        optimizationError_ = optimizationBuilder.error();
        return false;
    }

    passes::PassManager passManager(optimizationArena_);
    passManager.addDefaultPipeline();
    if (!passManager.run(optimizationModule_))
    {
        optimizationError_ = passManager.error();
        return false;
    }

    if (!boundaryVerifier.verify(StageBoundary::OptimizationToBackend, optimizationModule_))
    {
        optimizationError_ = boundaryVerifier.error();
        return false;
    }
    verifiedStageBoundaries_.push_back(StageBoundary::OptimizationToBackend);

    return true;
}
}
