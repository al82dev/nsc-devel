#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/PrecompiledSyntaxNodeIndex.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../../src/Frontend/Snapshot/PrecompiledSnapshotReader.h"
#include "../../src/Frontend/Snapshot/PrecompiledSnapshotWriter.h"
#include "../support/TestContext.h"

#include <string>
#include <vector>

// EN: Plan item 1.4. Snapshot references: syntax by relative index, symbols by identity, same-unit
//     symbols by relative ordinal. Every reference written is read back as the same value, and a
//     wrong reference kind, an index outside the unit, a pointer outside the index or truncated
//     bytes make the reader or the writer fail.
// ES: Punto 1.4 del plan. Referencias del snapshot: sintaxis por índice relativo, símbolos por
//     identidad, símbolos de la misma unidad por ordinal relativo. Toda referencia escrita se lee
//     de vuelta con el mismo valor, y una clase de referencia equivocada, un índice fuera de la
//     unidad, un puntero fuera del índice o bytes truncados hacen fallar al lector o al escritor.

namespace
{
using namespace nativesharp;

constexpr const char* kSource =
    "namespace Sample\n"
    "{\n"
    "    public class Box<T>\n"
    "    {\n"
    "        public int Count;\n"
    "        public T Get(int index) { return default; }\n"
    "    }\n"
    "}\n";
}

int main()
{
    tests::TestContext tests;
    source::SourceManager sources;
    diagnostics::DiagnosticEngine diagnostics;
    const FileId id = sources.addBuffer("Sample.ns", kSource);
    lexer::Lexer lexer(*sources.getFile(id), diagnostics);
    std::vector<lexer::Token> tokens = lexer.lexAll();
    ast::AstArena arena;
    parser::Parser parser(tokens, arena, diagnostics);
    const ast::AstNode* root = parser.parseCompilationUnit();
    frontend::PrecompiledSyntaxNodeIndex index;
    index.build(root);
    tests.expect(index.nodes().size() > 3U && !index.types().empty(), "the sample tree is indexed");
    if (index.nodes().size() <= 3U || index.types().empty()) return tests.result();

    const ast::AstNode* lastNode = index.nodes().back();
    const ast::TypeSyntax* firstType = index.types().front();
    frontend::PrecompiledSnapshotWriter writer(index);
    writer.node(root);
    writer.node(lastNode);
    writer.node(nullptr);
    writer.type(firstType);
    writer.type(nullptr);
    writer.typeKey("type:Sample::Sample.Box`1");
    writer.memberSignature("type:Sample::Sample.Box`1", "Get(int)");
    writer.namespaceSymbol("function:Sample::Sample.Helper()");
    writer.specialization("Box__g1__int");
    writer.unitOrdinal(7U);
    writer.u32(42U);
    writer.text("tail");
    tests.expect(!writer.failed(), "indexed syntax is written without failure");
    const std::string bytes = writer.take();

    frontend::PrecompiledSnapshotReader reader(bytes, index);
    tests.expect(reader.node() == root && reader.node() == lastNode && reader.node() == nullptr,
        "syntax nodes and a null node come back as the same pointers");
    tests.expect(reader.type() == firstType && reader.type() == nullptr,
        "type syntaxes and a null type come back as the same pointers");
    tests.expect(reader.typeKey() == "type:Sample::Sample.Box`1", "a type key comes back");
    std::string declaringType;
    std::string signature;
    reader.memberSignature(declaringType, signature);
    tests.expect(declaringType == "type:Sample::Sample.Box`1" && signature == "Get(int)",
        "a member signature comes back");
    tests.expect(reader.namespaceSymbol() == "function:Sample::Sample.Helper()",
        "a namespace symbol key comes back");
    tests.expect(reader.specialization() == "Box__g1__int", "a specialization key comes back");
    tests.expect(reader.unitOrdinal() == 7U, "a unit ordinal comes back");
    tests.expect(reader.u32() == 42U && reader.text() == "tail", "scalars come back");
    tests.expect(!reader.failed() && reader.atEnd(), "everything was read without failure");

    frontend::PrecompiledSnapshotReader wrongKind(bytes, index);
    (void)wrongKind.typeKey();
    tests.expect(wrongKind.failed(), "reading a node reference as a type key fails");

    frontend::PrecompiledSyntaxNodeIndex emptyIndex;
    emptyIndex.build(nullptr);
    frontend::PrecompiledSnapshotReader outside(bytes, emptyIndex);
    tests.expect(outside.node() == nullptr && outside.failed(), "a node index outside the unit fails");

    frontend::PrecompiledSnapshotReader truncated(bytes.substr(0, bytes.size() - 2), index);
    for (int step = 0; step < 32 && !truncated.failed(); ++step) (void)truncated.u64();
    tests.expect(truncated.failed(), "truncated bytes fail");

    ast::AstArena foreignArena;
    const ast::AstNode* foreign =
        foreignArena.make<ast::AstNode>(root->kind(), SourceRange{});
    frontend::PrecompiledSnapshotWriter foreignWriter(index);
    foreignWriter.node(foreign);
    tests.expect(foreignWriter.failed(), "a pointer outside the index makes the writer fail");
    return tests.result();
}
