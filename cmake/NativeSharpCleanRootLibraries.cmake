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
