#pragma once

namespace nativesharp::llvm_backend
{
enum class ProjectOopCodePartitionPolicy
{
    Balanced,
    StableIncremental,
    CacheAwareBalanced
};
}
