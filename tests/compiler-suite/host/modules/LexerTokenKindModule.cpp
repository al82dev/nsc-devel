#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Lexer/TokenKind.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::lexer::TokenKind;

// Independent catalog oracle frozen by CL-LEX-001. Its positional order validates both the
// public spelling and the serialized ordinal identity of every token kind.
constexpr std::array<std::string_view, 150> ExpectedTokenKindNames = {
    "EndOfFile",
    "Invalid",
    "Identifier",
    "IntegerLiteral",
    "FloatingLiteral",
    "CharacterLiteral",
    "StringLiteral",
    "AssemblyText",
    "AssemblyNewLine",
    "KwVoid",
    "KwBool",
    "KwSByte",
    "KwByte",
    "KwChar",
    "KwShort",
    "KwUShort",
    "KwInt",
    "KwUInt",
    "KwLong",
    "KwULong",
    "KwNInt",
    "KwNUInt",
    "KwInt128",
    "KwUInt128",
    "KwFloat",
    "KwDouble",
    "KwString",
    "KwConst",
    "KwStatic",
    "KwReadonly",
    "KwVolatile",
    "KwInline",
    "KwForceInline",
    "KwNoInline",
    "KwNoHeap",
    "KwPublic",
    "KwInternal",
    "KwProtected",
    "KwPrivate",
    "KwIf",
    "KwElse",
    "KwWhile",
    "KwDo",
    "KwFor",
    "KwSwitch",
    "KwCase",
    "KwBreak",
    "KwContinue",
    "KwReturn",
    "KwTry",
    "KwCatch",
    "KwFinally",
    "KwThrow",
    "KwStruct",
    "KwClass",
    "KwInterface",
    "KwEnum",
    "KwNamespace",
    "KwUsing",
    "KwNew",
    "KwDelete",
    "KwDestroy",
    "KwStackalloc",
    "KwSizeof",
    "KwAlignof",
    "KwOffsetof",
    "KwStaticAssert",
    "KwDefault",
    "KwExtern",
    "KwExport",
    "KwC",
    "KwVectorCall",
    "KwTrue",
    "KwFalse",
    "KwNull",
    "KwThis",
    "KwBase",
    "KwVirtual",
    "KwOverride",
    "KwAbstract",
    "KwSealed",
    "KwGet",
    "KwSet",
    "KwOperator",
    "KwImplicit",
    "KwExplicit",
    "KwIs",
    "KwAs",
    "KwDelegate",
    "KwEvent",
    "KwRaii",
    "KwMove",
    "KwOwned",
    "KwBorrowed",
    "KwVar",
    "KwForeach",
    "KwIn",
    "LeftParen",
    "RightParen",
    "LeftBrace",
    "RightBrace",
    "LeftBracket",
    "RightBracket",
    "Semicolon",
    "Comma",
    "Colon",
    "Question",
    "Dot",
    "Ellipsis",
    "Arrow",
    "FatArrow",
    "Plus",
    "Minus",
    "Star",
    "Slash",
    "Percent",
    "PlusPlus",
    "MinusMinus",
    "EqualEqual",
    "BangEqual",
    "Less",
    "LessEqual",
    "Greater",
    "GreaterEqual",
    "Bang",
    "AmpAmp",
    "PipePipe",
    "Amp",
    "Pipe",
    "Caret",
    "Tilde",
    "LessLess",
    "GreaterGreater",
    "Equal",
    "PlusEqual",
    "MinusEqual",
    "StarEqual",
    "SlashEqual",
    "PercentEqual",
    "AmpEqual",
    "PipeEqual",
    "CaretEqual",
    "LessLessEqual",
    "GreaterGreaterEqual",
    "InterpolatedStringLiteral",
    "QuestionQuestion",
    "QuestionQuestionEqual",
    "QuestionDot",
    "DotDot",
    "DotDotEqual",
};

// Registers one CL-LEX-001 case without duplicating common registry metadata.
void addTokenKindCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LEX-001", "Host.Lexer", "component", "PR",
        {"lexer", "token-kind"}, std::move(function)});
}

// Validates every frozen ordinal/name pair and rejects duplicate public spellings.
void testTokenKindCatalog(TestContext& context)
{
    std::unordered_set<std::string_view> uniqueNames;
    for (std::size_t index = 0; index < ExpectedTokenKindNames.size(); ++index)
    {
        const auto kind = static_cast<TokenKind>(index);
        const std::string_view actualName = nativesharp::lexer::tokenKindName(kind);
        context.expect(actualName == ExpectedTokenKindNames[index],
            "TL-LEX-001 token name differs at ordinal " + std::to_string(index));
        context.expect(uniqueNames.insert(actualName).second,
            "TL-LEX-001 token names must be unique at ordinal " + std::to_string(index));
    }
}

// Proves that values outside the catalog use the stable Invalid fallback.
void testUnknownTokenKindName(TestContext& context)
{
    const auto belowCatalog = static_cast<TokenKind>(-1);
    const auto aboveCatalog = static_cast<TokenKind>(ExpectedTokenKindNames.size());
    context.expect(nativesharp::lexer::tokenKindName(belowCatalog) == "Invalid" &&
            nativesharp::lexer::tokenKindName(aboveCatalog) == "Invalid",
        "TL-LEX-002 unknown token kinds must use the Invalid name");
}

