#include "LexerTestSupport.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{

namespace {

using lexer_test::LexerFixture;
using nativesharp::lexer::TokenKind;

struct OperatorSample {
    std::string_view spelling;
    TokenKind kind;
};

void addCase(TestRegistry& registry,
             std::string id,
             std::string_view contractId,
             std::string_view,
             TestFunction function) {
    registry.add(TestCase{std::move(id), std::string(contractId), "Host.Lexer", "component", "PR",
        {"lexer", "operator", "maximal-munch"}, std::move(function)});
}

void expectToken(TestContext& context,
                 const LexerFixture& fixture,
                 std::size_t index,
                 TokenKind kind,
                 std::uint32_t begin,
                 std::uint32_t end,
                 std::string_view lexeme,
                 std::string_view caseId) {
    lexer_test::expectToken(context, fixture.tokens()[index], kind, begin, end, lexeme, caseId);
}

void expectOperator(TestContext& context, const OperatorSample& sample, std::string_view caseId) {
    LexerFixture fixture(std::string(sample.spelling));
    context.expect(fixture.tokens().size() == 2, std::string(caseId) + " token count must match");
    if (fixture.tokens().size() != 2) {
        return;
    }

    expectToken(context,
                fixture,
                0,
                sample.kind,
                0,
                static_cast<std::uint32_t>(sample.spelling.size()),
                sample.spelling,
                caseId);
    expectToken(context,
                fixture,
                1,
                TokenKind::EndOfFile,
                static_cast<std::uint32_t>(sample.spelling.size()),
                static_cast<std::uint32_t>(sample.spelling.size()),
                "",
                caseId);
    context.expect(fixture.diagnostics().diagnostics().empty(),
                   std::string(caseId) + " must not diagnose a valid operator");
}

void expectSequence(TestContext& context,
                    std::string_view source,
                    const OperatorSample* samples,
                    std::size_t sampleCount,
                    std::string_view caseId) {
    LexerFixture fixture{std::string(source)};
    context.expect(fixture.tokens().size() == sampleCount + 1,
                   std::string(caseId) + " token count must match");
    if (fixture.tokens().size() != sampleCount + 1) {
        return;
    }

    std::size_t cursor = 0;
    for (std::size_t index = 0; index < sampleCount; ++index) {
        while (cursor < source.size() && source[cursor] == ' ') {
            ++cursor;
        }
        const auto end = cursor + samples[index].spelling.size();
        expectToken(context,
                    fixture,
                    index,
                    samples[index].kind,
                    static_cast<std::uint32_t>(cursor),
                    static_cast<std::uint32_t>(end),
                    samples[index].spelling,
                    caseId);
        cursor = end;
    }
    expectToken(context,
                fixture,
                sampleCount,
                TokenKind::EndOfFile,
                static_cast<std::uint32_t>(source.size()),
                static_cast<std::uint32_t>(source.size()),
                "",
                caseId);
    context.expect(fixture.diagnostics().diagnostics().empty(),
                   std::string(caseId) + " must not diagnose valid operators");
}

} // namespace

