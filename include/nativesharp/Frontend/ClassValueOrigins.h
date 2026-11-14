#pragma once

namespace nativesharp::frontend
{
// Unknown remains true unless receiver publication and other effects are proven safe.
struct ClassReceiverEffects final
{
    bool returned = false;
    bool published = false;
    bool unknown = true;
};
}
