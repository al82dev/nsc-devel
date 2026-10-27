#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::driver
{
class CompilationWorkerPool;
}

namespace nativesharp::frontend
{
class ProjectConstructionModel;

struct ProjectMethodBodyBuildOptions final
{
    std::size_t workerCount = 1;
    driver::CompilationWorkerPool* workerPool = nullptr;
    std::function<bool()> cancellationCheck;
    std::function<void(std::string_view, std::chrono::nanoseconds)> phaseTimingSink;
    std::string* executionError = nullptr;
    // EN: Freeze 3.0 (item 130a): previews which constructor a construction or a this/base initializer
    //     selects, so a constructor of a closed generic type is analyzed only when it is chosen.
    // ES: Congelamiento 3.0 (elemento 130a): anticipa que constructor elige una construccion o un
    //     inicializador this/base, para analizar un constructor de un tipo generico cerrado solo cuando
    //     se elige.
    const ProjectConstructionModel* constructorDemandResolver = nullptr;
};
}
