#pragma once

namespace nativesharp::interop
{
enum class LibraryApiChangeImpact
{
    Additive,
    SourceBreaking,
    AbiBreaking
};
}