// Checks the complete builtin classifier domain rather than sampling its endpoints.
void testBuiltinTypeClassifier(TestContext& context)
{
    for (std::size_t index = 0; index < ExpectedTokenKindNames.size(); ++index)
    {
        const auto kind = static_cast<TokenKind>(index);
        const bool expected = kind >= TokenKind::KwVoid && kind <= TokenKind::KwString;
        context.expect(nativesharp::lexer::isBuiltinTypeKeyword(kind) == expected,
            "TL-LEX-003 builtin classifier differs at ordinal " + std::to_string(index));
    }
}

// Checks that ownership classification contains exactly owned and borrowed.
void testOwnershipQualifierClassifier(TestContext& context)
{
    for (std::size_t index = 0; index < ExpectedTokenKindNames.size(); ++index)
    {
        const auto kind = static_cast<TokenKind>(index);
        const bool expected = kind == TokenKind::KwOwned || kind == TokenKind::KwBorrowed;
        context.expect(nativesharp::lexer::isOwnershipQualifierKeyword(kind) == expected,
            "TL-LEX-004 ownership classifier differs at ordinal " + std::to_string(index));
    }
}

// Verifies the complete positive assignment-operator set from the frozen contract.
void testAssignmentOperators(TestContext& context)
{
    constexpr std::array assignments = {
        TokenKind::Equal, TokenKind::PlusEqual, TokenKind::MinusEqual, TokenKind::StarEqual,
        TokenKind::SlashEqual, TokenKind::PercentEqual, TokenKind::AmpEqual, TokenKind::PipeEqual,
        TokenKind::CaretEqual, TokenKind::LessLessEqual, TokenKind::GreaterGreaterEqual,
        TokenKind::QuestionQuestionEqual,
    };
    for (const TokenKind kind : assignments)
    {
        context.expect(nativesharp::lexer::isAssignmentOperator(kind),
            "TL-LEX-005 every audited assignment token must classify as assignment");
    }
}

// Verifies that no catalog member outside the positive set is classified as assignment.
void testNonAssignmentOperators(TestContext& context)
{
    for (std::size_t index = 0; index < ExpectedTokenKindNames.size(); ++index)
    {
        const auto kind = static_cast<TokenKind>(index);
        const bool expected = kind == TokenKind::Equal ||
            (kind >= TokenKind::PlusEqual && kind <= TokenKind::GreaterGreaterEqual) ||
            kind == TokenKind::QuestionQuestionEqual;
        context.expect(nativesharp::lexer::isAssignmentOperator(kind) == expected,
            "TL-LEX-006 assignment rejection differs at ordinal " + std::to_string(index));
    }
}

// Validates the ten classic compound-to-base mappings exactly.
void testCompoundAssignmentBases(TestContext& context)
{
    constexpr std::array mappings = {
        std::pair{TokenKind::PlusEqual, TokenKind::Plus},
        std::pair{TokenKind::MinusEqual, TokenKind::Minus},
        std::pair{TokenKind::StarEqual, TokenKind::Star},
        std::pair{TokenKind::SlashEqual, TokenKind::Slash},
        std::pair{TokenKind::PercentEqual, TokenKind::Percent},
        std::pair{TokenKind::AmpEqual, TokenKind::Amp},
        std::pair{TokenKind::PipeEqual, TokenKind::Pipe},
        std::pair{TokenKind::CaretEqual, TokenKind::Caret},
        std::pair{TokenKind::LessLessEqual, TokenKind::LessLess},
        std::pair{TokenKind::GreaterGreaterEqual, TokenKind::GreaterGreater},
    };
    for (const auto& [compound, base] : mappings)
    {
        context.expect(nativesharp::lexer::compoundAssignmentBaseOperator(compound) == base,
            "TL-LEX-007 compound assignment must map to its exact base operator");
    }
}

// Checks the full catalog complement, including plain and null-coalescing assignment.
void testNonCompoundAssignmentBases(TestContext& context)
{
    constexpr std::array compounds = {
        TokenKind::PlusEqual, TokenKind::MinusEqual, TokenKind::StarEqual, TokenKind::SlashEqual,
        TokenKind::PercentEqual, TokenKind::AmpEqual, TokenKind::PipeEqual, TokenKind::CaretEqual,
        TokenKind::LessLessEqual, TokenKind::GreaterGreaterEqual,
    };
    for (std::size_t index = 0; index < ExpectedTokenKindNames.size(); ++index)
    {
        const auto kind = static_cast<TokenKind>(index);
        bool isCompound = false;
        for (const TokenKind compound : compounds)
        {
            isCompound = isCompound || kind == compound;
        }
        if (!isCompound)
        {
            context.expect(nativesharp::lexer::compoundAssignmentBaseOperator(kind) == TokenKind::Invalid,
                "TL-LEX-008 every non-compound token must map to Invalid");
        }
    }
}
}

// Adds the complete CL-LEX-001 catalog and classifier case set to the consolidated host registry.
void registerLexerTokenKindModule(TestRegistry& registry)
{
    addTokenKindCase(registry, "TL-LEX-001", testTokenKindCatalog);
    addTokenKindCase(registry, "TL-LEX-002", testUnknownTokenKindName);
    addTokenKindCase(registry, "TL-LEX-003", testBuiltinTypeClassifier);
    addTokenKindCase(registry, "TL-LEX-004", testOwnershipQualifierClassifier);
    addTokenKindCase(registry, "TL-LEX-005", testAssignmentOperators);
    addTokenKindCase(registry, "TL-LEX-006", testNonAssignmentOperators);
    addTokenKindCase(registry, "TL-LEX-007", testCompoundAssignmentBases);
    addTokenKindCase(registry, "TL-LEX-008", testNonCompoundAssignmentBases);
}
}
