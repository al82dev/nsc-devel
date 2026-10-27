#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../../src/Frontend/Cache/CachePayloadWriter.h"
#include "../../src/Frontend/Generics/Cache/TemplateCatalogCodec.h"
#include "../support/TestContext.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

// EN: The syntax tree serializer of TemplateCatalogCodec.cpp writes the syntax products of the syntax
//     caches and of the `.nsproducts` sidecar, a persistent format. It used to build one string per
//     subtree recursively; it now writes into one buffer with an explicit worklist. This test keeps a
//     literal copy of the recursive version as the reference and requires byte-identical output for
//     every NativeSharp source of the repository's libraries and test corpora.
// ES: El serializador de árboles sintácticos de TemplateCatalogCodec.cpp escribe los productos
//     sintácticos de las cachés de sintaxis y del sidecar `.nsproducts`, un formato persistente. Antes
//     construía un string por subárbol de forma recursiva; ahora escribe en un solo buffer con una lista
//     de trabajo explícita. Esta prueba conserva una copia literal de la versión recursiva como
//     referencia y exige la misma salida, byte por byte, para cada fuente NativeSharp de las bibliotecas
//     y de los corpus de prueba del repositorio.

namespace nativesharp::frontend::internal
{
// EN: Scalar writers shared by both versions; defined in TemplateCatalogCodec.cpp.
// ES: Escritores de escalares que comparten ambas versiones; definidos en TemplateCatalogCodec.cpp.
std::string serializeTypeSyntaxScalars(const ast::TypeSyntax& type);
std::string serializeAstNodeScalars(const ast::AstNode& node);
}

