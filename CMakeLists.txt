cmake_minimum_required(VERSION 3.14)
project(isotropic_remesher LANGUAGES CXX)

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

add_executable(
    remesher 
    src/main.cpp
    src/core/geo_utils.cpp
    src/ui/app_viewer.cpp
    src/ui/visuals.cpp
    src/remesher/remesher_base.cpp
    src/remesher/remeshers/remesher_standard.cpp
    src/remesher/remeshers/remesher_prio_local.cpp
    src/remesher/remeshers/remesher_prio_global.cpp
    src/remesher/evaluation_strategy.cpp
    src/remesher/loss.cpp
    src/io/logger.cpp
    src/io/paraview.cpp
)

target_include_directories(remesher PRIVATE "${polyscope_SOURCE_DIR}/deps/args")
target_include_directories(remesher PRIVATE "${polyscope_SOURCE_DIR}/deps/json/include")

target_link_libraries(remesher PRIVATE pmp polyscope portable_file_dialogs atomic)