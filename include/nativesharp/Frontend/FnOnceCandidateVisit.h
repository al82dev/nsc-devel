#pragma once
namespace nativesharp::frontend
{
// Describes an analysis visit, not reachability or runtime execution.
enum class FnOnceCandidateVisit { Unknown, DuringDiscardedStatementProbe };
}
