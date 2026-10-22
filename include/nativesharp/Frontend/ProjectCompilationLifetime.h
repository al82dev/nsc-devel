#pragma once

#include "nativesharp/Compilation/LibraryMetadataRepository.h"
#include "nativesharp/Driver/CompilationGraphScheduler.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectCompilationIndexes.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include <cstddef>
#include <memory>

namespace nativesharp::frontend
{
// Owns resources whose useful lifetime is the complete compilation, rather than one phase.
// Phase code borrows these objects; it never transfers ownership or creates competing roots.
class ProjectCompilationLifetime final
{
public:
    void reset() noexcept;
    void setWorkerCount(std::size_t workerCount) noexcept;
    driver::CompilationWorkerPool& workerPool();
    driver::CompilationGraphScheduler& scheduler() noexcept;
    ProjectCompilationIndexes& indexes() noexcept;
    const ProjectCompilationIndexes& indexes() const noexcept;
    compilation::LibraryMetadataRepository& metadataRepository() noexcept;
    const compilation::LibraryMetadataRepository& metadataRepository() const noexcept;
    TemplateParseCache& templateParseCache() noexcept;
    TemplateParseCache& precompiledSyntaxCache() noexcept;

private:
    std::size_t workerCount_ = 1;
    std::unique_ptr<driver::CompilationWorkerPool> workerPool_;
    driver::CompilationGraphScheduler scheduler_;
    ProjectCompilationIndexes indexes_;
    compilation::LibraryMetadataRepository metadataRepository_;
    TemplateParseCache templateParseCache_;
    TemplateParseCache precompiledSyntaxCache_;
};
}
