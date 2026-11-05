#include "nativesharp/Interop/CppSessionOptions.h"
#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppInteropSession.h"
#include <type_traits>

static_assert(!std::is_copy_constructible_v<nativesharp::interop::CppSessionOpenResult>);
static_assert(std::is_copy_constructible_v<nativesharp::interop::CppSessionOpenFailure>);

#include "nativesharp/Interop/CppInteropSessionFactory.h"

using namespace nativesharp::interop;
static_assert(std::is_abstract_v<CppInteropSession>);
static_assert(std::is_abstract_v<CppInteropSessionFactory>);
static_assert(std::is_copy_constructible_v<CppSessionOptions>);
static_assert(std::is_copy_constructible_v<CppSessionDiagnostic>);
static_assert(std::is_copy_constructible_v<CppCancellationToken>);
static_assert(!std::is_copy_constructible_v<CppSessionOpenResult>);
