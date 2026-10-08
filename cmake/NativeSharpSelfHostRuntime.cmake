function(nsc_resolve_selfhost_runtime output_library output_abi nslink_executable)
    get_filename_component(nslink_bin "${nslink_executable}" DIRECTORY)
    get_filename_component(nslink_prefix "${nslink_bin}" DIRECTORY)

    if(DEFINED RUNTIME_LIBRARY AND NOT RUNTIME_LIBRARY STREQUAL "")
        set(runtime_library "${RUNTIME_LIBRARY}")
    elseif(DEFINED ENV{NSC_WINDOWS_RUNTIME_LIBRARY} AND
           NOT "$ENV{NSC_WINDOWS_RUNTIME_LIBRARY}" STREQUAL "")
        set(runtime_library "$ENV{NSC_WINDOWS_RUNTIME_LIBRARY}")
    elseif(DEFINED ENV{NS_LIB_ROOT} AND NOT "$ENV{NS_LIB_ROOT}" STREQUAL "")
        set(runtime_library
            "$ENV{NS_LIB_ROOT}/nativesharp/runtime/NativeSharp.Runtime.Win32.lib")
    else()
        set(runtime_library
            "${nslink_prefix}/lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib")
    endif()

    if(DEFINED RUNTIME_ABI AND NOT RUNTIME_ABI STREQUAL "")
        set(runtime_abi "${RUNTIME_ABI}")
    elseif(DEFINED ENV{NS_LIB_ROOT} AND NOT "$ENV{NS_LIB_ROOT}" STREQUAL "")
        get_filename_component(runtime_prefix "$ENV{NS_LIB_ROOT}" DIRECTORY)
        set(runtime_abi
            "${runtime_prefix}/share/nativesharp/runtime/NativeSharpRuntime.abi")
    else()
        set(runtime_abi
            "${nslink_prefix}/share/nativesharp/runtime/NativeSharpRuntime.abi")
    endif()

    set(${output_library} "${runtime_library}" PARENT_SCOPE)
    set(${output_abi} "${runtime_abi}" PARENT_SCOPE)
endfunction()
