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
find_package(Threads REQUIRED)

# Firebase C++ SDK configuration
set(FIREBASE_CPP_SDK_DIR "/opt/firebase_cpp_sdk" CACHE PATH "Path to Firebase C++ SDK")
if(EXISTS "${FIREBASE_CPP_SDK_DIR}")
    message(STATUS "Firebase C++ SDK found at: ${FIREBASE_CPP_SDK_DIR}")
    include_directories(${FIREBASE_CPP_SDK_DIR}/include)
    link_directories(${FIREBASE_CPP_SDK_DIR}/libs/linux/x86_64/cxx11)
else()
    message(WARNING "Firebase C++ SDK not found at ${FIREBASE_CPP_SDK_DIR}. Download from: https://firebase.google.com/download/cpp")
endif()

# Library target
# Build as STATIC to avoid -fPIC issues with Firebase static libraries
add_library(uwb_transform STATIC
    src/FloorplanTransformer.cpp
)

target_link_libraries(uwb_transform
    PUBLIC
        Eigen3::Eigen
        Threads::Threads
        pthread
        dl
)

target_include_directories(uwb_transform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
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