void registerLexerOperatorModule(TestRegistry& registry) {
    addCase(registry, "TL-LEX-092",
                     "CL-LEX-012",
                     "single-character punctuation and operators",
                     [](TestContext& context) {
                         constexpr std::array samples{
                             OperatorSample{"(", TokenKind::LeftParen},
                             OperatorSample{")", TokenKind::RightParen},
                             OperatorSample{"{", TokenKind::LeftBrace},
                             OperatorSample{"}", TokenKind::RightBrace},
                             OperatorSample{"[", TokenKind::LeftBracket},
                             OperatorSample{"]", TokenKind::RightBracket},
                             OperatorSample{";", TokenKind::Semicolon},
                             OperatorSample{",", TokenKind::Comma},
                             OperatorSample{":", TokenKind::Colon},
                             OperatorSample{"?", TokenKind::Question},
                             OperatorSample{".", TokenKind::Dot},
                             OperatorSample{"~", TokenKind::Tilde},
                             OperatorSample{"+", TokenKind::Plus},
                             OperatorSample{"-", TokenKind::Minus},
                             OperatorSample{"*", TokenKind::Star},
                             OperatorSample{"/", TokenKind::Slash},
                             OperatorSample{"%", TokenKind::Percent},
                             OperatorSample{"=", TokenKind::Equal},
                             OperatorSample{"!", TokenKind::Bang},
                             OperatorSample{"&", TokenKind::Amp},
                             OperatorSample{"|", TokenKind::Pipe},
                             OperatorSample{"^", TokenKind::Caret},
                             OperatorSample{"<", TokenKind::Less},
                             OperatorSample{">", TokenKind::Greater},
                         };
                         for (const auto& sample : samples) {
                             expectOperator(context, sample, "TL-LEX-092");
                         }
                     });

    addCase(registry, "TL-LEX-093",
                     "CL-LEX-012",
                     "two-character operators prefer their compound token",
                     [](TestContext& context) {
                         constexpr std::array samples{
                             OperatorSample{"->", TokenKind::Arrow},
                             OperatorSample{"=>", TokenKind::FatArrow},
                             OperatorSample{"++", TokenKind::PlusPlus},
                             OperatorSample{"--", TokenKind::MinusMinus},
                             OperatorSample{"==", TokenKind::EqualEqual},
                             OperatorSample{"!=", TokenKind::BangEqual},
                             OperatorSample{"<=", TokenKind::LessEqual},
                             OperatorSample{">=", TokenKind::GreaterEqual},
                             OperatorSample{"&&", TokenKind::AmpAmp},
                             OperatorSample{"||", TokenKind::PipePipe},
                             OperatorSample{"<<", TokenKind::LessLess},
                             OperatorSample{">>", TokenKind::GreaterGreater},
                             OperatorSample{"+=", TokenKind::PlusEqual},
                             OperatorSample{"-=", TokenKind::MinusEqual},
                             OperatorSample{"*=", TokenKind::StarEqual},
                             OperatorSample{"/=", TokenKind::SlashEqual},
                             OperatorSample{"%=", TokenKind::PercentEqual},
                             OperatorSample{"&=", TokenKind::AmpEqual},
                             OperatorSample{"|=", TokenKind::PipeEqual},
                             OperatorSample{"^=", TokenKind::CaretEqual},
                             OperatorSample{"??", TokenKind::QuestionQuestion},
                             OperatorSample{"?.", TokenKind::QuestionDot},
                             OperatorSample{"..", TokenKind::DotDot},
                         };
                         for (const auto& sample : samples) {
                             expectOperator(context, sample, "TL-LEX-093");
                         }
                     });

    addCase(registry, "TL-LEX-094",
                     "CL-LEX-012",
                     "dot-family operators use maximal munch",
                     [](TestContext& context) {
                         constexpr std::array samples{
                             OperatorSample{"...", TokenKind::Ellipsis},
                             OperatorSample{"..", TokenKind::DotDot},
                             OperatorSample{"..=", TokenKind::DotDotEqual},
                             OperatorSample{".", TokenKind::Dot},
                         };
                         expectSequence(context,
                                        "... .. ..= .",
                                        samples.data(),
                                        samples.size(),
                                        "TL-LEX-094");
                     });

    addCase(registry, "TL-LEX-095",
                     "CL-LEX-012",
                     "question-family operators use maximal munch",
                     [](TestContext& context) {
                         constexpr std::array samples{
                             OperatorSample{"?" "?=", TokenKind::QuestionQuestionEqual},
                             OperatorSample{"??", TokenKind::QuestionQuestion},
                             OperatorSample{"?.", TokenKind::QuestionDot},
                             OperatorSample{"?", TokenKind::Question},
                         };
                         expectSequence(context,
                                        "?" "?= ?? ?. ?",
                                        samples.data(),
                                        samples.size(),
                                        "TL-LEX-095");
                     });

    addCase(registry, "TL-LEX-096",
                     "CL-LEX-012",
                     "shift-family operators use maximal munch",
                     [](TestContext& context) {
                         constexpr std::array samples{
                             OperatorSample{"<<=", TokenKind::LessLessEqual},
                             OperatorSample{">>=", TokenKind::GreaterGreaterEqual},
                             OperatorSample{"<<", TokenKind::LessLess},
                             OperatorSample{">>", TokenKind::GreaterGreater},
                             OperatorSample{"<=", TokenKind::LessEqual},
                             OperatorSample{">=", TokenKind::GreaterEqual},
                         };
                         expectSequence(context,
                                        "<<= >>= << >> <= >=",
                                        samples.data(),
                                        samples.size(),
                                        "TL-LEX-096");
                     });

    addCase(registry, "TL-LEX-097",
                     "CL-LEX-012",
                     "arrow and increment families remain distinct",
                     [](TestContext& context) {
                         constexpr std::array samples{
                             OperatorSample{"->", TokenKind::Arrow},
                             OperatorSample{"=>", TokenKind::FatArrow},
                             OperatorSample{"++", TokenKind::PlusPlus},
                             OperatorSample{"--", TokenKind::MinusMinus},
                         };
                         expectSequence(context,
                                        "-> => ++ --",
                                        samples.data(),
                                        samples.size(),
                                        "TL-LEX-097");
                     });
}

} // namespace nsc::suite
