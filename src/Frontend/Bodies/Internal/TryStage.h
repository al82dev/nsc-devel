#pragma once

namespace nativesharp::frontend
{
enum class ProjectMethodBodyModel::TryStage
{
    Start,
    AfterTryBody,
    NextCatch,
    AfterCatch,
    AfterFinally
};
}
