#include "Internal/DriverInternal.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

bool renderFrontendSourceAndSyntax(std::string_view action, frontend::FrontendCompilation &compilation,
                                   std::ostream &output, std::ostream &error)
{
    const source::SourceFile *sourceFile = compilation.sourceManager().getFile(compilation.primaryFileId());
    if (action == "--dump-source" && sourceFile != nullptr)
    {
        output << "file: " << sourceFile->displayPath() << '\n'
               << "bytes: " << sourceFile->size() << '\n'
               << "lines: " << sourceFile->lineCount() << '\n';
        for (std::uint32_t line = 1; line <= sourceFile->lineCount(); ++line)
        {
            output << line << ": " << sourceFile->lineText(line) << '\n';
        }
    }
    else if (action == "--dump-tokens")
    {
        lexer::TokenDumper dumper;
        dumper.dump(compilation.tokens(), output);
    }
    else if (action == "--dump-ast" && compilation.syntaxTree() != nullptr)
    {
        ast::AstDumper dumper;
        dumper.dump(compilation.syntaxTree(), output);
    }
    else if (action == "--dump-symbols" && compilation.semanticAnalyzer() != nullptr)
    {
        sema::SymbolDumper dumper;
        dumper.dump(compilation.semanticAnalyzer()->globalSymbols(), output);
    }
    else if (action == "--dump-sema" && compilation.semanticAnalyzer() != nullptr &&
             compilation.syntaxTree() != nullptr)
    {
        sema::SemanticDumper dumper;
        dumper.dump(compilation.syntaxTree(), compilation.semanticAnalyzer()->model(), output);
    }
    else if (action == "--dump-hir" && compilation.hirRoot() != nullptr)
    {
        hir::HirDumper dumper;
        dumper.dump(compilation.hirRoot(), output);
    }
    else if (action == "--verify-hir" && compilation.hirRoot() != nullptr)
    {
        hir::HirVerifier verifier;
        if (!verifier.verify(compilation.hirRoot()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return false;
        }
        output << "HIR verification: ok\n";
    }
    return true;
}

bool renderFrontendMirAndControlFlow(std::string_view action, frontend::FrontendCompilation &compilation,
                                     std::ostream &output, std::ostream &error)
{
    if (action == "--dump-mir" && compilation.mirModule() != nullptr)
    {
        mir::MirDumper dumper;
        dumper.dump(compilation.mirModule(), output);
    }
    else if (action == "--verify-mir" && compilation.mirModule() != nullptr)
    {
        mir::MirVerifier verifier;
        if (!verifier.verify(compilation.mirModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return false;
        }
        output << "MIR verification: ok\n";
    }
    else if (action == "--dump-cfg" && compilation.cfgModule() != nullptr)
    {
        cfg::CfgDumper dumper;
        dumper.dump(compilation.cfgModule(), output);
    }
    else if (action == "--verify-cfg" && compilation.cfgModule() != nullptr)
    {
        cfg::CfgVerifier verifier;
        if (!verifier.verify(compilation.cfgModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return false;
        }
        output << "CFG verification: ok\n";
    }
    else if (action == "--dump-dominance" && compilation.dominanceModule() != nullptr)
    {
        dominance::DominanceDumper dumper;
        dumper.dump(compilation.dominanceModule(), output);
    }
    else if (action == "--verify-dominance" && compilation.dominanceModule() != nullptr)
    {
        dominance::DominanceVerifier verifier;
        if (!verifier.verify(compilation.dominanceModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return false;
        }
        output << "Dominance verification: ok\n";
    }
    return true;
}

bool renderFrontendSsaAndOptimization(std::string_view action, frontend::FrontendCompilation &compilation,
                                      std::ostream &output, std::ostream &error)
{
    if (action == "--dump-ssa" && compilation.ssaModule() != nullptr)
    {
        ssa::SsaDumper dumper;
        dumper.dump(compilation.ssaModule(), output);
    }
    else if (action == "--verify-ssa" && compilation.ssaModule() != nullptr)
    {
        ssa::SsaVerifier verifier;
        if (!verifier.verify(compilation.ssaModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return false;
        }
        output << "SSA verification: ok\n";
    }
    else if (action == "--dump-opt" && compilation.optimizationModule() != nullptr)
    {
        passes::OptimizationDumper dumper;
        dumper.dump(compilation.optimizationModule(), output);
    }
    else if (action == "--verify-opt" && compilation.optimizationModule() != nullptr)
    {
        passes::OptimizationVerifier verifier;
        if (!verifier.verify(compilation.optimizationModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return false;
        }
        output << "Optimization verification: ok\n";
    }
    else if (action == "--dump-pass-pipeline" && compilation.optimizationModule() != nullptr)
    {
        passes::PassPipelineDumper dumper;
        dumper.dump(compilation.optimizationModule(), output);
    }
    return true;
}

bool renderFrontendLlvm(std::string_view action, bool success, frontend::FrontendCompilation &compilation,
                        llvm_backend::LlvmOptimizationLevel optimizationLevel, std::ostream &output,
                        std::ostream &error)
{
    if (!((action == "--dump-llvm" || action == "--verify-llvm" || action == "--dump-llvm-opt" ||
           action == "--verify-llvm-opt" || action == "--program" || action == "--check") &&
          success && compilation.optimizationModule() != nullptr && compilation.semanticAnalyzer() != nullptr))
        return true;

    llvm_backend::LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
    std::unique_ptr<llvm_backend::LlvmModule> llvmModule = lowerer.lower(compilation.optimizationModule());
    if (llvmModule == nullptr)
    {
        error << "nsc: internal error: LLVM lowering failed: " << lowerer.error() << '\n';
        return false;
    }

    if (action == "--dump-llvm")
    {
        output << llvmModule->text();
        return true;
    }

    std::string verificationError;
    if (!llvmModule->verify(verificationError))
    {
        error << "nsc: internal error: LLVM verification failed: " << verificationError << '\n';
        return false;
    }

    if (action == "--verify-llvm")
    {
        output << "LLVM verification: ok\n";
        return true;
    }

    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr)
    {
        error << "nsc: internal error: target machine creation failed: " << targetError << '\n';
        return false;
    }

    // NSC-LIMIT-005: observada. Ver LlvmOptimizer.h.
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation observation;
    std::string optimizationError;
    if (!optimizer.optimize(*llvmModule, *targetMachine, optimizationLevel, &observation, optimizationError))
    {
        error << "nsc: internal error: LLVM optimization failed: " << optimizationError << '\n';
        return false;
    }

    if (action == "--dump-llvm-opt")
    {
        output << llvmModule->text();
    }
    else if (action == "--verify-llvm-opt")
    {
        output << "LLVM optimization verification: ok\n"
               << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n';
    }
    return true;
}

void renderFrontendErrors(frontend::FrontendCompilation &compilation, std::ostream &error)
{
    if (compilation.diagnostics().hasErrors())
    {
        diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
        renderer.renderAll(compilation.diagnostics(), error);
    }
    if (!compilation.hirError().empty())
    {
        error << "nsc: internal error: " << compilation.hirError() << '\n';
    }
    if (!compilation.mirError().empty())
    {
        error << "nsc: internal error: " << compilation.mirError() << '\n';
    }
    if (!compilation.cfgError().empty())
    {
        error << "nsc: internal error: " << compilation.cfgError() << '\n';
    }
    if (!compilation.dominanceError().empty())
    {
        error << "nsc: internal error: " << compilation.dominanceError() << '\n';
    }
    if (!compilation.ssaError().empty())
    {
        error << "nsc: internal error: " << compilation.ssaError() << '\n';
    }
    if (!compilation.optimizationError().empty())
    {
        error << "nsc: internal error: " << compilation.optimizationError() << '\n';
    }
}

ExitCode Driver::runFrontend(std::string_view action, std::string_view path,
                             llvm_backend::LlvmOptimizationLevel optimizationLevel, std::ostream &output,
                             std::ostream &error)
{
    frontend::FrontendCompilation compilation;
    std::string loadError;
    const bool requireMain = action == "--program";
    const bool success = compilation.compileFile(std::filesystem::path(path), loadError, requireMain);

    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }

    if (!renderFrontendSourceAndSyntax(action, compilation, output, error) ||
        !renderFrontendMirAndControlFlow(action, compilation, output, error) ||
        !renderFrontendSsaAndOptimization(action, compilation, output, error) ||
        !renderFrontendLlvm(action, success, compilation, optimizationLevel, output, error))
    {
        return ExitCode::CompilationError;
    }

    renderFrontendErrors(compilation, error);
    return success ? ExitCode::Success : ExitCode::CompilationError;
}
} // namespace nativesharp::driver
