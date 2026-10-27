#include "nativesharp/Compilation/PrecompiledSurfaceDemandCollection.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Source/SourceFile.h"
#include "nativesharp/Source/SourceManager.h"

#include <string_view>
#include <utility>
#include <vector>

namespace nativesharp::compilation
{
namespace
{
bool isNameByte(char value) noexcept
{
    const unsigned char byte = static_cast<unsigned char>(value);
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') ||
        byte == '_' || byte >= 0x80U;
}

// The same test the frontend uses before running compile-time expansion on a source.
bool mayRequireCompileTimeExpansion(std::string_view source) noexcept
{
    return source.find('#') != std::string_view::npos ||
        source.find("comptime") != std::string_view::npos ||
        source.find("__nsc_macro_") != std::string_view::npos;
}

void addNamesInText(std::string_view text, std::unordered_set<std::string>& names)
{
    std::size_t cursor = 0;
    while (cursor < text.size())
    {
        if (!isNameByte(text[cursor]))
        {
            ++cursor;
            continue;
        }
        const std::size_t begin = cursor;
        while (cursor < text.size() && isNameByte(text[cursor])) ++cursor;
        names.emplace(text.substr(begin, cursor - begin));
    }
}

bool isStringToken(lexer::TokenKind kind) noexcept
{
    return kind == lexer::TokenKind::StringLiteral || kind == lexer::TokenKind::InterpolatedStringLiteral ||
        kind == lexer::TokenKind::Utf8StringLiteral;
}

// Adds the names of one source. Returns false when the source does not tokenize.
bool addSourceNames(
    const std::filesystem::path& path,
    std::string text,
    std::unordered_set<std::string>& names)
{
    const source::SourceFile file(FileId{1U}, path, std::move(text));
    diagnostics::DiagnosticEngine diagnostics;
    lexer::Lexer lexer(file, diagnostics);
    const std::vector<lexer::Token> tokens = lexer.lexAll();
    if (diagnostics.hasErrors()) return false;
    for (const lexer::Token& token : tokens)
    {
        const std::string_view lexeme = token.lexeme();
        if (isStringToken(token.kind()))
        {
            addNamesInText(lexeme, names);
            continue;
        }
        if (!lexeme.empty() && isNameByte(lexeme.front())) names.emplace(lexeme);
    }
    return true;
}
} // namespace

PrecompiledSurfaceDemand collectPrecompiledSurfaceDemand(
    const ProjectGraph& graph,
    const PrecompiledSurfaceSourceProvider& sourceProvider)
{
    PrecompiledSurfaceDemand demand;
    for (const Project& project : graph.projects())
    {
        for (const ProjectSource& source : project.sourceSet().sources())
        {
            std::optional<std::string> text;
            if (sourceProvider) text = sourceProvider(source.absolutePath);
            if (!text.has_value())
            {
                std::string readError;
                text = source::SourceManager::readFileBuffer(source.absolutePath, readError);
            }
            if (!text.has_value() || mayRequireCompileTimeExpansion(*text) ||
                !addSourceNames(source.absolutePath, std::move(*text), demand.names))
            {
                return PrecompiledSurfaceDemand{};
            }
        }
        addNamesInText(project.panicHandler(), demand.names);
    }
    demand.enabled = true;
    return demand;
}
} // namespace nativesharp::compilation
