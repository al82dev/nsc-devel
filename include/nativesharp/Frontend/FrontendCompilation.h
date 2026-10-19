#pragma once

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/CFG/CfgArena.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Dominance/DominanceArena.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/Frontend/StageBoundaryVerifier.h"
#include "nativesharp/HIR/HirArena.h"
#include "nativesharp/HIR/HirNode.h"
#include "nativesharp/Lexer/Token.h"
#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/Passes/OptimizationArena.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"
#include "nativesharp/SSA/SsaArena.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/SSA/SsaModule.h"
#include "nativesharp/Sema/TypeContext.h"
#include "nativesharp/Source/SourceManager.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
class FrontendCompilation final
{
public:
    FrontendCompilation();
    FrontendCompilation(const FrontendCompilation&) = delete;
    FrontendCompilation& operator=(const FrontendCompilation&) = delete;
    ~FrontendCompilation();

    bool compileFile(const std::filesystem::path& path, std::string& loadError, bool requireMain = false);
    bool compileBuffer(std::filesystem::path path, std::string buffer, bool requireMain = false);

    const source::SourceManager& sourceManager() const noexcept;
    diagnostics::DiagnosticEngine& diagnostics() noexcept;
    const diagnostics::DiagnosticEngine& diagnostics() const noexcept;
    const std::vector<lexer::Token>& tokens() const noexcept;
    ast::AstNode* syntaxTree() const noexcept;
    const sema::SemanticAnalyzer* semanticAnalyzer() const noexcept;
    const hir::HirNode* hirRoot() const noexcept;
    const std::string& hirError() const noexcept;
    const mir::MirModule* mirModule() const noexcept;
    const std::string& mirError() const noexcept;
    const cfg::CfgModule* cfgModule() const noexcept;
    const std::string& cfgError() const noexcept;
    const dominance::DominanceModule* dominanceModule() const noexcept;
    const std::string& dominanceError() const noexcept;
    const ssa::SsaModule* ssaModule() const noexcept;
    const std::string& ssaError() const noexcept;
    const passes::OptimizationModule* optimizationModule() const noexcept;
    const std::string& optimizationError() const noexcept;
    const std::vector<StageBoundary>& verifiedStageBoundaries() const noexcept;
    FileId primaryFileId() const noexcept;

private:
    bool run(FileId fileId, bool requireMain);
    bool buildHirMirAndCfg(StageBoundaryVerifier& boundaryVerifier);
    bool buildDominanceSsaAndOptimization(StageBoundaryVerifier& boundaryVerifier);

    source::SourceManager sourceManager_;
    diagnostics::DiagnosticEngine diagnostics_;
    std::vector<lexer::Token> tokens_;
    ast::AstArena arena_;
    ast::AstNode* syntaxTree_ = nullptr;
    sema::TypeContext typeContext_;
    target::TargetDataLayout dataLayout_;
    sema::SemanticAnalyzer* semanticAnalyzer_ = nullptr;
    hir::HirArena hirArena_;
    hir::HirNode* hirRoot_ = nullptr;
    std::string hirError_;
    mir::MirArena mirArena_;
    mir::MirModule* mirModule_ = nullptr;
    std::string mirError_;
    cfg::CfgArena cfgArena_;
    cfg::CfgModule* cfgModule_ = nullptr;
    std::string cfgError_;
    dominance::DominanceArena dominanceArena_;
    dominance::DominanceModule* dominanceModule_ = nullptr;
    std::string dominanceError_;
    ssa::SsaArena ssaArena_;
    ssa::SsaModule* ssaModule_ = nullptr;
    std::string ssaError_;
    passes::OptimizationArena optimizationArena_;
    passes::OptimizationModule* optimizationModule_ = nullptr;
    std::string optimizationError_;
    std::vector<StageBoundary> verifiedStageBoundaries_;
    FileId primaryFileId_;
};
}
