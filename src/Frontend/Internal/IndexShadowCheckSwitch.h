#pragma once

namespace nativesharp::frontend::internal
{
// Validation switch for lookup indexes. When true, every indexed lookup is also answered by the
// original linear search and any difference aborts the process. Committed builds keep it false;
// a separate validation build defines NSC_INDEX_SHADOW_CHECK. With it false the guarded checks
// compile to nothing.
#ifdef NSC_INDEX_SHADOW_CHECK
inline constexpr bool kIndexShadowCheckEnabled = true;
#else
inline constexpr bool kIndexShadowCheckEnabled = false;
#endif
}
