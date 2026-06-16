cmake_minimum_required(VERSION 3.14)
project(isotropic_remesher LANGUAGES CXX)
include(CTest)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

### Compiler options
set( CMAKE_EXPORT_COMPILE_COMMANDS 1 ) # Emit a compile flags file to support completion engines 


include(cmake/CPM.cmake)

CPMAddPackage(
    NAME pmp-library
    GITHUB_REPOSITORY pmp-library/pmp-library
    GIT_TAG 3.0.0
    OPTIONS 
        "PMP_BUILD_VIS OFF"
        "PMP_BUILD_APPS OFF"
        "PMP_BUILD_TESTS OFF"
)

CPMAddPackage(
    NAME polyscope
    GITHUB_REPOSITORY nmwsharp/polyscope
    GIT_TAG v2.6.1
)

CPMAddPackage(
  NAME portable_file_dialogs
  GITHUB_REPOSITORY samhocevar/portable-file-dialogs
  GIT_TAG 0.1.0
)
 
include_directories(include)

set(REMESHER_CORE_SOURCES
    src/core/geo_utils.cpp
    src/remesher/remesher_base.cpp
    src/remesher/remeshers/remesher_standard.cpp
    src/remesher/remeshers/remesher_prio_local.cpp
    src/remesher/remeshers/remesher_prio_global.cpp
    src/remesher/evaluation_strategy.cpp
    src/remesher/loss.cpp
)

add_library(ba_remeshing_core STATIC ${REMESHER_CORE_SOURCES})
target_link_libraries(ba_remeshing_core PUBLIC pmp atomic)
target_include_directories(ba_remeshing_core PUBLIC include)

add_executable(
    remesher
    src/main.cpp
    src/ui/app_viewer.cpp
    src/ui/visuals.cpp
    src/io/logger.cpp
    src/io/plot.cpp
    src/io/paraview.cpp
)

target_include_directories(remesher PRIVATE "${polyscope_SOURCE_DIR}/deps/args")
target_include_directories(remesher PRIVATE "${polyscope_SOURCE_DIR}/deps/json/include")
target_compile_definitions(remesher PRIVATE BA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

target_link_libraries(remesher PRIVATE ba_remeshing_core polyscope portable_file_dialogs)

if(BUILD_TESTING)
    set(GTEST_ROOT "${pmp-library_SOURCE_DIR}/external/googletest-1.13.0/googletest")
    add_library(ba_googletest STATIC
        "${GTEST_ROOT}/src/gtest-all.cc"
        "${GTEST_ROOT}/src/gtest_main.cc"
    )
    target_include_directories(ba_googletest PRIVATE "${GTEST_ROOT}")
    target_include_directories(ba_googletest PUBLIC "${GTEST_ROOT}/include")
    add_executable(remesher_tests tests/remesher_tests.cpp)
    target_compile_definitions(remesher_tests PRIVATE BA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(remesher_tests PRIVATE ba_remeshing_core ba_googletest)
    add_test(NAME remesher_tests COMMAND remesher_tests)
endif()
