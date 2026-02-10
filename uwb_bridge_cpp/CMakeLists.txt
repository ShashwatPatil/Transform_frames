cmake_minimum_required(VERSION 3.15)
project(uwb_bridge VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for optimization
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native)
    # Debug symbols even in release for production debugging
    add_compile_options(-g)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(Threads REQUIRED)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)

# Firebase C++ SDK configuration
set(FIREBASE_CPP_SDK_DIR "/opt/firebase_cpp_sdk" CACHE PATH "Path to Firebase C++ SDK")
if(EXISTS "${FIREBASE_CPP_SDK_DIR}")
    message(STATUS "Firebase C++ SDK found at: ${FIREBASE_CPP_SDK_DIR}")
    include_directories(${FIREBASE_CPP_SDK_DIR}/include)
    link_directories(${FIREBASE_CPP_SDK_DIR}/libs/linux/x86_64/cxx11)
else()
    message(WARNING "Firebase C++ SDK not found at ${FIREBASE_CPP_SDK_DIR}. Download from: https://firebase.google.com/download/cpp")
endif()

# FetchContent for dependencies
include(FetchContent)

# spdlog for logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# nlohmann/json (header-only)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Eclipse Paho MQTT C library
message(STATUS "Fetching Paho MQTT C...")
FetchContent_Declare(
    paho-mqtt-c
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.c.git
    GIT_TAG v1.3.13
)
set(PAHO_BUILD_STATIC ON CACHE BOOL "Build static library" FORCE)
set(PAHO_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(PAHO_ENABLE_TESTING OFF CACHE BOOL "Build tests" FORCE)
set(PAHO_HIGH_PERFORMANCE OFF CACHE BOOL "High performance" FORCE)
set(PAHO_WITH_SSL ON CACHE BOOL "Enable SSL support" FORCE)
FetchContent_MakeAvailable(paho-mqtt-c)

# Set variables for Paho MQTT C++ to find the C library
set(PAHO_MQTT_C_INCLUDE_DIRS "${paho-mqtt-c_SOURCE_DIR}/src" CACHE PATH "Paho MQTT C include dir" FORCE)
set(PAHO_MQTT_C_LIBRARIES "paho-mqtt3as-static" CACHE STRING "Paho MQTT C library" FORCE)

# Eclipse Paho MQTT C++ library
message(STATUS "Fetching Paho MQTT C++...")
FetchContent_Declare(
    paho-mqtt-cpp
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.cpp.git
    GIT_TAG v1.3.2
)
set(PAHO_BUILD_STATIC ON CACHE BOOL "Build static library" FORCE)
set(PAHO_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(PAHO_BUILD_SAMPLES OFF CACHE BOOL "Build samples" FORCE)
set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "Build documentation" FORCE)
# Ensure it finds the C library we just built
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${paho-mqtt-cpp_SOURCE_DIR}/cmake)
FetchContent_MakeAvailable(paho-mqtt-cpp)

# Add Transform_frames library
add_subdirectory(${CMAKE_SOURCE_DIR}/../libuwb_transform ${CMAKE_BINARY_DIR}/transform_frames)

# Find system libraries required by Firebase
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
pkg_check_modules(GLIB REQUIRED glib-2.0 gobject-2.0)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/ConfigLoader.cpp
    src/MqttHandler.cpp
    src/BridgeCore.cpp
    src/FirestoreManager.cpp
    src/main.cpp
)

# Executable
add_executable(uwb_bridge ${SOURCES})

# Include directories for the executable
target_include_directories(uwb_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${paho-mqtt-cpp_SOURCE_DIR}/src
        ${LIBSECRET_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(uwb_bridge
    PRIVATE
        uwb_transform
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        paho-cpp-objs
        paho-mqtt3as-static
        Threads::Threads
        Eigen3::Eigen
        
        # Use linking groups to handle circular dependencies
        -Wl,--start-group
        firebase_firestore
        firebase_auth
        firebase_app
        -Wl,--end-group
        
        # System libraries required by Firebase
        ${LIBSECRET_LIBRARIES}
        ${GLIB_LIBRARIES}
        
        ssl
        crypto
        pthread
        dl
)

# Installation
install(TARGETS uwb_bridge
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION etc/uwb_bridge
    FILES_MATCHING PATTERN "*.json"
)

# Print configuration summary
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  UWB Bridge Configuration")
message(STATUS "========================================")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")
message(STATUS "")
