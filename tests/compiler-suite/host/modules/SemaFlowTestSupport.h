#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeContext.h"
#include "nativesharp/Source/SourceManager.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite::sema_flow_support
{
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::sema::Symbol;

// Stops at semantic analysis deliberately. Host.Sema cases must not acquire
// dependencies on later compiler stages whose failures would obscure Sema.
class FrontendCompilation final
{
public:
    bool compileBuffer(std::filesystem::path path, std::string source, bool requireMain)
    {
        const auto fileId = sources_.addBuffer(std::move(path), std::move(source));
        const auto* file = sources_.getFile(fileId);
        if (file == nullptr) return false;

        nativesharp::lexer::Lexer lexer(*file, diagnostics_);
        tokens_ = lexer.lexAll();
        if (diagnostics_.hasErrors()) return false;

        nativesharp::parser::Parser parser(tokens_, arena_, diagnostics_);
        syntaxTree_ = parser.parseCompilationUnit();
        if (syntaxTree_ == nullptr || diagnostics_.hasErrors()) return false;

        analyzer_ = std::make_unique<nativesharp::sema::SemanticAnalyzer>(
            types_, nativesharp::target::TargetDataLayout::WindowsX64(), diagnostics_);
        if (!analyzer_->analyze(syntaxTree_)) return false;
        return !requireMain || analyzer_->validateExecutableEntryPoint(syntaxTree_);
    }

    const nativesharp::diagnostics::DiagnosticEngine& diagnostics() const noexcept { return diagnostics_; }
    nativesharp::ast::AstNode* syntaxTree() const noexcept { return syntaxTree_; }
    const nativesharp::sema::SemanticAnalyzer* semanticAnalyzer() const noexcept { return analyzer_.get(); }

private:
    nativesharp::source::SourceManager sources_;
    nativesharp::diagnostics::DiagnosticEngine diagnostics_;
    std::vector<nativesharp::lexer::Token> tokens_;
    nativesharp::ast::AstArena arena_;
    nativesharp::ast::AstNode* syntaxTree_ = nullptr;
    nativesharp::sema::TypeContext types_;
    std::unique_ptr<nativesharp::sema::SemanticAnalyzer> analyzer_;
};

struct ExpectedDiagnostic final
{
    DiagnosticId id;
    std::string messageFragment;
};

struct Scenario final
{
    std::string label;
    std::string source;
    bool requireMain = false;
    bool shouldCompile = true;
    std::vector<ExpectedDiagnostic> diagnostics;
    std::vector<std::string> globalSymbols;
};

// Finds one global symbol by its stable source name. Complexity is O(g).
inline const Symbol* findGlobal(const FrontendCompilation& compilation, const std::string& name)
{
    const auto* analyzer = compilation.semanticAnalyzer();
    if (analyzer == nullptr)
    {
        return nullptr;
    }
    for (const Symbol* symbol : analyzer->globalSymbols())
    {
        if (symbol != nullptr && symbol->name() == name)
        {
            return symbol;
        }
    }
    return nullptr;
}

// Compares the complete ordered diagnostic contract, including selected message text. Complexity is O(d).
inline void expectDiagnostics(TestContext& context, const std::string& id, const Scenario& scenario,
    const FrontendCompilation& compilation)
{
    const auto& actual = compilation.diagnostics().diagnostics();
    context.expect(actual.size() == scenario.diagnostics.size(),
        id + " " + scenario.label + " diagnostic count must be exact");
    const std::size_t common = std::min(actual.size(), scenario.diagnostics.size());
    for (std::size_t index = 0; index < common; ++index)
    {
        const ExpectedDiagnostic& expected = scenario.diagnostics[index];
        context.expect(actual[index].id() == expected.id,
            id + " " + scenario.label + " diagnostic identity must be exact");
        if (!expected.messageFragment.empty())
        {
            context.expect(actual[index].message().find(expected.messageFragment) != std::string::npos,
                id + " " + scenario.label + " diagnostic text must retain its contract");
        }
    }
}

// Runs one isolated semantic scenario and checks all declared observables. Complexity is O(source + d + g).
inline void runScenario(TestContext& context, const std::string& id, const Scenario& scenario)
{
    FrontendCompilation compilation;
    const bool completed = compilation.compileBuffer(
        std::filesystem::path(id + "-" + scenario.label + ".ns"), scenario.source, scenario.requireMain);
    context.expect(completed == scenario.shouldCompile,
        id + " " + scenario.label + " completion state must be exact");
    context.expect(compilation.syntaxTree() != nullptr,
        id + " " + scenario.label + " must retain the parsed syntax tree");
    expectDiagnostics(context, id, scenario, compilation);
    for (const std::string& name : scenario.globalSymbols)
    {
        context.expect(findGlobal(compilation, name) != nullptr,
            id + " " + scenario.label + " must retain global symbol " + name);
    }
}

// Registers one canonical Host.Sema case with fresh state for every scenario. Complexity is O(1).
inline void addCase(TestRegistry& registry, std::string id, std::string contract,
    std::initializer_list<Scenario> scenarios, TestDisposition disposition = TestDisposition::Required)
{
    std::vector<Scenario> ownedScenarios(scenarios);
    const std::string capturedId = id;
    registry.add(TestCase{
        std::move(id), std::move(contract), "Host.Sema", "component", "PR", {"sema", "phase2"},
        [capturedId, ownedScenarios = std::move(ownedScenarios)](TestContext& context) {
            for (const Scenario& scenario : ownedScenarios)
            {
                runScenario(context, capturedId, scenario);
            }
        },
        disposition,
    });
}
}
