#pragma once

namespace nativesharp::frontend
{
enum class GenericTemplateKind
{
    Class,
    Struct,
    Delegate,
    Method,
    // Preserve the serialized numeric values of the four preceding kinds.
    Interface
};
}
