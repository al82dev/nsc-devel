if(NOT DEFINED NSC_OUTPUT OR NSC_OUTPUT STREQUAL "")
    message(FATAL_ERROR "NSC_OUTPUT is required")
endif()
if(NOT DEFINED NSC_TEMPLATE OR NSC_TEMPLATE STREQUAL "")
    message(FATAL_ERROR "NSC_TEMPLATE is required")
endif()
if(NOT DEFINED NSC_VERSION_BASE OR NSC_VERSION_BASE STREQUAL "")
    message(FATAL_ERROR "NSC_VERSION_BASE is required")
endif()

# Normal builds receive a unique identity. The independent-build gate may provide a canonical
# hexadecimal override so every other byte of nsc.exe can be compared without exempting the whole
# executable merely because this deliberately volatile field changed.
if(DEFINED NSC_BUILD_ID_OVERRIDE AND NOT NSC_BUILD_ID_OVERRIDE STREQUAL "")
    string(LENGTH "${NSC_BUILD_ID_OVERRIDE}" NSC_BUILD_ID_OVERRIDE_LENGTH)
    if(NOT NSC_BUILD_ID_OVERRIDE_LENGTH EQUAL 16 OR
       NOT NSC_BUILD_ID_OVERRIDE MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "NSC_BUILD_ID_OVERRIDE must contain exactly 16 lowercase hexadecimal digits")
    endif()
    set(NSC_BUILD_TIMESTAMP "19700101T000000Z")
    set(NSC_BUILD_ID "${NSC_BUILD_ID_OVERRIDE}")
else()
    # UTC keeps the value independent of the machine's time zone. The random suffix guarantees a
    # different identity when two builds begin in the same second.
    string(TIMESTAMP NSC_BUILD_TIMESTAMP "%Y%m%dT%H%M%SZ" UTC)
    string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef NSC_BUILD_ID)
endif()
set(NSC_COMPILER_VERSION
    "${NSC_VERSION_BASE}+build.${NSC_BUILD_TIMESTAMP}.${NSC_BUILD_ID}")

get_filename_component(NSC_OUTPUT_DIRECTORY "${NSC_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${NSC_OUTPUT_DIRECTORY}")
set(NSC_TEMP_OUTPUT "${NSC_OUTPUT}.tmp")
configure_file("${NSC_TEMPLATE}" "${NSC_TEMP_OUTPUT}" @ONLY)
file(RENAME "${NSC_TEMP_OUTPUT}" "${NSC_OUTPUT}")

message(STATUS "NativeSharp build identity: ${NSC_COMPILER_VERSION}")
