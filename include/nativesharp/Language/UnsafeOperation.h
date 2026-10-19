#pragma once

namespace nativesharp::language
{
enum class UnsafeOperation { RawDereference, UnionInactiveRead, IntegerPointerCast, InlineAssembly, FfiCall };
}
