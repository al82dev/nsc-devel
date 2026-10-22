#include "nativesharp/Frontend/ProjectCompilationLifetime.h"

#include <algorithm>

namespace nativesharp::frontend
{
void ProjectCompilationLifetime::setWorkerCount(std::size_t workerCount) noexcept
{
    const std::size_t normalized = std::max<std::size_t>(1, workerCount);
    if (workerCount_ == normalized) return;
    workerCount_ = normalized;
    workerPool_.reset();
}

driver::CompilationWorkerPool& ProjectCompilationLifetime::workerPool()
{
    if (workerPool_ == nullptr)
    {
        workerPool_ = std::make_unique<driver::CompilationWorkerPool>(workerCount_);
    }
    return *workerPool_;
}

driver::CompilationGraphScheduler&
ProjectCompilationLifetime::scheduler() noexcept
{
    return scheduler_;
}

ProjectCompilationIndexes& ProjectCompilationLifetime::indexes() noexcept
{
    return indexes_;
}

const ProjectCompilationIndexes&
ProjectCompilationLifetime::indexes() const noexcept
{
    return indexes_;
}

compilation::LibraryMetadataRepository&
ProjectCompilationLifetime::metadataRepository() noexcept
{
    return metadataRepository_;
}

const compilation::LibraryMetadataRepository&
ProjectCompilationLifetime::metadataRepository() const noexcept
{
    return metadataRepository_;
}

TemplateParseCache& ProjectCompilationLifetime::templateParseCache() noexcept
{
    return templateParseCache_;
}

TemplateParseCache& ProjectCompilationLifetime::precompiledSyntaxCache() noexcept
{
    return precompiledSyntaxCache_;
}
}
