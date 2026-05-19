cmake_minimum_required(VERSION 3.20)
project(reaction_diffusion CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type defaults to Debug if not set
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(raylib REQUIRED)
find_package(OpenMP REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBLO REQUIRED liblo)

add_executable(reaction_diffusion src/main.cpp)

target_include_directories(reaction_diffusion PRIVATE ${LIBLO_INCLUDE_DIRS})
target_compile_options(reaction_diffusion PRIVATE ${LIBLO_CFLAGS_OTHER})
target_link_libraries(reaction_diffusion PRIVATE raylib OpenMP::OpenMP_CXX ${LIBLO_LIBRARIES})

target_compile_options(reaction_diffusion PRIVATE
        $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra>
        $<$<CONFIG:Release>:-O3 -march=native -DNDEBUG>
)
