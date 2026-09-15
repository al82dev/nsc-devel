#include "nativesharp/Lexer/TokenKind.h"

namespace nativesharp::lexer
{
std::string_view tokenKindName(TokenKind kind) noexcept
{
    switch (kind)
    {
#define NS_TOKEN_CASE(name) case TokenKind::name: return #name
        NS_TOKEN_CASE(EndOfFile);
        NS_TOKEN_CASE(Invalid);
        NS_TOKEN_CASE(Identifier);
        NS_TOKEN_CASE(IntegerLiteral);
        NS_TOKEN_CASE(FloatingLiteral);
        NS_TOKEN_CASE(CharacterLiteral);
        NS_TOKEN_CASE(StringLiteral);
        NS_TOKEN_CASE(AssemblyText);
        NS_TOKEN_CASE(AssemblyNewLine);
        NS_TOKEN_CASE(KwVoid);
        NS_TOKEN_CASE(KwBool);
        NS_TOKEN_CASE(KwSByte);
        NS_TOKEN_CASE(KwByte);
        NS_TOKEN_CASE(KwChar);
        NS_TOKEN_CASE(KwShort);
        NS_TOKEN_CASE(KwUShort);
        NS_TOKEN_CASE(KwInt);
        NS_TOKEN_CASE(KwUInt);
        NS_TOKEN_CASE(KwLong);
        NS_TOKEN_CASE(KwULong);
        NS_TOKEN_CASE(KwNInt);
        NS_TOKEN_CASE(KwNUInt);
        NS_TOKEN_CASE(KwFloat);
        NS_TOKEN_CASE(KwDouble);
        NS_TOKEN_CASE(KwString);
        NS_TOKEN_CASE(KwConst);
        NS_TOKEN_CASE(KwStatic);
        NS_TOKEN_CASE(KwReadonly);
        NS_TOKEN_CASE(KwVolatile);
        NS_TOKEN_CASE(KwInline);
        NS_TOKEN_CASE(KwForceInline);
        NS_TOKEN_CASE(KwNoInline);
        NS_TOKEN_CASE(KwNoHeap);
        NS_TOKEN_CASE(KwPublic);
        NS_TOKEN_CASE(KwInternal);
        NS_TOKEN_CASE(KwProtected);
        NS_TOKEN_CASE(KwPrivate);
        NS_TOKEN_CASE(KwIf);
        NS_TOKEN_CASE(KwElse);
        NS_TOKEN_CASE(KwWhile);
        NS_TOKEN_CASE(KwDo);
        NS_TOKEN_CASE(KwFor);
        NS_TOKEN_CASE(KwSwitch);
        NS_TOKEN_CASE(KwCase);
        NS_TOKEN_CASE(KwBreak);
        NS_TOKEN_CASE(KwContinue);
        NS_TOKEN_CASE(KwReturn);
        NS_TOKEN_CASE(KwTry);
        NS_TOKEN_CASE(KwCatch);
        NS_TOKEN_CASE(KwFinally);
        NS_TOKEN_CASE(KwThrow);
        NS_TOKEN_CASE(KwStruct);
        NS_TOKEN_CASE(KwClass);
        NS_TOKEN_CASE(KwInterface);
        NS_TOKEN_CASE(KwEnum);
        NS_TOKEN_CASE(KwNamespace);
        NS_TOKEN_CASE(KwUsing);
        NS_TOKEN_CASE(KwNew);
        NS_TOKEN_CASE(KwDelete);
        NS_TOKEN_CASE(KwDestroy);
        NS_TOKEN_CASE(KwStackalloc);
        NS_TOKEN_CASE(KwSizeof);
        NS_TOKEN_CASE(KwAlignof);
        NS_TOKEN_CASE(KwOffsetof);
        NS_TOKEN_CASE(KwStaticAssert);
        NS_TOKEN_CASE(KwDefault);
        NS_TOKEN_CASE(KwExtern);
        NS_TOKEN_CASE(KwExport);
        NS_TOKEN_CASE(KwC);
        NS_TOKEN_CASE(KwTrue);
        NS_TOKEN_CASE(KwFalse);
        NS_TOKEN_CASE(KwNull);
        NS_TOKEN_CASE(KwThis);
        NS_TOKEN_CASE(KwBase);
        NS_TOKEN_CASE(KwVirtual);
        NS_TOKEN_CASE(KwOverride);
        NS_TOKEN_CASE(KwAbstract);
        NS_TOKEN_CASE(KwSealed);
        NS_TOKEN_CASE(KwGet);
        NS_TOKEN_CASE(KwSet);
        NS_TOKEN_CASE(KwOperator);
        NS_TOKEN_CASE(KwImplicit);
        NS_TOKEN_CASE(KwExplicit);
        NS_TOKEN_CASE(KwIs);
        NS_TOKEN_CASE(KwAs);
        NS_TOKEN_CASE(KwDelegate);
        NS_TOKEN_CASE(KwEvent);
        NS_TOKEN_CASE(KwRaii);
        NS_TOKEN_CASE(KwMove);
        NS_TOKEN_CASE(KwOwned);
        NS_TOKEN_CASE(KwBorrowed);
        NS_TOKEN_CASE(KwVar);
        NS_TOKEN_CASE(KwForeach);
        NS_TOKEN_CASE(KwIn);
        NS_TOKEN_CASE(LeftParen);
        NS_TOKEN_CASE(RightParen);
        NS_TOKEN_CASE(LeftBrace);
        NS_TOKEN_CASE(RightBrace);
        NS_TOKEN_CASE(LeftBracket);
        NS_TOKEN_CASE(RightBracket);
        NS_TOKEN_CASE(Semicolon);
        NS_TOKEN_CASE(Comma);
        NS_TOKEN_CASE(Colon);
        NS_TOKEN_CASE(Question);
        NS_TOKEN_CASE(Dot);
        NS_TOKEN_CASE(Arrow);
        NS_TOKEN_CASE(FatArrow);
        NS_TOKEN_CASE(Plus);
        NS_TOKEN_CASE(Minus);
        NS_TOKEN_CASE(Star);
        NS_TOKEN_CASE(Slash);
        NS_TOKEN_CASE(Percent);
        NS_TOKEN_CASE(PlusPlus);
        NS_TOKEN_CASE(MinusMinus);
        NS_TOKEN_CASE(EqualEqual);
        NS_TOKEN_CASE(BangEqual);
        NS_TOKEN_CASE(Less);
        NS_TOKEN_CASE(LessEqual);
        NS_TOKEN_CASE(Greater);
        NS_TOKEN_CASE(GreaterEqual);
        NS_TOKEN_CASE(Bang);
        NS_TOKEN_CASE(AmpAmp);
        NS_TOKEN_CASE(PipePipe);
        NS_TOKEN_CASE(Amp);
        NS_TOKEN_CASE(Pipe);
        NS_TOKEN_CASE(Caret);
        NS_TOKEN_CASE(Tilde);
        NS_TOKEN_CASE(LessLess);
        NS_TOKEN_CASE(GreaterGreater);
        NS_TOKEN_CASE(Equal);
        NS_TOKEN_CASE(PlusEqual);
        NS_TOKEN_CASE(MinusEqual);
        NS_TOKEN_CASE(StarEqual);
        NS_TOKEN_CASE(SlashEqual);
        NS_TOKEN_CASE(PercentEqual);
        NS_TOKEN_CASE(AmpEqual);
        NS_TOKEN_CASE(PipeEqual);
        NS_TOKEN_CASE(CaretEqual);
        NS_TOKEN_CASE(LessLessEqual);
        NS_TOKEN_CASE(GreaterGreaterEqual);
#undef NS_TOKEN_CASE
    }

    return "Invalid";
}

bool isBuiltinTypeKeyword(TokenKind kind) noexcept
{
    return kind >= TokenKind::KwVoid && kind <= TokenKind::KwString;
}

bool isOwnershipQualifierKeyword(TokenKind kind) noexcept
{
    return kind == TokenKind::KwOwned || kind == TokenKind::KwBorrowed;
}

bool isAssignmentOperator(TokenKind kind) noexcept
{
    return kind == TokenKind::Equal ||
        (kind >= TokenKind::PlusEqual && kind <= TokenKind::GreaterGreaterEqual);
}

// IDs 79/81. La correspondencia `+=` -> `+` vive AQUI, junto a isAssignmentOperator, y no en el
// HIR: es una propiedad del token, no del lowering, y tenerla en un solo sitio es lo que impide
// que el dia que se anada un operador compuesto nuevo quede la mitad de la tabla en otra capa.
// Devuelve Invalid para `=` (que no es compuesto) y para cualquier otro token.
TokenKind compoundAssignmentBaseOperator(TokenKind kind) noexcept
{
    switch (kind)
    {
        case TokenKind::PlusEqual: return TokenKind::Plus;
        case TokenKind::MinusEqual: return TokenKind::Minus;
        case TokenKind::StarEqual: return TokenKind::Star;
        case TokenKind::SlashEqual: return TokenKind::Slash;
        case TokenKind::PercentEqual: return TokenKind::Percent;
        case TokenKind::AmpEqual: return TokenKind::Amp;
        case TokenKind::PipeEqual: return TokenKind::Pipe;
        case TokenKind::CaretEqual: return TokenKind::Caret;
        case TokenKind::LessLessEqual: return TokenKind::LessLess;
        case TokenKind::GreaterGreaterEqual: return TokenKind::GreaterGreater;
        default: return TokenKind::Invalid;
    }
}
}
