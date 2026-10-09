cmake_minimum_required(VERSION 3.25)

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(NativeSharpCompilerSuiteInfrastructure LANGUAGES CXX)
    enable_testing()
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(
    nsc_component_tests
    host/src/main.cpp
    host/src/TestRegistry.cpp
    host/modules/InfrastructureModule.cpp
    host/modules/BasicValuesModule.cpp)

target_include_directories(
    nsc_component_tests
    PRIVATE
        host/include
        "${CMAKE_CURRENT_LIST_DIR}/../../include")
target_compile_features(nsc_component_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(nsc_component_tests PRIVATE /W4 /WX)
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(nsc_component_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(nsc_component_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME nsc.compiler-suite.infrastructure COMMAND nsc_component_tests --all)
add_test(NAME nsc.compiler-suite.host-basic-values COMMAND nsc_component_tests --component Host.Basic)
add_test(
    NAME nsc.compiler-suite.controller-infrastructure
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_infrastructure_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/controller-gate")
add_test(
    NAME nsc.compiler-suite.host-basic-values-controller
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/controller/integration/run_host_basic_values_gate.py"
        --host-executable "$<TARGET_FILE:nsc_component_tests>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --repository-root "${CMAKE_CURRENT_LIST_DIR}/../.."
        --work-parent "${CMAKE_CURRENT_BINARY_DIR}/host-basic-values-gate"
        --build-mode "$<CONFIG>")
