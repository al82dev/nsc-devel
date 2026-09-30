#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Interop/LibraryAbiReport.h"
#include "nativesharp/Interop/LibraryApiDiff.h"

#include "../support/TestContext.h"

#include <sstream>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    using namespace nativesharp::interop;
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

    LibraryMetadata metadata;
    metadata.moduleName = "T13Abi";
    metadata.targetTriple = "x86_64-pc-windows-msvc";
    LibraryMetadataType bits;
    bits.kind = LibraryMetadataTypeKind::Struct;
    bits.accessibility = 3;
    bits.identityKey = "T13Abi::Demo.Bits";
    bits.size = 4;
    bits.alignment = 4;
    LibraryMetadataField packed;
    packed.name = "Value";
    packed.typeKey = "builtin:uint";
    packed.size = 4;
    packed.alignment = 4;
    packed.flags = kMetadataFieldBitField;
    packed.bitOffset = 4;
    packed.bitWidth = 3;
    bits.fields.push_back(packed);
    metadata.types.push_back(bits);
    LibraryMetadataType callback;
    callback.kind = LibraryMetadataTypeKind::Delegate;
    callback.accessibility = 3;
    callback.identityKey = "T13Abi::Demo.Callback";
    callback.delegateFlags = 3;
    callback.delegateReturnTypeKey = "builtin:int";
    callback.delegateParameterTypeKeys.push_back("builtin:int");
    metadata.types.push_back(callback);
    LibraryMetadataMember member;
    member.kind = 2;
    member.accessibility = 3;
    member.declaringTypeKey = "type:T13Abi::Demo.Bits";
    member.name = "Read";
    member.signatureKey = "Read()";
    member.declaredTypeKey = "builtin:uint";
    member.mangledName = "__nsc_t13_read";
    metadata.members.push_back(member);
    LibraryMetadataDispatch dispatch;
    dispatch.typeKey = "type:T13Abi::Demo.Owner";
    dispatch.descriptorName = "__nsc_desc_owner";
    dispatch.descriptorAccessorName = "__nsc_get_desc_owner";
    dispatch.vtableName = "__nsc_vtable_owner";
    dispatch.destructionEntryName = "__nsc_destroy_owner";
    metadata.dispatchTables.push_back(dispatch);
    LibraryMetadataExport exported;
    exported.cAbiName = "t13_read";
    exported.declaringTypeKey = member.declaringTypeKey;
    exported.returnTypeKey = "builtin:uint";
    exported.nativeMangledName = member.mangledName;
    metadata.exports.push_back(exported);

    std::ostringstream report;
    LibraryAbiReporter::write(metadata, report);
    const std::string reportText = report.str();
    tests.expect(reportText.find("NativeSharp ABI report v1") != std::string::npos &&
            reportText.find("bit-offset=4 bit-width=3") != std::string::npos,
        "ABI report publishes exact bitfield storage coordinates");
    tests.expect(reportText.find("calling-convention=vectorcall") != std::string::npos &&
            reportText.find("calling-convention=win64-c") != std::string::npos,
        "ABI report distinguishes vectorcall delegates and C exports");
    tests.expect(reportText.find("destruction-entry=__nsc_destroy_owner") != std::string::npos,
        "ABI report exposes the cross-module object destruction entry");

    LibraryMetadata layoutChanged = metadata;
    layoutChanged.types.front().size = 8;
    const LibraryApiDiffReport layoutDiff = LibraryApiDiffer::compare(metadata, layoutChanged);
    tests.expect(!layoutDiff.abiCompatible,
        "ABI diff rejects a changed aggregate layout");

    LibraryMetadata ownershipChanged = metadata;
    ownershipChanged.members.front().declaredOwnership = LibraryMetadataOwnershipKind::Owned;
    const LibraryApiDiffReport ownershipDiff = LibraryApiDiffer::compare(metadata, ownershipChanged);
    tests.expect(ownershipDiff.abiCompatible && !ownershipDiff.sourceCompatible,
        "ABI diff remains compatible for a source-only ownership contract change");
    return tests.result();
}
