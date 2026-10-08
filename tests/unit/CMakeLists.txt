add_executable(
    nsc_unit_tests
    consolidated/main.cpp
    consolidated/TestRegistry.cpp
    modules/BasicUnitTests.cpp
    modules/DiagnosticsUnitTests.cpp
    modules/LexerUnitTests.cpp
    modules/NumericUnitTests.cpp
    modules/SourceUnitTests.cpp)

target_include_directories(
    nsc_unit_tests
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(nsc_unit_tests PRIVATE cxx_std_20)
target_compile_definitions(
    nsc_unit_tests
    PRIVATE NSC_TEST_CASES_DIR="${CMAKE_SOURCE_DIR}/tests/cases")
target_link_libraries(
    nsc_unit_tests
    PRIVATE nsc_basic nsc_compilation nsc_frontend nsc_target)
nsc_enable_warnings(nsc_unit_tests)

add_test(NAME nsc.unit.consolidated COMMAND nsc_unit_tests --all)
