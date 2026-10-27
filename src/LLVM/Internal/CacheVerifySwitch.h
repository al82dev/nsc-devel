#pragma once

namespace nativesharp::llvm_backend
{
// Validation switch for caches. When true, every cache hit is also rebuilt from its inputs and
// the two results are compared byte for byte; any difference aborts the process. Committed
// builds keep it false; a separate validation build defines NSC_CACHE_VERIFY. With it false the
// guarded checks compile to nothing.
#ifdef NSC_CACHE_VERIFY
inline constexpr bool kCacheVerifyEnabled = true;
#else
inline constexpr bool kCacheVerifyEnabled = false;
#endif
}
