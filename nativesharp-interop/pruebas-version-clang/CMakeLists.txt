cmake_minimum_required(VERSION 3.20)
project(ClangVersionProbe NONE)
find_package(Clang ${NSC_EXPECTED_CLANG_VERSION} EXACT CONFIG REQUIRED
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/incompatible" NO_DEFAULT_PATH)