namespace
{
using namespace nativesharp;

std::string legacyTypeSyntaxTree(const ast::TypeSyntax* type);
std::string legacyAstNodeTree(const ast::AstNode* node);
std::string legacyGenericParameter(const ast::GenericParameterSyntax& parameter);
std::string legacyGenericConstraint(const ast::GenericConstraintClause& clause);

// ---- Reference: the recursive serializer as it was, copied verbatim (names prefixed). ----
// ---- Referencia: el serializador recursivo tal como era, copiado literal (nombres con prefijo). ----

void writeSourceRange(std::string& out, SourceRange range)
{
    frontend::internal::CachePayloadWriter::u32(out, range.begin().fileId().value());
    frontend::internal::CachePayloadWriter::u32(out, range.begin().offset());
    frontend::internal::CachePayloadWriter::u32(out, range.end().fileId().value());
    frontend::internal::CachePayloadWriter::u32(out, range.end().offset());
}

std::string legacyGenericParameter(
    const ast::GenericParameterSyntax& parameter)
{
    std::string out;
    frontend::internal::CachePayloadWriter::string(out, parameter.name);
    frontend::internal::CachePayloadWriter::byte(out, static_cast<std::uint8_t>(parameter.kind));
    frontend::internal::CachePayloadWriter::byte(out, parameter.constantType != nullptr ? 1U : 0U);
    if (parameter.constantType != nullptr)
        out += legacyTypeSyntaxTree(parameter.constantType);
    writeSourceRange(out, parameter.range);
    return out;
}

std::string legacyGenericConstraint(
    const ast::GenericConstraintClause& clause)
{
    std::string out;
    frontend::internal::CachePayloadWriter::string(out, clause.parameterName);
    writeSourceRange(out, clause.parameterRange);
    frontend::internal::CachePayloadWriter::byte(out, clause.requiresClass ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, clause.requiresValueType ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, clause.requiresDefaultConstructor ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, clause.requiresCopy ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, clause.requiresMove ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, clause.requiresTrackedLifetime ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, clause.requiresUnmanaged ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, clause.requiresNotNull ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, clause.requiresRefLike ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, clause.equalityConstraint != nullptr ? 1U : 0U);
    if (clause.equalityConstraint != nullptr)
        out += legacyTypeSyntaxTree(clause.equalityConstraint);
    frontend::internal::CachePayloadWriter::byte(out, clause.associatedProjection != nullptr ? 1U : 0U);
    if (clause.associatedProjection != nullptr)
        out += legacyTypeSyntaxTree(clause.associatedProjection);
    frontend::internal::CachePayloadWriter::u32(out, static_cast<std::uint32_t>(clause.typeConstraints.size()));
    for (const ast::TypeSyntax* constraint : clause.typeConstraints)
        out += legacyTypeSyntaxTree(constraint);
    writeSourceRange(out, clause.range);
    frontend::internal::CachePayloadWriter::byte(out, clause.substitutedArgument != nullptr ? 1U : 0U);
    if (clause.substitutedArgument != nullptr)
        out += legacyTypeSyntaxTree(clause.substitutedArgument);
    return out;
}

std::string legacyTypeSyntaxTree(const ast::TypeSyntax* type)
{
    if (type == nullptr) return {};
    std::string out = frontend::internal::serializeTypeSyntaxScalars(*type);
    frontend::internal::CachePayloadWriter::u32(out, static_cast<std::uint32_t>(type->typeArguments().size()));
    for (const ast::TypeSyntax* argument : type->typeArguments())
        out += legacyTypeSyntaxTree(argument);
    const bool hasReturn = type->functionReturnType() != nullptr;
    const bool hasError = type->functionErrorType() != nullptr;
    frontend::internal::CachePayloadWriter::byte(out, hasReturn ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, hasError ? 1U : 0U);
    if (hasReturn) out += legacyTypeSyntaxTree(type->functionReturnType());
    if (hasError) out += legacyTypeSyntaxTree(type->functionErrorType());
    frontend::internal::CachePayloadWriter::u32(
        out,
        static_cast<std::uint32_t>(type->modifiers().size()));
    for (const ast::TypeModifierSyntax& modifier : type->modifiers())
    {
        frontend::internal::CachePayloadWriter::byte(out, static_cast<std::uint8_t>(modifier.kind));
        writeSourceRange(out, modifier.range);
        frontend::internal::CachePayloadWriter::u64(out, modifier.arrayRank);
        frontend::internal::CachePayloadWriter::byte(
            out,
            static_cast<std::uint8_t>(modifier.pointerNullability));
        frontend::internal::CachePayloadWriter::byte(out, modifier.pointerHasProvenance ? 1U : 0U);
        frontend::internal::CachePayloadWriter::u64(out, modifier.pointerKnownAlignment);
        frontend::internal::CachePayloadWriter::u32(out, modifier.pointerAddressSpace);
        frontend::internal::CachePayloadWriter::byte(out, modifier.arrayCount != nullptr ? 1U : 0U);
        if (modifier.arrayCount != nullptr) out += legacyAstNodeTree(modifier.arrayCount);
    }
    frontend::internal::CachePayloadWriter::byte(out, type->constExpression() != nullptr ? 1U : 0U);
    if (type->constExpression() != nullptr) out += legacyAstNodeTree(type->constExpression());
    return out;
}

void legacyDeclarationMetadata(std::string& out, const ast::AstNode& node)
{
    frontend::internal::CachePayloadWriter::u32(
        out,
        static_cast<std::uint32_t>(node.genericParameterSyntaxes().size()));
    for (const ast::GenericParameterSyntax& parameter : node.genericParameterSyntaxes())
        out += legacyGenericParameter(parameter);
    frontend::internal::CachePayloadWriter::u32(
        out,
        static_cast<std::uint32_t>(node.lifetimeParameters().size()));
    for (const ast::LifetimeParameterSyntax& parameter : node.lifetimeParameters())
    {
        frontend::internal::CachePayloadWriter::string(out, parameter.name);
        writeSourceRange(out, parameter.range);
    }
    frontend::internal::CachePayloadWriter::u32(
        out,
        static_cast<std::uint32_t>(node.lifetimeOutlivesConstraints().size()));
    for (const ast::LifetimeOutlivesConstraint& constraint : node.lifetimeOutlivesConstraints())
    {
        frontend::internal::CachePayloadWriter::string(out, constraint.longer);
        frontend::internal::CachePayloadWriter::string(out, constraint.shorter);
        writeSourceRange(out, constraint.range);
    }
    frontend::internal::CachePayloadWriter::u32(
        out,
        static_cast<std::uint32_t>(node.genericConstraints().size()));
    for (const ast::GenericConstraintClause& constraint : node.genericConstraints())
        out += legacyGenericConstraint(constraint);
}

std::string legacyAstNodeTree(const ast::AstNode* node)
{
    if (node == nullptr) return {};
    std::string out = frontend::internal::serializeAstNodeScalars(*node);
    const ast::TypeSyntax* typeSyntax = node->typeSyntax();
    const ast::TypeSyntax* throwsType = node->throwsTypeSyntax();
    frontend::internal::CachePayloadWriter::byte(out, typeSyntax != nullptr ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, throwsType != nullptr ? 1U : 0U);
    if (typeSyntax != nullptr) out += legacyTypeSyntaxTree(typeSyntax);
    if (throwsType != nullptr) out += legacyTypeSyntaxTree(throwsType);
    frontend::internal::CachePayloadWriter::u32(out, static_cast<std::uint32_t>(node->typeSyntaxes().size()));
    for (const ast::TypeSyntax* syntax : node->typeSyntaxes())
        out += legacyTypeSyntaxTree(syntax);
    legacyDeclarationMetadata(out, *node);
    const ast::AstNode* first = node->first();
    const ast::AstNode* second = node->second();
    const ast::AstNode* third = node->third();
    frontend::internal::CachePayloadWriter::byte(out, first != nullptr ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, second != nullptr ? 1U : 0U);
    frontend::internal::CachePayloadWriter::byte(out, third != nullptr ? 1U : 0U);
    if (first != nullptr) out += legacyAstNodeTree(first);
    if (second != nullptr) out += legacyAstNodeTree(second);
    if (third != nullptr) out += legacyAstNodeTree(third);
    frontend::internal::CachePayloadWriter::u32(out, static_cast<std::uint32_t>(node->children().size()));
    for (const ast::AstNode* child : node->children())
        out += legacyAstNodeTree(child);
    return out;
}

// ---- End of the reference. / Fin de la referencia. ----

// EN: Parses one source file into `arena`; returns the root even when the parser reported errors,
//     because the serializer must reproduce whatever tree the parser built.
// ES: Analiza un archivo fuente en `arena`; devuelve la raíz aunque el parser reporte errores, porque
//     el serializador debe reproducir cualquier árbol que el parser haya construido.
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

// EN: Every `.ns` file under `root`, in a stable order.
// ES: Cada archivo `.ns` bajo `root`, en un orden estable.
std::vector<std::filesystem::path> sourceFiles(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> files;
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) return files;
    for (std::filesystem::recursive_directory_iterator entry(root, error), end; entry != end;
         entry.increment(error))
    {
        if (error) break;
        // EN: Build outputs (obj folders) hold generated copies; only checked-in sources count.
        // ES: Las salidas de build (carpetas obj) tienen copias generadas; solo cuentan las fuentes del repo.
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
}

int main()
{
    tests::TestContext tests;
    const std::filesystem::path sourceRoot(NSC_SOURCE_DIR);
    std::size_t compared = 0;
    std::size_t bytes = 0;
    std::vector<std::string> mismatches;
    for (const char* folder : {"lib", "tests/cc1", "tests/language", "tests/bug_triage"})
    {
        for (const std::filesystem::path& file : sourceFiles(sourceRoot / folder))
        {
            source::SourceManager sources;
            ast::AstArena arena;
            std::vector<lexer::Token> tokens;
            const ast::AstNode* root = parseFile(file, sources, arena, tokens);
            if (root == nullptr) continue;
            const std::string expected = legacyAstNodeTree(root);
            const std::string actual = frontend::internal::serializeAstNodeTree(root);
            ++compared;
            bytes += expected.size();
            if (expected != actual) mismatches.push_back(file.generic_string());
        }
    }
    tests.expect(compared >= 50U,
        "the corpus has at least 50 parsed sources (found " + std::to_string(compared) + ")");
    tests.expect(mismatches.empty(),
        "the worklist serializer writes the same bytes as the recursive one for every source" +
            (mismatches.empty() ? std::string{} : " (first mismatch: " + mismatches.front() + ")"));
    tests.expect(frontend::internal::serializeAstNodeTree(nullptr).empty(),
        "a null tree still serializes to nothing");
    tests.expect(bytes > 0U, "the compared trees are not empty");
    return tests.result();
}
