#pragma once

namespace nativesharp::frontend
{
enum class MethodEscapeOriginKind
{
    ExternalStorage,
    CurrentReceiver,
    CurrentFrameLocal,
    CurrentFrameParameter,
    VariableLengthArray
};
}
