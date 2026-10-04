#include "nativesharp/Frontend/ComptimeEngine.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Language/MetaprogrammingSemantics.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>
#include <span>

namespace nativesharp::frontend
{
namespace
{
bool asciiSpace(char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
        value == '\f' || value == '\v';
}

bool asciiDigit(char value) noexcept { return value >= '0' && value <= '9'; }
bool asciiAlpha(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}
bool asciiAlnum(char value) noexcept { return asciiAlpha(value) || asciiDigit(value); }
bool identifierByte(char value) noexcept
{
    return asciiAlnum(value) || value == '_';
}

class IntegerExpression final
{
public:
    IntegerExpression(
        std::string_view text,
        std::size_t maximumSteps,
        std::size_t maximumNesting,
        std::size_t& steps)
        : text_(text), maximumSteps_(maximumSteps), maximumNesting_(maximumNesting), steps_(steps)
    {
    }

    std::optional<std::int64_t> evaluate(std::string& error)
    {
        const auto value = parseBitwiseOr(error);
        skipSpace();
        if (value.has_value() && cursor_ != text_.size())
        {
            error = "unexpected token in #run expression at byte " + std::to_string(cursor_);
            return std::nullopt;
        }
        return value;
    }

private:
    bool tick(std::string& error)
    {
        if (steps_ >= maximumSteps_)
        {
            error = "comptime step budget exceeded";
            return false;
        }
        ++steps_;
        return true;
    }

    void skipSpace()
    {
        while (cursor_ < text_.size() && asciiSpace(text_[cursor_]))
            ++cursor_;
    }

    bool take(std::string_view token)
    {
        skipSpace();
        if (text_.substr(cursor_, token.size()) != token) return false;
        cursor_ += token.size();
        return true;
    }

    std::optional<std::int64_t> parsePrimary(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        skipSpace();
        if (take("("))
        {
            if (nesting_ >= maximumNesting_)
            {
                error = "comptime nesting limit exceeded";
                return std::nullopt;
            }
            ++nesting_;
            auto value = parseBitwiseOr(error);
            --nesting_;
            if (!value.has_value() || !take(")"))
            {
                if (error.empty()) error = "expected ')' in #run expression";
                return std::nullopt;
            }
            return value;
        }
        const std::size_t begin = cursor_;
        if (cursor_ < text_.size() &&
            (asciiAlpha(text_[cursor_]) ||
             text_[cursor_] == '_'))
        {
            error = "comptime sandbox forbids identifiers, calls, FFI, and host access";
            return std::nullopt;
        }
        int base = 10;
        if (cursor_ + 2 <= text_.size() && text_[cursor_] == '0' &&
            (text_[cursor_ + 1] == 'x' || text_[cursor_ + 1] == 'X'))
        {
            base = 16;
            cursor_ += 2;
        }
        const std::size_t digits = cursor_;
        while (cursor_ < text_.size() &&
               (asciiAlnum(text_[cursor_]) ||
                text_[cursor_] == '_'))
            ++cursor_;
        if (cursor_ == digits)
        {
            error = "expected integer expression after #run(";
            return std::nullopt;
        }
        std::string clean;
        for (char value : text_.substr(digits, cursor_ - digits))
            if (value != '_') clean.push_back(value);
        std::uint64_t parsed = 0;
        const auto converted = std::from_chars(clean.data(), clean.data() + clean.size(), parsed, base);
        if (converted.ec != std::errc{} || converted.ptr != clean.data() + clean.size() ||
            parsed > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            error = "invalid signed 64-bit integer in #run expression at byte " +
                std::to_string(begin);
            return std::nullopt;
        }
        return static_cast<std::int64_t>(parsed);
    }

    std::optional<std::int64_t> parseUnary(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        if (take("+")) return parseUnary(error);
        if (take("-"))
        {
            auto value = parseUnary(error);
            if (!value.has_value()) return std::nullopt;
            if (*value == std::numeric_limits<std::int64_t>::min())
            {
                error = "signed overflow in #run unary '-'";
                return std::nullopt;
            }
            return -*value;
        }
        if (take("~"))
        {
            auto value = parseUnary(error);
            return value.has_value() ? std::optional<std::int64_t>(~*value) : std::nullopt;
        }
        return parsePrimary(error);
    }

