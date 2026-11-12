#pragma once

namespace nativesharp::frontend
{
// Describes only the result, not permission to discard it or proof of reference lifetime.
// Expression evaluation and its cleanups remain mandatory in every case.
enum class ProjectOopYieldResultKind
{
    Unknown,
    TrivialScalar,
    BorrowedReference
};
}
