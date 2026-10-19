#pragma once

namespace nativesharp::language_service
{
enum class LanguageSymbolKind
{
    Class,
    Interface,
    Struct,
    Enum,
    Delegate,
    Function,
    Method,
    Constructor,
    Destructor,
    Field,
    Event,
    Variable,
    Parameter
};
}
