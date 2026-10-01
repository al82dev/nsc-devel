# LLVM's official Windows component archives reference zlib and zstd, but the LLVM package does
# not carry those two libraries. Build the pinned, vendored sources as ordinary NativeSharp
# targets so they inherit the compiler's clang-cl toolchain and static MSVC runtime contract.

set(NSC_ZLIB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib")
set(NSC_ZLIB_SOURCES
    ${NSC_ZLIB_SOURCE_DIR}/adler32.c
    ${NSC_ZLIB_SOURCE_DIR}/compress.c
    ${NSC_ZLIB_SOURCE_DIR}/crc32.c
    ${NSC_ZLIB_SOURCE_DIR}/deflate.c
    ${NSC_ZLIB_SOURCE_DIR}/infback.c
    ${NSC_ZLIB_SOURCE_DIR}/inffast.c
    ${NSC_ZLIB_SOURCE_DIR}/inflate.c
    ${NSC_ZLIB_SOURCE_DIR}/inftrees.c
    ${NSC_ZLIB_SOURCE_DIR}/trees.c
    ${NSC_ZLIB_SOURCE_DIR}/uncompr.c
    ${NSC_ZLIB_SOURCE_DIR}/zutil.c)

add_library(nsc_zlib STATIC ${NSC_ZLIB_SOURCES})
add_library(ZLIB::ZLIB ALIAS nsc_zlib)
target_include_directories(nsc_zlib PUBLIC ${NSC_ZLIB_SOURCE_DIR})
target_compile_definitions(nsc_zlib PRIVATE
    NO_FSEEKO
    _CRT_SECURE_NO_DEPRECATE
    _CRT_NONSTDC_NO_DEPRECATE)
set_target_properties(nsc_zlib PROPERTIES OUTPUT_NAME nsc-zlib)

set(NSC_ZSTD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zstd/lib")
file(GLOB NSC_ZSTD_COMMON_SOURCES CONFIGURE_DEPENDS
    "${NSC_ZSTD_SOURCE_DIR}/common/*.c")
file(GLOB NSC_ZSTD_COMPRESS_SOURCES CONFIGURE_DEPENDS
    "${NSC_ZSTD_SOURCE_DIR}/compress/*.c")
file(GLOB NSC_ZSTD_DECOMPRESS_SOURCES CONFIGURE_DEPENDS
    "${NSC_ZSTD_SOURCE_DIR}/decompress/*.c")
list(FILTER NSC_ZSTD_COMPRESS_SOURCES EXCLUDE REGEX "zstdmt_compress\\.c$")

add_library(nsc_zstd STATIC
    ${NSC_ZSTD_COMMON_SOURCES}
    ${NSC_ZSTD_COMPRESS_SOURCES}
    ${NSC_ZSTD_DECOMPRESS_SOURCES})
add_library(zstd::libzstd_static ALIAS nsc_zstd)
target_include_directories(nsc_zstd PUBLIC ${NSC_ZSTD_SOURCE_DIR})
target_compile_definitions(nsc_zstd PRIVATE
    XXH_NAMESPACE=ZSTD_
    ZSTD_DISABLE_ASM
    ZSTD_HEAPMODE=0
    _CRT_SECURE_NO_WARNINGS)
set_target_properties(nsc_zstd PROPERTIES OUTPUT_NAME nsc-zstd)
