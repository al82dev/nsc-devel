#include "nativesharp/Language/AbiSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;
    tests.expect(validateRepresentation({Representation::C, 8, 16, 3}, error) &&
            validateRepresentation({Representation::Transparent, {}, {}, 1}, error) &&
            !validateRepresentation({Representation::Transparent, 1, {}, 1}, error),
        "representation attributes accept only coherent Windows x64 contracts");
    const auto mask = bitFieldMask({32, 4, 3, false, true});
    tests.expect(mask == 0x70 && !bitFieldMask({8, 7, 2, false, false}).has_value(),
        "declared-layout bitfields have bounded deterministic masks");

    const PointerContract weak{true, false, false, 1, 0};
    const PointerContract strong{false, true, true, 16, 0};
    tests.expect(pointerCastRequiresUnsafe(weak, strong) && !pointerCastRequiresUnsafe(strong, weak),
        "pointer casts that invent nullability, mutability, provenance, or alignment require unsafe");
    tests.expect(callingConventionSupportedOnWindowsX64(AbiCallingConvention::VectorCall, false) &&
            !callingConventionSupportedOnWindowsX64(AbiCallingConvention::VectorCall, true),
        "calling conventions are type-level and variadic support is checked on Windows x64");
    tests.expect(unsafeOperationRequiresContext(UnsafeOperation::RawDereference) &&
            unsafeOperationRequiresContext(UnsafeOperation::InlineAssembly) &&
            unsafeBlockTooBroad(12, false) && !unsafeBlockTooBroad(12, true),
        "unsafe operations are auditable and broad unexplained blocks are linted");

    tests.expect(validateFfiParameter({false, FfiOwnership::Borrowed, 1, false, false}, 2, error) &&
            !validateFfiParameter({false, FfiOwnership::Borrowed, 4, false, false}, 2, error),
        "FFI buffer length links are range-checked against the signature");
    tests.expect(!validateFfiParameter({false, FfiOwnership::Borrowed, {}, true, true}, 1, error) &&
            !ffiMayUnwindAcrossBoundary(),
        "escaping callbacks need owned lifetime and unwind never crosses FFI implicitly");
    tests.expect(validateSimd({32, 8, false}, error) && !validateSimd({32, 32, false}, error) &&
            simdScalarFallbackPreservesSemantics(),
        "portable SIMD bounds vector width and permits semantics-preserving scalar fallback");
    return tests.result();
}
