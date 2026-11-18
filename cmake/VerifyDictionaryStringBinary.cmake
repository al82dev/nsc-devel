# Build an isolated consumer of explicit, prebuilt Core and Collections artifacts.
foreach(required IN ITEMS SOURCE_FILE CORE_LIBRARY CORE_METADATA COLLECTIONS_LIBRARY
                          COLLECTIONS_METADATA NSC_EXECUTABLE RUNTIME_LIBRARY
                          NSLINK_EXECUTABLE OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing ${required}")
    endif()
endforeach()
foreach(input IN ITEMS SOURCE_FILE CORE_LIBRARY CORE_METADATA COLLECTIONS_LIBRARY COLLECTIONS_METADATA)
    if(NOT EXISTS "${${input}}")
        message(FATAL_ERROR "Missing input ${input}: ${${input}}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${OUTPUT_DIR}/consumer")
configure_file("${SOURCE_FILE}" "${OUTPUT_DIR}/consumer/Main.ns" COPYONLY)
file(WRITE "${OUTPUT_DIR}/consumer/Consumer.nsproj"
    "<Project><PropertyGroup><Name>DictionaryStringBinary</Name><OutputType>Exe</OutputType></PropertyGroup>\n"
    "<ItemGroup><Compile Include=\"Main.ns\" />\n"
    "<LibraryReference Include=\"${CORE_LIBRARY}\" Metadata=\"${CORE_METADATA}\" />\n"
    "<LibraryReference Include=\"${COLLECTIONS_LIBRARY}\" Metadata=\"${COLLECTIONS_METADATA}\" />\n"
    "</ItemGroup></Project>\n")
set(PROJECT_FILE "${OUTPUT_DIR}/consumer/Consumer.nsproj")
include("${CMAKE_CURRENT_LIST_DIR}/VerifyDictionaryStringUpdate.cmake")
