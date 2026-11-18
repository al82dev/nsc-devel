# EN: Copies one compiler-tree library (lib/<library>) into a clean root. The standard libraries live in
#     NativeSharp Libraries and lib/ keeps only projects that forward to them with a path relative to the
#     checkout ("../../../nativesharp-libraries"); from the clean root that relative path names nothing, so the
#     copied projects are pointed at the absolute libraries checkout. Other files are copied unchanged.
# ES: Copia una biblioteca del arbol del compilador (lib/<library>) a una raiz limpia. Las bibliotecas estandar
#     viven en NativeSharp Libraries y lib/ conserva solo proyectos que las redirigen con una ruta relativa al
#     checkout ("../../../nativesharp-libraries"); desde la raiz limpia esa ruta no nombra nada, asi que los
#     proyectos copiados apuntan al checkout absoluto de las bibliotecas. Los demas archivos se copian igual.
function(nsc_copy_clean_root_library source_root libraries_root library destination)
    file(COPY "${source_root}/lib/${library}" DESTINATION "${destination}")
    file(GLOB projects "${destination}/${library}/*.nsproj")
    foreach(project IN LISTS projects)
        file(READ "${project}" text)
        string(FIND "${text}" "../../../nativesharp-libraries/" forwarded)
        if(NOT forwarded EQUAL -1)
            if(NOT IS_DIRECTORY "${libraries_root}/stdlib")
                message(FATAL_ERROR "NativeSharp Libraries checkout not found: ${libraries_root}")
            endif()
            string(REPLACE "../../../nativesharp-libraries/" "${libraries_root}/" text "${text}")
            file(WRITE "${project}" "${text}")
        endif()
    endforeach()
endfunction()

# EN: The libraries checkout given by the test, or the sibling of the compiler checkout (the CMake default).
# ES: El checkout de bibliotecas que da la prueba o el hermano del checkout del compilador (el valor por defecto
#     de CMake).
function(nsc_clean_root_libraries_source source_root output_variable)
    if(DEFINED LIBRARIES_SOURCE_ROOT AND NOT LIBRARIES_SOURCE_ROOT STREQUAL "")
        set(root "${LIBRARIES_SOURCE_ROOT}")
    else()
        set(root "${source_root}/../nativesharp-libraries")
    endif()
    get_filename_component(root "${root}" ABSOLUTE)
    set(${output_variable} "${root}" PARENT_SCOPE)
endfunction()

# EN: Lays out one clean root with the same relation between checkouts as the source tree: the compiler tree in
#     <clean_root>/compiler (selfhost/nsc and lib/<library> copied unchanged, forwarding projects included) and the
#     standard libraries in <clean_root>/nativesharp-libraries/stdlib/<library>, where the forwarding path
#     "../../../nativesharp-libraries" of lib/<library>/*.nsproj lands. Nothing is rewritten, so the compiler
#     tree and the libraries tree are byte-identical to their sources.
# ES: Arma una raiz limpia con la misma relacion entre checkouts que el arbol fuente: el arbol del compilador en
#     <clean_root>/compiler (selfhost/nsc y lib/<library> copiados sin cambios, incluidos los proyectos que
#     redirigen) y las bibliotecas estandar en <clean_root>/nativesharp-libraries/stdlib/<library>, donde cae la
#     ruta "../../../nativesharp-libraries" de lib/<library>/*.nsproj. No se reescribe nada, asi que ambos arboles
#     son identicos byte a byte a sus fuentes.
function(nsc_make_clean_root source_root libraries_root clean_root)
    set(compiler "${clean_root}/compiler")
    file(MAKE_DIRECTORY "${compiler}/selfhost" "${compiler}/lib" "${clean_root}/nativesharp-libraries/stdlib")
    file(COPY "${source_root}/selfhost/nsc" DESTINATION "${compiler}/selfhost")
    foreach(library IN LISTS ARGN)
        file(COPY "${source_root}/lib/${library}" DESTINATION "${compiler}/lib")
        # EN: Only a library whose project forwards to NativeSharp Libraries needs its sources there.
        # ES: Solo una biblioteca cuyo proyecto redirige a NativeSharp Libraries necesita alli sus fuentes.
        file(GLOB projects "${source_root}/lib/${library}/*.nsproj")
        set(forwarded FALSE)
        foreach(project IN LISTS projects)
            file(READ "${project}" text)
            string(FIND "${text}" "../../../nativesharp-libraries/" position)
            if(NOT position EQUAL -1)
                set(forwarded TRUE)
            endif()
        endforeach()
        if(forwarded)
            if(NOT IS_DIRECTORY "${libraries_root}/stdlib/${library}")
                message(FATAL_ERROR "NativeSharp Libraries checkout lacks stdlib/${library}: ${libraries_root}")
            endif()
            file(COPY "${libraries_root}/stdlib/${library}" DESTINATION "${clean_root}/nativesharp-libraries/stdlib")
        endif()
    endforeach()
endfunction()
