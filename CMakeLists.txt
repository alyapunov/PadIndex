cmake_minimum_required(VERSION 3.5)
project(PadIndex)

SET(DEFAULT_BUILD_TYPE "Debug")
IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
ENDIF()
SET(EXPECTED_BUILD_TYPES Debug Release RelWithDebInfo MinSizeRel)
IF("${CMAKE_BUILD_TYPE}" IN_LIST EXPECTED_BUILD_TYPES)
    MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
ELSE()
    MESSAGE(WARNING "Unexcpected build type: ${CMAKE_BUILD_TYPE}")
ENDIF()

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_C_STANDARD 11)
SET(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Werror")
SET(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -Werror")

include_directories(.)
add_executable(PadIndex
        PadIndex.cpp Timer.hpp Utils.hpp DbFileReader.hpp
        Db.hpp Db.cpp Index.hpp Index.cpp Filters.hpp Filters.cpp
        Benchmarks.hpp Benchmarks.cpp
        dynamic_bitset.hpp)
