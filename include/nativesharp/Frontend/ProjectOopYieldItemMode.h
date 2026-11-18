#pragma once

namespace nativesharp::frontend
{
// The declared iterator item mode; Unknown means its source was not established.
enum class ProjectOopYieldItemMode
{
    Unknown,
    Value,
    Reference,
    MutableReference
};
}
