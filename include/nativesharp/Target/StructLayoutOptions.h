#pragma once

#include <cstdint>

namespace nativesharp::target
{
// C2.2 "union and explicit layout". Everything a C aggregate can say about WHERE its fields sit,
// expressed once, at the layer that owns the target's layout rules:
//
//   * isUnion       -- every field starts at the same place (offset 0 unless it names its own),
//                      and the aggregate is as big as its largest member. `union` in C.
//   * pack          -- an upper bound on every field's alignment, so a field is placed tighter
//                      than its type would naturally ask for. `#pragma pack(N)` in C. 0 = natural.
//   * minimumAlignment -- a lower bound on the AGGREGATE's alignment. `__declspec(align(N))` /
//                      `alignas(N)` in C. 0 = whatever the fields imply.
//
// A field may also name its own byte offset (StructFieldRequest::explicitOffset), which is how
// Win32's `INPUT` and every other hand-laid-out record is expressed. Overlap is not an error
// there: overlapping IS the feature. `minimumAlignment` is a field-local `align(N)`: it may raise
// the post-pack alignment but never weakens the element type's natural requirement.
struct StructLayoutOptions
{
    bool isUnion = false;
    std::uint64_t pack = 0;
    std::uint64_t minimumAlignment = 0;
};
}
