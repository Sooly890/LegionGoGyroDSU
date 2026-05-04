cmake_minimum_required(VERSION 3.10)

project (LegionGoSGyro)

set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(SOURCES
    src/main.cpp
    src/dsu.cpp
    src/iio.cpp
)

add_executable(LegionGoSGyro ${SOURCES})

target_include_directories(LegionGoSGyro
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/CRCpp/inc
)

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBIIO REQUIRED libiio)

include_directories(${LIBIIO_INCLUDE_DIRS})

target_compile_options(LegionGoSGyro PRIVATE
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Debug>:-O0 -g>
)

target_link_libraries(LegionGoSGyro PRIVATE Threads::Threads ${LIBIIO_LIBRARIES})
