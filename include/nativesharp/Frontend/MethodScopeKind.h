#pragma once

namespace nativesharp::frontend
{
enum class MethodScopeKind
{
    Method,
    Constructor,
    Destructor,
    FieldInitializer,
    Block,
    For,
    Foreach,
    Catch,
    Using
};
}
