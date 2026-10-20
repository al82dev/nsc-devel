#pragma once

namespace nativesharp::mir
{
class MirBlock;
class MirValue;

struct MirPhiIncoming
{
    MirBlock* block = nullptr;
    MirValue* value = nullptr;
};
}
