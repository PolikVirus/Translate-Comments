cmake_minimum_required(VERSION 3.21)

# hard-disable PCH + IPO globally (prevents cmake_pch.cxx and /GL -> LTCG)
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
project(CommentTranslator VERSION 1.0.2)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})

# make absolutely sure the target doesn't use PCH/IPO even if setup_geode_mod adds them
set_property(TARGET ${PROJECT_NAME} PROPERTY DISABLE_PRECOMPILE_HEADERS ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY PRECOMPILE_HEADERS "")

set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)

if (MSVC)
    # kill /GL and LTCG and a few other "aggressive" opts that tend to trip v180 crashes
    target_compile_options(${PROJECT_NAME} PRIVATE /GL- /Gw- /Gy- /bigobj)
    target_link_options(${PROJECT_NAME} PRIVATE /LTCG:OFF)
endif()