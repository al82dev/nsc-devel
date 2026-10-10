#include "DominanceTestSupport.h"

// Keep the test-only access declaration and the product implementation in one
// ABI view. This prevents MSVC decorated-name mismatches for the private builder
// helpers exercised by the directed component cases.
#include "../../../../src/Dominance/DominanceBuilder.cpp"