    std::optional<std::int64_t> parseMultiply(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto left = parseUnary(error);
        while (left.has_value())
        {
            char op = 0;
            if (take("*")) op = '*';
            else if (take("/")) op = '/';
            else if (take("%")) op = '%';
            else break;
            auto right = parseUnary(error);
            if (!right.has_value()) return std::nullopt;
            if ((op == '/' || op == '%') && *right == 0)
            {
                error = "division by zero in #run expression";
                return std::nullopt;
            }
            if ((op == '/' || op == '%') &&
                *left == std::numeric_limits<std::int64_t>::min() && *right == -1)
            {
                error = "signed overflow in #run expression";
                return std::nullopt;
            }
            if (op == '*')
            {
                std::int64_t product = 0;
                if (!__builtin_mul_overflow(*left, *right, &product)) left = product;
                else { error = "signed overflow in #run expression"; return std::nullopt; }
            }
            else if (op == '/') left = *left / *right;
            else left = *left % *right;
        }
        return left;
    }

    std::optional<std::int64_t> parseAdd(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto left = parseMultiply(error);
        while (left.has_value())
        {
            char op = 0;
            if (take("+")) op = '+';
            else if (take("-")) op = '-';
            else break;
            auto right = parseMultiply(error);
            if (!right.has_value()) return std::nullopt;
            std::int64_t result = 0;
            const bool overflow = op == '+' ? __builtin_add_overflow(*left, *right, &result)
                                            : __builtin_sub_overflow(*left, *right, &result);
            if (overflow) { error = "signed overflow in #run expression"; return std::nullopt; }
            left = result;
        }
        return left;
    }

    std::optional<std::int64_t> parseShift(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto left = parseAdd(error);
        while (left.has_value())
        {
            bool rightShift = false;
            if (take("<<")) {}
            else if (take(">>")) rightShift = true;
            else break;
            auto right = parseAdd(error);
            if (!right.has_value()) return std::nullopt;
            if (*right < 0 || *right >= 64)
            {
                error = "shift count outside 0..63 in #run expression";
                return std::nullopt;
            }
            const auto bits = static_cast<std::uint64_t>(*left);
            left = static_cast<std::int64_t>(rightShift ? bits >> *right : bits << *right);
        }
        return left;
    }

    std::optional<std::int64_t> parseBitwiseAnd(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto value = parseShift(error);
        while (value.has_value() && take("&"))
        {
            auto right = parseShift(error);
            if (!right.has_value()) return std::nullopt;
            value = *value & *right;
        }
        return value;
    }

    std::optional<std::int64_t> parseBitwiseXor(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto value = parseBitwiseAnd(error);
        while (value.has_value() && take("^"))
        {
            auto right = parseBitwiseAnd(error);
            if (!right.has_value()) return std::nullopt;
            value = *value ^ *right;
        }
        return value;
    }

    std::optional<std::int64_t> parseBitwiseOr(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto value = parseBitwiseXor(error);
        while (value.has_value() && take("|"))
        {
            auto right = parseBitwiseXor(error);
            if (!right.has_value()) return std::nullopt;
            value = *value | *right;
        }
        return value;
    }

