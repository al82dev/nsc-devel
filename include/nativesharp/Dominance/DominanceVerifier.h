#pragma once

#include <string>

namespace nativesharp::dominance
{
class DominanceFunction;
class DominanceModule;

class DominanceVerifier final
{
public:
    bool verify(const DominanceModule* module);
    const std::string& error() const noexcept;

private:
    bool verifyFunction(const DominanceFunction* function);
    bool fail(std::string message);

    std::string error_;
};
}
