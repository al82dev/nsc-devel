#pragma once

namespace nativesharp::frontend
{
enum class GenericTemplateKind
{
    Class,
    Struct,
    Delegate,
    Method,
    // Appended to preserve the serialized numeric values of the four 1.0.1 kinds in NSMETA.
    Interface
};
}
