#pragma once

namespace nativesharp::ast
{
// NS304-T06-F05: `ref T` / `ref mut T` as a type-level qualifier (an item by reference), distinct
// from ownership (`borrowed`) and from a declaration's leading `ref` return marker.
enum class ReferenceQualifierSyntaxKind
{
    None,
    Ref,
    RefMut
};
}
