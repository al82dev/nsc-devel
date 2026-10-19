#pragma once

#include <cstddef>
#include <string>

#include "nativesharp/Language/PerformanceBudget.h"
#include "nativesharp/Language/PerformanceCorpus.h"

namespace nativesharp::language
{
struct PerformanceBudgetProfile final
{
    std::string identity;
    PerformanceCorpus corpus = PerformanceCorpus::GiantFile;
    PerformanceBudget maximum;
    unsigned tolerancePercent = 5;
    std::string referenceHardware;
    std::size_t minimumSamples = 5;
};
}
