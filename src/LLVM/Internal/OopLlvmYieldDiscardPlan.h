#pragma once

namespace nativesharp::frontend
{
struct ProjectOopMirNode;
}

namespace nativesharp::llvm_backend
{
struct OopLlvmYieldDiscardPlan final
{
    const frontend::ProjectOopMirNode* condition = nullptr;
    const frontend::ProjectOopMirNode* whenTrue = nullptr;
    const frontend::ProjectOopMirNode* whenFalse = nullptr;
    bool trueOwned = false;
    bool falseOwned = false;
};
}
