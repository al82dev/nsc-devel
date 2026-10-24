#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/PrecompiledSyntaxNodeIndex.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../../src/Frontend/Snapshot/PrecompiledSnapshotValidation.h"
#include "../support/TestContext.h"

#include <string>
#include <vector>

// EN: Plan item 1.5. A snapshot header is accepted only when every dependency it names is loaded
//     with the same NSMETA digest and the restored API tree matches its counts and kind digest.
// ES: Punto 1.5 del plan. La cabecera de un snapshot solo se acepta cuando cada dependencia que
//     nombra está cargada con el mismo digest de NSMETA y el árbol de API restaurado coincide con
//     sus cantidades y su digest de clases.

namespace
{
using namespace nativesharp;

const ast::AstNode* parse(
    const char* text,
    source::SourceManager& sources,
    ast::AstArena& arena,
    std::vector<lexer::Token>& tokens)
{
    diagnostics::DiagnosticEngine diagnostics;
    const FileId id = sources.addBuffer("Sample.ns", text);
    lexer::Lexer lexer(*sources.getFile(id), diagnostics);
    tokens = lexer.lexAll();
    parser::Parser parser(tokens, arena, diagnostics);
    return parser.parseCompilationUnit();
}
}

int main()
{
    tests::TestContext tests;
    source::SourceManager sources;
    ast::AstArena arena;
    std::vector<lexer::Token> tokens;
    std::vector<lexer::Token> otherTokens;
    const ast::AstNode* root = parse(
        "namespace Sample { public class A { public int X; } }", sources, arena, tokens);
    const ast::AstNode* otherRoot = parse(
        "namespace Sample { public class A { public int X; public int Y; } }", sources, arena,
        otherTokens);
    frontend::PrecompiledSyntaxNodeIndex index;
    index.build(root);
    frontend::PrecompiledSyntaxNodeIndex otherIndex;
    otherIndex.build(otherRoot);

    compilation::PrecompiledSemanticSnapshot snapshot;
    snapshot.dependencies.push_back({"NativeSharp.Core", std::string(64, 'a')});
    snapshot.nodeCount = index.nodes().size();
    snapshot.typeCount = index.types().size();
    snapshot.nodeKindDigest = index.kindDigest();
    const std::vector<compilation::PrecompiledSnapshotDependency> loaded = {
        {"NativeSharp.Core", std::string(64, 'a')}, {"Other", std::string(64, 'f')}};
    std::string reason;
    tests.expect(frontend::validatePrecompiledSnapshot(snapshot, loaded, index, reason) && reason.empty(),
        "a matching snapshot is accepted");

    const std::vector<compilation::PrecompiledSnapshotDependency> changed = {
        {"NativeSharp.Core", std::string(64, 'b')}};
    tests.expect(!frontend::validatePrecompiledSnapshot(snapshot, changed, index, reason) &&
        reason == "dependency-digest:NativeSharp.Core", "another dependency NSMETA is rejected");
    tests.expect(!frontend::validatePrecompiledSnapshot(snapshot, {}, index, reason) &&
        reason == "dependency-not-loaded:NativeSharp.Core", "a missing dependency is rejected");
    tests.expect(!frontend::validatePrecompiledSnapshot(snapshot, loaded, otherIndex, reason) &&
        reason == "node-count", "another tree shape is rejected by its counts");

    compilation::PrecompiledSemanticSnapshot wrongKinds = snapshot;
    wrongKinds.nodeKindDigest = std::string(64, '0');
    tests.expect(!frontend::validatePrecompiledSnapshot(wrongKinds, loaded, index, reason) &&
        reason == "node-kinds", "another kind digest is rejected");
    return tests.result();
}
