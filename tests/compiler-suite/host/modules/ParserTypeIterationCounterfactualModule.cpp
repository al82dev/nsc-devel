#include "ParserFinalTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;
using nativesharp::ast::TypeSyntax;
using nativesharp::lexer::TokenKind;

constexpr std::size_t kTypeDepth = 32768;
constexpr std::string_view kProbe = "TP-PAR-541-depth-32768";

std::string nestedGenericType()
{
    std::string source;
    source.reserve(kTypeDepth * 5U + 3U);
    for (std::size_t index = 0; index < kTypeDepth; ++index) source += "Box<";
    source += "int";
    source.append(kTypeDepth, '>');
    return source;
}

std::string nestedFunctionType()
{
    std::string source;
    source.reserve(kTypeDepth * 7U + 3U);
    for (std::size_t index = 0; index < kTypeDepth; ++index) source += "fn*()->";
    source += "int";
    return source;
}

std::string nestedTupleType()
{
    std::string source(kTypeDepth, '(');
    source += "int";
    for (std::size_t index = 0; index < kTypeDepth; ++index) source += ",)";
    return source;
}

std::size_t genericDepth(const TypeSyntax* type)
{
    std::size_t depth = 0;
    while (type != nullptr && type->baseName() == "Box" && type->typeArguments().size() == 1U)
    {
        ++depth;
        type = type->typeArguments().front();
    }
    return type != nullptr && type->baseTokenKind() == TokenKind::KwInt ? depth : 0U;
}

std::size_t functionDepth(const TypeSyntax* type)
{
    std::size_t depth = 0;
    while (type != nullptr && type->isFunction())
    {
        ++depth;
        type = type->functionReturnType();
    }
    return type != nullptr && type->baseTokenKind() == TokenKind::KwInt ? depth : 0U;
}

std::size_t tupleDepth(const TypeSyntax* type)
{
    std::size_t depth = 0;
    while (type != nullptr && type->isTuple() && type->typeArguments().size() == 1U)
    {
        ++depth;
        type = type->typeArguments().front();
    }
    return type != nullptr && type->baseTokenKind() == TokenKind::KwInt ? depth : 0U;
}

bool parseDeepType(
    std::string source,
    std::vector<std::string> knownTypes,
    std::size_t (*measure)(const TypeSyntax*))
{
    ParserFixture fixture(std::move(source), std::move(knownTypes));
    std::size_t afterType = 0;
    const bool scanned = fixture.parser().scanTypeAt(0, afterType);
    TypeSyntax* type = fixture.parser().parseType();
    const std::size_t eof = fixture.tokens().size() - 1U;
    return scanned && afterType == eof && type != nullptr && measure(type) == kTypeDepth &&
        fixture.parser().index_ == eof && fixture.diagnostics().diagnostics().empty();
}

void testIterativeTypeDepth(TestContext& context)
{
    if (isolated_process_support::probeMode() != kProbe)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-541", kProbe);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-541 depth 32768 must complete in the isolated parser process; started=" +
                std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
        return;
    }
    const bool generic = parseDeepType(nestedGenericType(), {"Box"}, genericDepth);
    const bool function = parseDeepType(nestedFunctionType(), {}, functionDepth);
    const bool tuple = parseDeepType(nestedTupleType(), {}, tupleDepth);
    context.expect(generic && function && tuple,
        "TP-PAR-541 generic, function and tuple types at depth 32768 must preserve scan cursor, "
        "AST shape and diagnostics without native-stack recursion");
}
}

void registerParserTypeIterationCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-541",
        "CP-PAR-042",
        testIterativeTypeDepth,
        "NIGHTLY");
}
}
