#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Frontend/ComptimeEngine.h"

#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/CFG/CfgBuilder.h"
#include "nativesharp/CFG/CfgVerifier.h"
#include "nativesharp/Dominance/DominanceBuilder.h"
#include "nativesharp/Dominance/DominanceVerifier.h"
#include "nativesharp/MIR/MirBuilder.h"
#include "nativesharp/MIR/MirVerifier.h"
#include "nativesharp/HIR/HirBuilder.h"
#include "nativesharp/HIR/HirVerifier.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/SSA/SsaBuilder.h"
#include "nativesharp/SSA/SsaVerifier.h"
#include "nativesharp/Passes/OptimizationBuilder.h"
#include "nativesharp/Passes/PassManager.h"

#include <utility>

namespace nativesharp::frontend
{
FrontendCompilation::FrontendCompilation()
    : dataLayout_(target::TargetDataLayout::WindowsX64())
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
    return run(sourceManager_.addBuffer(path, std::move(comptime.source)), requireMain);
}

bool FrontendCompilation::compileBuffer(
    std::filesystem::path path,
    std::string buffer,
    bool requireMain)
{
    ComptimeResult comptime = ComptimeEngine().execute(buffer);
    if (!comptime.success) return false;
    return run(sourceManager_.addBuffer(std::move(path), std::move(comptime.source)), requireMain);
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
FileId FrontendCompilation::primaryFileId() const noexcept { return primaryFileId_; }

bool FrontendCompilation::run(FileId fileId, bool requireMain)
{
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

    parser::Parser parser(tokens_, arena_, diagnostics_);
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

    hir::HirVerifier verifier;
    if (!verifier.verify(hirRoot_))
    {
        hirError_ = verifier.error();
        return false;
    }

    mir::MirBuilder mirBuilder(mirArena_);
    mirModule_ = mirBuilder.build(hirRoot_);
    if (mirModule_ == nullptr)
    {
        mirError_ = mirBuilder.error();
        return false;
    }

    mir::MirVerifier mirVerifier;
    if (!mirVerifier.verify(mirModule_))
    {
        mirError_ = mirVerifier.error();
        return false;
    }

    cfg::CfgBuilder cfgBuilder(cfgArena_);
    cfgModule_ = cfgBuilder.build(mirModule_);
    if (cfgModule_ == nullptr)
    {
        cfgError_ = cfgBuilder.error();
        return false;
    }

    cfg::CfgVerifier cfgVerifier;
    if (!cfgVerifier.verify(cfgModule_))
    {
        cfgError_ = cfgVerifier.error();
        return false;
    }

    dominance::DominanceBuilder dominanceBuilder(dominanceArena_);
    dominanceModule_ = dominanceBuilder.build(cfgModule_);
    if (dominanceModule_ == nullptr)
    {
        dominanceError_ = dominanceBuilder.error();
        return false;
    }

    dominance::DominanceVerifier dominanceVerifier;
    if (!dominanceVerifier.verify(dominanceModule_))
    {
        dominanceError_ = dominanceVerifier.error();
        return false;
    }

    ssa::SsaBuilder ssaBuilder(ssaArena_);
    ssaModule_ = ssaBuilder.build(mirModule_, cfgModule_, dominanceModule_);
    if (ssaModule_ == nullptr)
    {
        ssaError_ = ssaBuilder.error();
        return false;
    }

    ssa::SsaVerifier ssaVerifier;
    if (!ssaVerifier.verify(ssaModule_))
    {
        ssaError_ = ssaVerifier.error();
        return false;
    }

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

    return true;
}
}
