#include "nativesharp/AST/AstDumper.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <vector>

namespace
{
class UserValueInterpolationHandler final
    : public nativesharp::language::StaticInterpolationHandler
{
public:
    bool appendLiteral(std::string_view text, std::string&) override
    {
        result.append(text);
        return true;
    }

    bool appendHole(
        const nativesharp::language::InterpolationPart& hole,
        std::string& error) override
    {
        if (hole.text != "user" || hole.format != "tag")
        {
            error = "unexpected user interpolation hole";
            return false;
        }
        result += "<user-value>";
        return true;
    }

    std::string result;
};

struct Parsed final
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    std::vector<nativesharp::lexer::Token> tokens;
    nativesharp::ast::AstArena arena;
    nativesharp::ast::AstNode* root = nullptr;
};

void parse(
    Parsed& parsed,
    std::string source)
{
    const nativesharp::FileId id = parsed.sources.addBuffer("t01.ns", std::move(source));
    nativesharp::lexer::Lexer lexer(*parsed.sources.getFile(id), parsed.diagnostics);
    parsed.tokens = lexer.lexAll();
    const std::array<std::string, 1> knownTypes = {"ReadOnlySpan"};
    nativesharp::parser::Parser parser(
        parsed.tokens, parsed.arena, parsed.diagnostics, knownTypes);
    parsed.root = parser.parseCompilationUnit();
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp;
    using language::ExpressionFeature;

    for (std::size_t value = 0; value <= static_cast<std::size_t>(ExpressionFeature::WithExpressions); ++value)
    {
        const auto feature = static_cast<ExpressionFeature>(value);
        const auto& contract = language::expressionLoweringContract(feature);
        tests.expect(contract.feature == feature && contract.sourceOrder &&
                contract.primaryEvaluatedOnce && !contract.mayAllocateImplicitly && contract.preservesAbi,
            "every T01 lowering contract is ordered, single-evaluation, allocation-free and ABI-preserving");
        tests.expect(language::expressionFeatureAvailable(feature),
            "every T01 feature belongs to the cumulative language surface");
    }

    tests.expect(language::isTypedParameterPackType("type:NativeSharp.Core.ReadOnlySpan<builtin:int>"),
        "typed params recognizes ReadOnlySpan<T>");
    tests.expect(!language::isTypedParameterPackType("builtin:int[]"),
        "typed params never treats a heap array as a parameter pack");
    tests.expect(language::supportsNullAwareOperators("builtin:int*") &&
            language::supportsNullAwareOperators("type:Widget") &&
            !language::supportsNullAwareOperators("builtin:int"),
        "null-aware classification accepts nullable carriers and rejects values");

    const auto range = language::resolveHalfOpenRange(
        language::RangeEndpoint{1, false}, language::RangeEndpoint{1, true}, 8);
    tests.expect(range.has_value() && range->start == 1 && range->end == 7,
        "half-open range resolves from-end endpoints");
    tests.expect(!language::resolveHalfOpenRange(
            language::RangeEndpoint{7, false}, language::RangeEndpoint{3, false}, 8).has_value(),
        "range resolution rejects reversed bounds");

    std::vector<language::InterpolationPart> parts;
    std::string interpolationError;
    tests.expect(language::parseStaticInterpolation(
            "left={{ {value:D} }}", parts, interpolationError) && parts.size() == 3 &&
            parts[1].kind == language::InterpolationPartKind::Hole &&
            parts[1].text == "value" && parts[1].format == "D",
        "static interpolation separates escaped literals, holes and constant format text");
    tests.expect(!language::parseStaticInterpolation("bad={}", parts, interpolationError) &&
            !interpolationError.empty(),
        "static interpolation rejects an empty hole deterministically");

    tests.expect(language::parseStaticInterpolation(
            "value={-42:D5}; hex={31:X4}; zero={-0:D2}", parts, interpolationError),
        "built-in static handler input parses");
    std::string rendered;
    tests.expect(language::renderBuiltinStaticInterpolation(parts, rendered, interpolationError) &&
            rendered == "value=-0042; hex=001F; zero=00",
        "built-in static handler owns sign-aware width, casing, and canonical zero");

    tests.expect(language::parseStaticInterpolation(
            "left={user:tag};right", parts, interpolationError),
        "custom compile-time handler input parses");
    UserValueInterpolationHandler userHandler;
    tests.expect(language::renderStaticInterpolation(parts, userHandler, interpolationError) &&
            userHandler.result == "left=<user-value>;right",
        "a stack-selected user handler formats a user value without String or runtime culture");

    constexpr std::string_view source = R"ns(
struct Box { public int Value; }
int Consume(params ReadOnlySpan<int> values) { return 0; }
int Demo(int* left, int* right, int[] items, Box value)
{
    int* chosen = left ?? right;
    chosen ??= right;
    int last = items[^1];
    int[] cut = items[1..^1];
    Box changed = value with { Value = 7 };
    Box initialized = { Value = 1, ..value };
    string folded = $"constant={42:D}";
    string text = $"last={last:D}";
    Consume(values: cut);
    return chosen?.Value;
}
)ns";

    Parsed parsed;
    parse(parsed, std::string(source));
    tests.expect(parsed.root != nullptr && parsed.diagnostics.errorCount() == 0,
        "T01 surface syntax parses without edition-dependent diagnostics");
    std::ostringstream dump;
    ast::AstDumper{}.dump(parsed.root, dump);
    const std::string tree = dump.str();
    tests.expect(tree.find("NullCoalescingExpression") != std::string::npos &&
            tree.find("op=QuestionQuestionEqual") != std::string::npos &&
            tree.find("NullConditionalMemberExpression") != std::string::npos,
        "null-aware syntax keeps explicit AST nodes");
    tests.expect(tree.find("RangeExpression") != std::string::npos &&
            tree.find("FromEndExpression") != std::string::npos,
        "range and from-end syntax keep explicit AST nodes");
    tests.expect(tree.find("WithExpression") != std::string::npos &&
            tree.find("SpreadElementExpression") != std::string::npos &&
            tree.find("InterpolatedStringExpression") != std::string::npos &&
            tree.find("aux=\"runtime-interpolation\"") != std::string::npos,
        "with, spread, static interpolation, and runtime interpolation survive parsing");
    tests.expect(tree.find("aux=\"params\"") != std::string::npos &&
            tree.find("name=\"values\"") != std::string::npos,
        "typed packs and named arguments are present in the AST dump");

    return tests.result();
}
