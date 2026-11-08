#pragma once

namespace nativesharp::compilation
{
// The access policy of a NativeSharp compilation unit is distinct from the
// options used to parse any one C/C++ header. An absent policy remains unset.
enum class CppConsumerVolatileMode
{
    Microsoft,
    Iso
};
}
