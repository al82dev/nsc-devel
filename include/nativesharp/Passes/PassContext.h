#pragma once

namespace nativesharp::mir
{
class MirArena;
class MirModule;
}

namespace nativesharp::passes
{
class AnalysisManager;

class PassContext final
{
public:
    PassContext(
        mir::MirModule* module,
        mir::MirArena& arena,
        AnalysisManager& analyses) noexcept;

    mir::MirModule* module() const noexcept;
    mir::MirArena& arena() const noexcept;
    AnalysisManager& analyses() const noexcept;

private:
    mir::MirModule* module_;
    mir::MirArena* arena_;
    AnalysisManager* analyses_;
};
}
