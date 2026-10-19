#pragma once

namespace nativesharp::language
{
struct FloatingPointContract final
{
    bool preservesSignedZero;
    bool preservesNanSemantics;
    bool allowsContraction;
    bool allowsReassociation;
    bool preservesNanPayload;
    bool preservesDenormals;
    bool constevalMatchesRuntime;
};
}