    std::string_view text_;
    std::size_t cursor_ = 0;
    std::size_t maximumSteps_ = 0;
    std::size_t maximumNesting_ = 0;
    std::size_t& steps_;
    std::size_t nesting_ = 0;
};

bool startsWord(std::string_view source, std::size_t offset, std::string_view word)
{
    if (source.substr(offset, word.size()) != word) return false;
    if (offset != 0 && identifierByte(source[offset - 1])) return false;
    const std::size_t after = offset + word.size();
    return after >= source.size() || !identifierByte(source[after]);
}

std::string_view trim(std::string_view text) noexcept
{
    while (!text.empty() && asciiSpace(text.front())) text.remove_prefix(1);
    while (!text.empty() && asciiSpace(text.back())) text.remove_suffix(1);
    return text;
}

bool attributeIdentity(std::string_view text) noexcept
{
    if (text.empty() || (!asciiAlpha(text.front()) && text.front() != '_')) return false;
    for (const char value : text)
        if (!identifierByte(value) && value != '.') return false;
    return true;
}

std::optional<std::string_view> optionValue(std::string_view text, std::string_view option)
{
    const std::string needle = std::string(option) + "(";
    const std::size_t begin = text.find(needle);
    if (begin == std::string_view::npos) return std::nullopt;
    const std::size_t valueBegin = begin + needle.size();
    const std::size_t end = text.find(')', valueBegin);
    if (end == std::string_view::npos) return std::nullopt;
    return trim(text.substr(valueBegin, end - valueBegin));
}

bool parseConstantArguments(
    std::string_view text,
    std::vector<std::string>& arguments,
    std::string& error)
{
    text = trim(text);
    if (text.empty()) return true;
    std::size_t begin = 0;
    bool string = false;
    bool escaped = false;
    for (std::size_t cursor = 0; cursor <= text.size(); ++cursor)
    {
        const char value = cursor < text.size() ? text[cursor] : ',';
        if (string)
        {
            if (escaped) escaped = false;
            else if (value == '\\') escaped = true;
            else if (value == '"') string = false;
            continue;
        }
        if (value == '"') { string = true; continue; }
        if (value != ',') continue;
        const std::string_view item = trim(text.substr(begin, cursor - begin));
        const bool quoted = item.size() >= 2 && item.front() == '"' && item.back() == '"';
        const bool boolean = item == "true" || item == "false";
        std::size_t digit = (!item.empty() && (item.front() == '+' || item.front() == '-')) ? 1 : 0;
        bool integer = digit < item.size();
        for (; integer && digit < item.size(); ++digit) integer = asciiDigit(item[digit]);
        if ((!quoted && !boolean && !integer) || string)
        {
            error = "attribute arguments must be string, boolean, or integer constants";
            return false;
        }
        arguments.emplace_back(item);
        begin = cursor + 1;
    }
    return true;
}

bool parseAttributeDirective(
    std::string_view payload,
    std::size_t offset,
    std::size_t length,
    ComptimeAttributeDirective& directive,
    std::string& error)
{
    payload = trim(payload);
    directive.offset = offset;
    directive.length = length;
    if (payload.starts_with("attribute "))
    {
        directive.isDefinition = true;
        payload.remove_prefix(10);
        payload = trim(payload);
        const std::size_t separator = payload.find_first_of(" \t\r\n");
        const std::string_view identity = payload.substr(0, separator);
        if (!attributeIdentity(identity)) { error = "invalid attribute definition identity"; return false; }
        directive.identity.assign(identity);
        const std::string_view options = separator == std::string_view::npos
            ? std::string_view{} : payload.substr(separator + 1);
        const auto targets = optionValue(options, "targets");
        if (!targets.has_value()) { error = "attribute definition requires targets(...)"; return false; }
        std::size_t begin = 0;
        while (begin <= targets->size())
        {
            const std::size_t pipe = targets->find('|', begin);
            const std::string_view target = trim(targets->substr(begin, pipe - begin));
            if (target == "type") directive.targets.push_back(ComptimeAttributeTarget::Type);
            else if (target == "field") directive.targets.push_back(ComptimeAttributeTarget::Field);
            else if (target == "method") directive.targets.push_back(ComptimeAttributeTarget::Method);
            else { error = "unknown attribute target '" + std::string(target) + "'"; return false; }
            if (pipe == std::string_view::npos) break;
            begin = pipe + 1;
        }
        if (const auto repeatable = optionValue(options, "repeatable"); repeatable.has_value())
        {
            if (*repeatable == "true") directive.repeatable = true;
            else if (*repeatable != "false") { error = "repeatable(...) expects true or false"; return false; }
        }
        if (const auto retention = optionValue(options, "retention"); retention.has_value())
        {
            if (*retention == "runtime")
                directive.retention = ComptimeAttributeRetention::NativeRuntimeSection;
            else if (*retention != "compiletime")
            { error = "retention(...) expects compiletime or runtime"; return false; }
        }
        return true;
    }

    const std::size_t open = payload.find('(');
    const std::size_t close = payload.rfind(')');
    const std::string_view identity = trim(payload.substr(0, open));
    if (!attributeIdentity(identity)) { error = "invalid attribute use identity"; return false; }
    if ((open == std::string_view::npos) != (close == std::string_view::npos) ||
        (open != std::string_view::npos && close != payload.size() - 1))
    { error = "malformed attribute constant argument list"; return false; }
    directive.identity.assign(identity);
    return open == std::string_view::npos ||
        parseConstantArguments(payload.substr(open + 1, close - open - 1), directive.constantArguments, error);
}

bool parseAuthoredDiagnostic(
    std::string_view source,
    std::size_t cursor,
    std::string_view directiveName,
    const std::vector<MacroExpansionRecord>& macroExpansions,
    ComptimeAuthoredDiagnostic& diagnostic,
    std::size_t& end,
    std::string& error)
{
    std::size_t open = cursor + directiveName.size();
    while (open < source.size() && asciiSpace(source[open])) ++open;
    if (open == source.size() || source[open] != '(')
    { error = "expected '(' after " + std::string(directiveName); return false; }
    std::size_t close = open + 1;
    unsigned depth = 1;
    bool quoted = false;
    bool escaped = false;
    for (; close < source.size() && depth != 0; ++close)
    {
        const char value = source[close];
        if (quoted)
        {
            if (escaped) escaped = false;
            else if (value == '\\') escaped = true;
            else if (value == '"') quoted = false;
            continue;
        }
        if (value == '"') { quoted = true; continue; }
        if (value == '(') ++depth;
        else if (value == ')') --depth;
    }
    if (depth != 0) { error = "unterminated authored diagnostic"; return false; }
    end = close;
    std::vector<std::string> arguments;
    if (!parseConstantArguments(source.substr(open + 1, end - open - 2), arguments, error))
        return false;
    if (arguments.size() < 2 || arguments.size() > 4 || arguments[0].size() < 2 ||
        arguments[0].front() != '"' || arguments[0].back() != '"' ||
        arguments[1].size() < 2 || arguments[1].front() != '"' || arguments[1].back() != '"')
    {
        error = "authored diagnostic expects string message, string package code, and optional span offset/length";
        return false;
    }
    diagnostic.message = arguments[0].substr(1, arguments[0].size() - 2);
    diagnostic.code = arguments[1].substr(1, arguments[1].size() - 2);
    const std::size_t separator = diagnostic.code.find('-');
    const std::string package = separator == std::string::npos
        ? std::string{} : diagnostic.code.substr(0, separator);
    if (!language::validPackageDiagnosticCode(package, diagnostic.code))
    { error = "package diagnostic code cannot be empty or collide with NS3xxx"; return false; }
    diagnostic.offset = cursor;
    diagnostic.length = end - cursor;
    if (arguments.size() >= 3)
    {
        std::size_t parsed = 0;
        const auto converted = std::from_chars(
            arguments[2].data(), arguments[2].data() + arguments[2].size(), parsed);
        if (converted.ec != std::errc{} || converted.ptr != arguments[2].data() + arguments[2].size())
        { error = "diagnostic span offset must be a non-negative integer"; return false; }
        diagnostic.offset = parsed;
    }
    if (arguments.size() == 4)
    {
        std::size_t parsed = 0;
        const auto converted = std::from_chars(
            arguments[3].data(), arguments[3].data() + arguments[3].size(), parsed);
        if (converted.ec != std::errc{} || converted.ptr != arguments[3].data() + arguments[3].size())
        { error = "diagnostic span length must be a non-negative integer"; return false; }
        diagnostic.length = parsed;
    }
    if (diagnostic.offset > source.size() || diagnostic.length > source.size() - diagnostic.offset)
    { error = "authored diagnostic span lies outside its source file"; return false; }
    diagnostic.severity = directiveName == "#compile_warning"
        ? ComptimeDiagnosticSeverity::Warning
        : directiveName == "#compile_note" ? ComptimeDiagnosticSeverity::Note
                                            : ComptimeDiagnosticSeverity::Error;
    for (const MacroExpansionRecord& expansion : macroExpansions)
        diagnostic.notes.push_back("expanded from macro '" + expansion.macroName +
            "' at byte " + std::to_string(expansion.invocationOffset));
    return true;
}

std::string comptimeCacheKey(std::string_view expression)
{
    std::string payload = "nativesharp-comptime-expression-v1\n";
    payload.append(expression);
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

std::string digestText(std::string_view text)
{
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

bool declaredCapability(
    std::span<const std::pair<std::string, std::string>> declarations,
    std::string_view kind,
    std::string_view identity)
{
    return std::ranges::any_of(declarations, [&](const auto& declaration)
    {
        return declaration.first == kind && declaration.second == identity;
    });
}

bool resolveCapabilityCalls(
    std::string_view expression,
    std::span<const std::pair<std::string, std::string>> declarations,
    const ComptimeExecutionContext& context,
    std::string& resolved,
    std::vector<ComptimeDependency>& dependencies,
    std::string& error)
{
    resolved.assign(expression);
    struct Call final { std::string_view name; std::string_view kind; };
    static constexpr Call calls[] = {{"file_i64", "file"}, {"env_i64", "env"}};
    for (const Call call : calls)
    {
        std::size_t search = 0;
        while ((search = resolved.find(call.name, search)) != std::string::npos)
        {
            std::size_t cursor = search + call.name.size();
            while (cursor < resolved.size() && asciiSpace(resolved[cursor])) ++cursor;
            if (cursor >= resolved.size() || resolved[cursor] != '(')
            { search += call.name.size(); continue; }
            ++cursor;
            while (cursor < resolved.size() && asciiSpace(resolved[cursor])) ++cursor;
            if (cursor >= resolved.size() || resolved[cursor] != '"')
            { error = std::string(call.name) + " requires one string literal identity"; return false; }
            const std::size_t identityBegin = ++cursor;
            while (cursor < resolved.size() && resolved[cursor] != '"') ++cursor;
            if (cursor == resolved.size()) { error = "unterminated capability identity"; return false; }
            const std::string identity = resolved.substr(identityBegin, cursor - identityBegin);
            ++cursor;
            while (cursor < resolved.size() && asciiSpace(resolved[cursor])) ++cursor;
            if (cursor >= resolved.size() || resolved[cursor] != ')')
            { error = std::string(call.name) + " accepts exactly one argument"; return false; }
            const std::size_t callEnd = cursor + 1;
            if (!declaredCapability(declarations, call.kind, identity))
            { error = "comptime capability '" + std::string(call.kind) + ":" + identity + "' was not declared"; return false; }

            std::string content;
            if (call.kind == "file")
            {
                const std::filesystem::path relative(identity);
                if (relative.is_absolute() || identity.find("..") != std::string::npos)
                { error = "comptime file capability must be project-relative and cannot escape its root"; return false; }
                const std::filesystem::path path = (context.sourceDirectory / relative).lexically_normal();
                std::ifstream stream(path, std::ios::binary);
                if (!stream) { error = "cannot read declared comptime file '" + identity + "'"; return false; }
                content.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
            }
            else
            {
                char* value = nullptr;
                std::size_t valueLength = 0;
                const errno_t environmentError = _dupenv_s(&value, &valueLength, identity.c_str());
                if (environmentError != 0 || value == nullptr)
                { error = "declared comptime environment variable is unset: '" + identity + "'"; return false; }
                content.assign(value, valueLength == 0 ? 0 : valueLength - 1);
                std::free(value);
            }
            std::string_view number = trim(content);
            std::int64_t parsed = 0;
            const auto converted = std::from_chars(number.data(), number.data() + number.size(), parsed);
            if (converted.ec != std::errc{} || converted.ptr != number.data() + number.size())
            { error = "declared comptime input '" + identity + "' is not a signed integer"; return false; }
            dependencies.push_back({std::string(call.kind), identity, digestText(content)});
            const std::string replacement = std::to_string(parsed);
            resolved.replace(search, callEnd - search, replacement);
            search += replacement.size();
        }
    }
    if (resolved.find("clock_i64") != std::string::npos ||
        resolved.find("random_i64") != std::string::npos || resolved.find("ffi") != std::string::npos)
    {
        error = context.hermetic
            ? "hermetic comptime forbids clock, randomness, and FFI capabilities"
            : "non-hermetic clock/random/FFI execution requires an out-of-profile host provider";
        return false;
    }
    return true;
}

std::string reproducibilityDigestOf(const ComptimeResult& result)
{
    std::string payload = "nativesharp-comptime-result-v1\n";
    payload.append(result.source);
    payload.push_back('\n');
    for (const ComptimeDependency& dependency : result.dependencies)
    {
        payload.append(dependency.kind).push_back(':');
        payload.append(dependency.identity).push_back(':');
        payload.append(dependency.digest).push_back('\n');
    }
    for (const ComptimeEvaluation& evaluation : result.evaluations)
        payload.append("value:").append(std::to_string(evaluation.value)).push_back('\n');
    for (const ComptimeAttributeDirective& attribute : result.attributes)
    {
        payload.append(attribute.isDefinition ? "attribute-definition:" : "attribute-use:");
        payload.append(attribute.identity).push_back(':');
        for (const std::string& argument : attribute.constantArguments)
            payload.append(argument).push_back(',');
        payload.push_back('\n');
    }
    for (const ComptimeAuthoredDiagnostic& diagnostic : result.authoredDiagnostics)
    {
        payload.append("diagnostic:").append(diagnostic.code).push_back(':');
        payload.append(diagnostic.message).push_back('\n');
    }
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    return basic::sha256Hex(basic::sha256(bytes));
}
}

std::optional<ComptimeCache::Entry> ComptimeCache::find(std::string_view key) const
{
    std::lock_guard lock(mutex_);
    const auto found = entries_.find(std::string(key));
    return found == entries_.end() ? std::nullopt : std::optional<Entry>(found->second);
}

void ComptimeCache::store(std::string key, Entry entry)
{
    std::lock_guard lock(mutex_);
    entries_.insert_or_assign(std::move(key), entry);
}

std::size_t ComptimeCache::size() const
{
    std::lock_guard lock(mutex_);
    return entries_.size();
}

ComptimeEngine::ComptimeEngine(
    ComptimeLimits limits,
    std::shared_ptr<ComptimeCache> cache,
    ComptimeExecutionContext context) noexcept
    : limits_(limits), cache_(std::move(cache)), context_(std::move(context))
{
}

ComptimeResult ComptimeEngine::execute(std::string_view source) const
{
    ComptimeResult result;
    if (source.size() > limits_.maximumSourceBytes)
    {
        result.error = "comptime source byte limit exceeded";
        return result;
    }
    MacroExpansionResult macros = MacroExpansionEngine().expand(source);
    if (!macros.success)
    {
        result.error = macros.error;
        return result;
    }
    result.source = std::move(macros.source);
    result.macroExpansions = std::move(macros.expansions);
    source = result.source;
    enum class Scan { Code, LineComment, BlockComment, String, Character } state = Scan::Code;
    bool escaped = false;
    for (std::size_t cursor = 0; cursor < source.size(); ++cursor)
    {
        const char current = source[cursor];
        const char next = cursor + 1 < source.size() ? source[cursor + 1] : '\0';
        if (state == Scan::LineComment)
        {
            if (current == '\n') state = Scan::Code;
            continue;
        }
        if (state == Scan::BlockComment)
        {
            if (current == '*' && next == '/') { state = Scan::Code; ++cursor; }
            continue;
        }
        if (state == Scan::String || state == Scan::Character)
        {
            if (escaped) { escaped = false; continue; }
            if (current == '\\') { escaped = true; continue; }
            if ((state == Scan::String && current == '"') ||
                (state == Scan::Character && current == '\'')) state = Scan::Code;
            continue;
        }
        if (current == '/' && next == '/') { state = Scan::LineComment; ++cursor; continue; }
        if (current == '/' && next == '*') { state = Scan::BlockComment; ++cursor; continue; }
        if (current == '"') { state = Scan::String; continue; }
        if (current == '\'') { state = Scan::Character; continue; }
        if (startsWord(source, cursor, "comptime"))
        {
            result.source.replace(cursor, 8, 8, ' ');
            cursor += 7;
            continue;
        }
        if (source.substr(cursor, 11) == "#capability")
        {
            std::size_t open = cursor + 11;
            while (open < source.size() && asciiSpace(source[open])) ++open;
            if (open == source.size() || source[open] != '(')
            { result.error = "expected '(' after #capability"; return result; }
            std::size_t close = source.find(')', open + 1);
            if (close == std::string_view::npos)
            { result.error = "unterminated #capability declaration"; return result; }
            std::vector<std::string> arguments;
            std::string error;
            if (!parseConstantArguments(source.substr(open + 1, close - open - 1), arguments, error) ||
                arguments.size() != 2 || arguments[0].size() < 2 || arguments[1].size() < 2 ||
                arguments[0].front() != '"' || arguments[0].back() != '"' ||
                arguments[1].front() != '"' || arguments[1].back() != '"')
            { result.error = "#capability expects string kind and string identity"; return result; }
            const std::string kind = arguments[0].substr(1, arguments[0].size() - 2);
            const std::string identity = arguments[1].substr(1, arguments[1].size() - 2);
            if (kind != "file" && kind != "env" && kind != "clock" && kind != "random" && kind != "ffi")
            { result.error = "unknown comptime capability kind '" + kind + "'"; return result; }
            if (context_.hermetic && kind != "file" && kind != "env")
            { result.error = "hermetic comptime forbids capability kind '" + kind + "'"; return result; }
            result.declaredCapabilities.emplace_back(kind, identity);
            for (std::size_t index = cursor; index <= close; ++index)
                if (result.source[index] != '\r' && result.source[index] != '\n') result.source[index] = ' ';
            cursor = close;
            continue;
        }
        std::string_view diagnosticName;
        if (source.substr(cursor, 14) == "#compile_error") diagnosticName = "#compile_error";
        else if (source.substr(cursor, 16) == "#compile_warning") diagnosticName = "#compile_warning";
        else if (source.substr(cursor, 13) == "#compile_note") diagnosticName = "#compile_note";
        if (!diagnosticName.empty())
        {
            ComptimeAuthoredDiagnostic diagnostic;
            std::size_t end = cursor;
            std::string error;
            if (!parseAuthoredDiagnostic(source, cursor, diagnosticName,
                    result.macroExpansions, diagnostic, end, error))
            {
                result.error = error + " (diagnostic at byte " + std::to_string(cursor) + ")";
                return result;
            }
            result.authoredDiagnostics.push_back(std::move(diagnostic));
            for (std::size_t index = cursor; index < end; ++index)
                if (result.source[index] != '\r' && result.source[index] != '\n') result.source[index] = ' ';
            cursor = end - 1;
            continue;
        }
        if (source.substr(cursor, 2) == "#[")
        {
            std::size_t close = cursor + 2;
            bool inString = false;
            bool stringEscape = false;
            for (; close < source.size(); ++close)
            {
                const char value = source[close];
                if (inString)
                {
                    if (stringEscape) stringEscape = false;
                    else if (value == '\\') stringEscape = true;
                    else if (value == '"') inString = false;
                    continue;
                }
                if (value == '"') { inString = true; continue; }
                if (value == ']') break;
            }
            if (close == source.size())
            {
                result.error = "unterminated attribute directive at byte " + std::to_string(cursor);
                return result;
            }
            ComptimeAttributeDirective directive;
            std::string error;
            const std::size_t length = close + 1 - cursor;
            if (!parseAttributeDirective(
                    source.substr(cursor + 2, close - cursor - 2), cursor, length, directive, error))
            {
                result.error = error + " (attribute at byte " + std::to_string(cursor) + ")";
                return result;
            }
            result.attributes.push_back(std::move(directive));
            for (std::size_t index = cursor; index <= close; ++index)
                if (result.source[index] != '\r' && result.source[index] != '\n') result.source[index] = ' ';
            cursor = close;
            continue;
        }
        if (source.substr(cursor, 4) != "#run" ||
            (cursor != 0 && identifierByte(source[cursor - 1]))) continue;
        if (result.evaluations.size() >= limits_.maximumDirectives)
        {
            result.error = "comptime directive limit exceeded";
            return result;
        }
        std::size_t open = cursor + 4;
        while (open < source.size() && asciiSpace(source[open])) ++open;
        if (open == source.size() || source[open] != '(')
        {
            result.error = "expected '(' after #run at byte " + std::to_string(cursor);
            return result;
        }
        std::size_t close = open + 1;
        unsigned depth = 1;
        for (; close < source.size() && depth != 0; ++close)
        {
            if (source[close] == '(') ++depth;
            else if (source[close] == ')') --depth;
        }
        if (depth != 0)
        {
            result.error = "unterminated #run expression at byte " + std::to_string(cursor);
            return result;
        }
        const std::size_t end = close;
        const std::string_view expression = source.substr(open + 1, end - open - 2);
        std::string resolvedExpression;
        std::vector<ComptimeDependency> externalDependencies;
        std::string capabilityError;
        if (!resolveCapabilityCalls(expression, result.declaredCapabilities, context_,
                resolvedExpression, externalDependencies, capabilityError))
        {
            result.error = capabilityError + " (directive at byte " + std::to_string(cursor) + ")";
            return result;
        }
        std::string cacheMaterial = resolvedExpression;
        for (const ComptimeDependency& dependency : externalDependencies)
            cacheMaterial += "\n" + dependency.kind + ":" + dependency.identity + "=" + dependency.digest;
        const std::string dependencyDigest = comptimeCacheKey(cacheMaterial);
        result.dependencies.push_back({
            "source-expression",
            "#run@" + std::to_string(cursor),
            dependencyDigest});
        result.dependencies.insert(result.dependencies.end(),
            externalDependencies.begin(), externalDependencies.end());
        std::string error;
        std::optional<std::int64_t> value;
        bool cacheHit = false;
        if (cache_)
        {
            if (const auto cached = cache_->find(dependencyDigest); cached.has_value())
            {
                if (cached->executedSteps > limits_.maximumSteps -
                    std::min(result.executedSteps, limits_.maximumSteps))
                {
                    result.error = "comptime step budget exceeded";
                    return result;
                }
                result.executedSteps += cached->executedSteps;
                value = cached->value;
                cacheHit = true;
                ++result.cacheHits;
            }
        }
        if (!value.has_value())
        {
            const std::size_t stepsBefore = result.executedSteps;
            IntegerExpression evaluator(resolvedExpression,
                limits_.maximumSteps, limits_.maximumNesting, result.executedSteps);
            value = evaluator.evaluate(error);
            ++result.cacheMisses;
            if (value.has_value() && cache_)
                cache_->store(dependencyDigest, {*value, result.executedSteps - stepsBefore});
        }
        if (!value.has_value())
        {
            result.error = error + " (directive at byte " + std::to_string(cursor) + ")";
            return result;
        }
        const std::string replacement = std::to_string(*value);
        const std::size_t length = end - cursor;
        if (replacement.size() > length)
        {
            result.error = "#run result does not fit its source span at byte " +
                std::to_string(cursor);
            return result;
        }
        std::string padded = replacement;
        padded.append(length - replacement.size(), ' ');
        result.source.replace(cursor, length, padded);
        result.evaluations.push_back({cursor, length, *value, dependencyDigest, cacheHit});
        cursor = end - 1;
    }
    result.success = true;
    result.reproducibilityDigest = reproducibilityDigestOf(result);
    return result;
}
}
