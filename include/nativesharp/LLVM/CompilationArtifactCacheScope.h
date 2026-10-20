#pragma once

namespace nativesharp::llvm_backend
{
class CompilationArtifactCache;

class CompilationArtifactCacheScope final
{
public:
    explicit CompilationArtifactCacheScope(const CompilationArtifactCache& cache) noexcept;
    ~CompilationArtifactCacheScope();

    CompilationArtifactCacheScope(const CompilationArtifactCacheScope&) = delete;
    CompilationArtifactCacheScope& operator=(const CompilationArtifactCacheScope&) = delete;

private:
    const CompilationArtifactCache* previous_ = nullptr;
};
}
