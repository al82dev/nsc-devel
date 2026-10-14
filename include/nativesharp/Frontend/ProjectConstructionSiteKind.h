#pragma once

namespace nativesharp::frontend
{
// The syntax form is frozen during method-body analysis so construction binding does not need to
// rediscover it by walking the AST a second time.
enum class ProjectConstructionSiteKind
{
    Object,
    Placement,
    // NS304-T13-F02: `new (allocator) T(args)` where `allocator : IAllocator`. The HIR desugars to
    // `allocator.Allocate(size)` + placement construction on the returned pointer.
    Allocator
};
}
