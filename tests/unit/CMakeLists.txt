add_executable(
    nsc_unit_tests
    consolidated/main.cpp
    consolidated/TestRegistry.cpp)

target_include_directories(
    nsc_unit_tests
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(nsc_unit_tests PRIVATE cxx_std_20)
nsc_enable_warnings(nsc_unit_tests)

add_test(NAME nsc.unit.consolidated COMMAND nsc_unit_tests --all)
