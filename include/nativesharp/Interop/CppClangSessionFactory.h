#pragma once

#include "nativesharp/Interop/CppInteropSessionFactory.h"

namespace nativesharp::interop
{
// Implemented by the optional nsc_cpp_interop component.
class CppClangSessionFactory final : public CppInteropSessionFactory
{
public:
    CppSessionOpenResult open(CppSessionOptions options, CppCancellationToken cancellation) override;
};
}
