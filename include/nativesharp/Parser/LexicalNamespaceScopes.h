#pragma once

#include "nativesharp/Lexer/Token.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
struct LexicalUsing final
{
    std::string alias;
    std::vector<std::string> target;
    bool complete = true;
    bool explicitTypeSyntax = false;
};

struct LexicalNamespaceScope final
{
    FileId file;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    std::optional<std::size_t> parent;
    std::vector<std::string> namespaceSegments;
    std::vector<LexicalUsing> directives;
};

// A declaration-scope inventory, not semantic name resolution. Bodies are skipped so a
// resource `using` or a lambda cannot be mistaken for a namespace directive.
inline std::vector<LexicalNamespaceScope> lexicalNamespaceScopes(std::span<const lexer::Token> tokens)
{
    using lexer::TokenKind;
    if (tokens.empty() || !tokens.front().range().begin().isValid()) return {};
    const auto file = tokens.front().range().begin().fileId();
    const auto last = tokens.back().range().end().offset();
    const auto end = last == std::numeric_limits<std::uint32_t>::max() ? last : last + 1;
    std::vector<LexicalNamespaceScope> scopes{{file, 0, end, std::nullopt, {}, {}}};
    struct Frame { std::size_t scope; bool closesWithBrace; };
    std::vector<Frame> stack{{0, false}};
    std::size_t bodyDepth = 0;
    auto qualified = [&](std::size_t& i, std::vector<std::string>& result) {
        if (i >= tokens.size() || tokens[i].kind() != TokenKind::Identifier) return false;
        result.emplace_back(tokens[i++].lexeme());
        while (i + 1 < tokens.size() && tokens[i].kind() == TokenKind::Dot &&
               tokens[i + 1].kind() == TokenKind::Identifier)
        {
            result.emplace_back(tokens[i + 1].lexeme());
            i += 2;
        }
        return true;
    };
    for (std::size_t i = 0; i < tokens.size();)
    {
        const auto kind = tokens[i].kind();
        if (kind == TokenKind::EndOfFile) break;
        if (bodyDepth != 0)
        {
            if (kind == TokenKind::LeftBrace) ++bodyDepth;
            else if (kind == TokenKind::RightBrace) --bodyDepth;
            ++i;
            continue;
        }
        if (kind == TokenKind::RightBrace)
        {
            if (stack.size() > 1 && stack.back().closesWithBrace)
            {
                scopes[stack.back().scope].end = tokens[i].range().begin().offset();
                stack.pop_back();
            }
            ++i;
            continue;
        }
        if (kind == TokenKind::KwNamespace)
        {
            const auto begin = tokens[i].range().begin().offset();
            std::size_t next = i + 1;
            std::vector<std::string> declared;
            if (qualified(next, declared) && next < tokens.size() &&
                (tokens[next].kind() == TokenKind::LeftBrace || tokens[next].kind() == TokenKind::Semicolon))
            {
                const auto parent = stack.back().scope;
                auto path = scopes[parent].namespaceSegments;
                path.insert(path.end(), declared.begin(), declared.end());
                const bool braced = tokens[next].kind() == TokenKind::LeftBrace;
                scopes.push_back({file, begin, end, parent, std::move(path), {}});
                stack.push_back({scopes.size() - 1, braced});
                i = next + 1;
                continue;
            }
        }
        if (kind == TokenKind::KwUsing)
        {
            std::size_t next = i + 1;
            LexicalUsing directive;
            if (next + 1 < tokens.size() && tokens[next].kind() == TokenKind::Identifier &&
                tokens[next + 1].kind() == TokenKind::Equal)
            {
                directive.alias = std::string(tokens[next].lexeme());
                next += 2;
            }
            bool hasTarget = qualified(next, directive.target);
            if (!hasTarget && !directive.alias.empty() && next < tokens.size() &&
                lexer::isBuiltinTypeKeyword(tokens[next].kind()))
            {
                directive.target.emplace_back(tokens[next++].lexeme());
                directive.explicitTypeSyntax = true;
                hasTarget = true;
            }
            if (hasTarget && !directive.alias.empty() && next < tokens.size() &&
                (tokens[next].kind() == TokenKind::Star || tokens[next].kind() == TokenKind::LeftBracket ||
                 tokens[next].kind() == TokenKind::Question || tokens[next].kind() == TokenKind::Bang ||
                 tokens[next].kind() == TokenKind::KwConst || tokens[next].kind() == TokenKind::Less))
            {
                directive.explicitTypeSyntax = true;
                while (next < tokens.size() && tokens[next].kind() != TokenKind::Semicolon &&
                       tokens[next].kind() != TokenKind::LeftBrace &&
                       tokens[next].kind() != TokenKind::RightBrace &&
                       tokens[next].kind() != TokenKind::EndOfFile) ++next;
            }
            if (hasTarget && next < tokens.size() && tokens[next].kind() == TokenKind::Semicolon)
            {
                scopes[stack.back().scope].directives.push_back(std::move(directive));
                i = next + 1;
                continue;
            }
            // Preserve unsupported syntax for a later stage; never turn a prefix such as
            // `using P = T*` into a different, apparently complete alias to `T`.
            directive.complete = false;
            scopes[stack.back().scope].directives.push_back(std::move(directive));
            while (next < tokens.size() && tokens[next].kind() != TokenKind::Semicolon &&
                   tokens[next].kind() != TokenKind::LeftBrace &&
                   tokens[next].kind() != TokenKind::RightBrace &&
                   tokens[next].kind() != TokenKind::EndOfFile) ++next;
            i = next < tokens.size() && tokens[next].kind() == TokenKind::Semicolon ? next + 1 : next;
            continue;
        }
        if (kind == TokenKind::LeftBrace) bodyDepth = 1;
        ++i;
    }
    return scopes;
}
}
