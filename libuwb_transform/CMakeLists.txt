cmake_minimum_required(VERSION 3.15)
project(uwb_transform VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for optimization
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -O3 -march=native)
endif()

# Find required packages
find_package(Eigen3 3.3 REQUIRED NO_MODULE)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Library target
add_library(uwb_transform SHARED
    src/FloorplanTransformer.cpp
)

target_link_libraries(uwb_transform
    PUBLIC
        Eigen3::Eigen
)

target_include_directories(uwb_transform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Executable target
add_executable(uwb_transform_demo
    src/main.cpp
)

target_link_libraries(uwb_transform_demo
    PRIVATE
        uwb_transform
)

# Installation rules
install(TARGETS uwb_transform uwb_transform_demo
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

install(DIRECTORY config/
    DESTINATION config
    FILES_MATCHING PATTERN "*.json"
)

# Print configuration summary
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  UWB Transform Library Configuration")
message(STATUS "========================================")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "Eigen3 version:   ${Eigen3_VERSION}")
message(STATUS "Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")
message(STATUS "")
