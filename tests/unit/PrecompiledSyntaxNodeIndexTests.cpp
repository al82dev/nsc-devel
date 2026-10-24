#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/PrecompiledSyntaxNodeIndex.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../../src/Frontend/Cache/CachePayloadReader.h"
#include "../../src/Frontend/Generics/Cache/TemplateCatalogCodec.h"
#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// EN: Plan item 1.3. The producer indexes the API tree it analyzed and the consumer indexes the tree
//     it restored from the `.nsproducts` syntax product; snapshot references only work if both
//     indexes agree. For every library source this test indexes the parsed tree and the tree
//     restored from its serialized product and requires the same counts and kind digest. It also
//     checks that indexOf() finds the first occurrence and that every operand and child is indexed.
// ES: Punto 1.3 del plan. El productor indexa el árbol de API que analizó y el consumidor indexa el
//     árbol que restauró del producto sintáctico del `.nsproducts`; las referencias del snapshot solo
//     funcionan si ambos índices coinciden. Para cada fuente de las bibliotecas esta prueba indexa el
//     árbol parseado y el restaurado de su producto serializado y exige las mismas cantidades y el
//     mismo digest de clases. También comprueba que indexOf() encuentra la primera aparición y que
//     todo operando e hijo queda indexado.

namespace
{
using namespace nativesharp;

const ast::AstNode* parseFile(
    const std::filesystem::path& path,
    source::SourceManager& sources,
    ast::AstArena& arena,
    std::vector<lexer::Token>& tokens)
{
    std::ifstream stream(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    diagnostics::DiagnosticEngine diagnostics;
    const FileId id = sources.addBuffer(path.generic_string(), std::move(text));
    lexer::Lexer lexer(*sources.getFile(id), diagnostics);
    tokens = lexer.lexAll();
    parser::Parser parser(tokens, arena, diagnostics);
    return parser.parseCompilationUnit();
}

std::vector<std::filesystem::path> sourceFiles(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> files;
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) return files;
    for (std::filesystem::recursive_directory_iterator entry(root, error), end; entry != end;
         entry.increment(error))
    {
        if (error) break;
        if (entry->is_directory(error) && entry->path().filename() == "obj")
        {
            entry.disable_recursion_pending();
            continue;
        }
        if (entry->is_regular_file(error) && entry->path().extension() == ".ns")
            files.push_back(entry->path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

// EN: True when indexOf() finds the first occurrence and every operand, child and type slot is indexed.
// ES: True cuando indexOf() encuentra la primera aparición y todo operando, hijo y ranura de tipo
//     está indexado.
bool indexIsClosed(const frontend::PrecompiledSyntaxNodeIndex& index)
{
    for (std::size_t position = 0; position < index.nodes().size(); ++position)
    {
        const ast::AstNode* node = index.nodes()[position];
        const std::optional<std::size_t> first = index.indexOf(node);
        if (!first.has_value() || *first > position || index.nodes()[*first] != node) return false;
        for (const ast::AstNode* operand : {node->first(), node->second(), node->third()})
            if (operand != nullptr && !index.indexOf(operand).has_value()) return false;
        for (const ast::AstNode* child : node->children())
            if (!index.indexOf(child).has_value()) return false;
        if (node->typeSyntax() != nullptr && !index.indexOf(node->typeSyntax()).has_value())
            return false;
        for (const ast::TypeSyntax* type : node->typeSyntaxes())
            if (!index.indexOf(type).has_value()) return false;
    }
    for (std::size_t position = 0; position < index.types().size(); ++position)
    {
        const ast::TypeSyntax* type = index.types()[position];
        const std::optional<std::size_t> first = index.indexOf(type);
        if (!first.has_value() || *first > position || index.types()[*first] != type) return false;
        for (const ast::TypeSyntax* argument : type->typeArguments())
            if (!index.indexOf(argument).has_value()) return false;
    }
    return true;
}
}

int main()
{
    tests::TestContext tests;
    const std::filesystem::path sourceRoot(NSC_SOURCE_DIR);
    std::size_t compared = 0;
    std::vector<std::string> mismatches;
    std::vector<std::string> openIndexes;
    std::vector<std::string> digests;
    for (const std::filesystem::path& file : sourceFiles(sourceRoot / "lib"))
    {
        source::SourceManager sources;
        ast::AstArena arena;
        std::vector<lexer::Token> tokens;
        const ast::AstNode* root = parseFile(file, sources, arena, tokens);
        if (root == nullptr) continue;
        frontend::PrecompiledSyntaxNodeIndex parsed;
        parsed.build(root);
        const std::string product = frontend::internal::serializeAstNodeTree(root);
        frontend::internal::CachePayloadReader reader(product);
        ast::AstArena restoredArena;
        const ast::AstNode* restoredRoot =
            frontend::internal::deserializeAstNodeTree(restoredArena, reader);
        frontend::PrecompiledSyntaxNodeIndex restored;
        restored.build(restoredRoot);
        ++compared;
        if (restoredRoot == nullptr || parsed.nodes().size() != restored.nodes().size() ||
            parsed.types().size() != restored.types().size() ||
            parsed.kindDigest() != restored.kindDigest())
        {
            mismatches.push_back(file.generic_string());
        }
        if (!indexIsClosed(parsed)) openIndexes.push_back(file.generic_string());
        if (parsed.nodes().empty() || parsed.nodes().front() != root)
            openIndexes.push_back(file.generic_string());
        digests.push_back(parsed.kindDigest());
    }
    tests.expect(compared >= 50U,
        "the library corpus has at least 50 parsed sources (found " + std::to_string(compared) + ")");
    tests.expect(mismatches.empty(),
        "the parsed and the restored tree index alike for every source" +
            (mismatches.empty() ? std::string{} : " (first mismatch: " + mismatches.front() + ")"));
    tests.expect(openIndexes.empty(),
        "the root is index 0, indexOf() finds the first occurrence and every slot is indexed" +
            (openIndexes.empty() ? std::string{} : " (first failure: " + openIndexes.front() + ")"));
    std::sort(digests.begin(), digests.end());
    tests.expect(std::unique(digests.begin(), digests.end()) - digests.begin() > 1,
        "different sources do not all share one kind digest");

    frontend::PrecompiledSyntaxNodeIndex empty;
    empty.build(nullptr);
    tests.expect(empty.nodes().empty() && empty.types().empty() && empty.kindDigest().size() == 64U,
        "an absent tree has an empty index and a digest");
    return tests.result();
}
