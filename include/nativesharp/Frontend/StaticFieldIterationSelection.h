#pragma once

#include "nativesharp/Frontend/StaticFieldAccessSelection.h"

#include <string>
#include <vector>

namespace nativesharp::frontend
{
// A provisional ordered selection. Body analysis compares the entire set to
// the semantic member model, even when no generated access reads a value.
struct StaticFieldIterationSelection final
{
    std::string queriedTypeIdentity;
    std::vector<StaticFieldAccessSelection> fields;
};
}
