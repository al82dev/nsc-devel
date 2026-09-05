if(NOT DEFINED NSC_OUTPUT OR NSC_OUTPUT STREQUAL "")
    message(FATAL_ERROR "NSC_OUTPUT is required")
endif()
if(NOT DEFINED NSC_TEMPLATE OR NSC_TEMPLATE STREQUAL "")
    message(FATAL_ERROR "NSC_TEMPLATE is required")
endif()
if(NOT DEFINED NSC_VERSION_BASE OR NSC_VERSION_BASE STREQUAL "")
    message(FATAL_ERROR "NSC_VERSION_BASE is required")
endif()

# UTC keeps the value independent of the machine's time zone. The random suffix guarantees a
# different identity when two builds begin in the same second.
string(TIMESTAMP NSC_BUILD_TIMESTAMP "%Y%m%dT%H%M%SZ" UTC)
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef NSC_BUILD_ID)
set(NSC_COMPILER_VERSION
    "${NSC_VERSION_BASE}+build.${NSC_BUILD_TIMESTAMP}.${NSC_BUILD_ID}")

get_filename_component(NSC_OUTPUT_DIRECTORY "${NSC_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${NSC_OUTPUT_DIRECTORY}")
set(NSC_TEMP_OUTPUT "${NSC_OUTPUT}.tmp")
configure_file("${NSC_TEMPLATE}" "${NSC_TEMP_OUTPUT}" @ONLY)
file(RENAME "${NSC_TEMP_OUTPUT}" "${NSC_OUTPUT}")

message(STATUS "NativeSharp build identity: ${NSC_COMPILER_VERSION}")
