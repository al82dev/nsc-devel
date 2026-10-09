cmake_minimum_required(VERSION 3.25)

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(NativeSharpCompilerSuiteInfrastructure LANGUAGES CXX)
    enable_testing()
endif()

add_executable(
    nsc_component_tests
    host/src/main.cpp
    host/src/TestRegistry.cpp
    host/modules/InfrastructureModule.cpp)

target_include_directories(
    nsc_component_tests
    PRIVATE host/include)
target_compile_features(nsc_component_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(nsc_component_tests PRIVATE /W4 /WX)
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(nsc_component_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(nsc_component_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME nsc.compiler-suite.infrastructure COMMAND nsc_component_tests --all)
