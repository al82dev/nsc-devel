#pragma once

#include <string>

namespace nativesharp::sema
{
class Type;
}

namespace nativesharp::hir
{
class HirNode;

class HirVerifier final
{
public:
    bool verify(const HirNode* root);
    const std::string& error() const noexcept;

private:
    bool verifyNode(const HirNode* node, sema::Type* functionReturnType, int loopDepth);
    bool verifyPointerTo(const HirNode* address, const sema::Type* pointee);
    bool fail(const HirNode* node, std::string message);

    std::string error_;
};
}
