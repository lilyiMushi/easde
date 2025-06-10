cmake_minimum_required(VERSION 3.16)
project(MinecraftAI VERSION 2.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio objdetect)

# Try to find JsonCpp using different methods
find_package(jsoncpp QUIET)
if(NOT jsoncpp_FOUND)
    # Try alternative package names
    find_package(JsonCpp QUIET)
    if(NOT JsonCpp_FOUND)
        # Only use pkg-config if available (mainly for Linux)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(JSONCPP jsoncpp)
            if(NOT JSONCPP_FOUND)
                message(STATUS "JsonCpp not found via pkg-config, trying alternative names...")
                pkg_check_modules(JSONCPP libjsoncpp)
            endif()
        else()
            message(STATUS "PkgConfig not available, skipping pkg-config search for JsonCpp")
        endif()
    else()
        # JsonCpp found with capital J
        set(jsoncpp_FOUND TRUE)
    endif()
endif()

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    add_definitions(-D_WIN32_WINNT=0x0601)
    set(PLATFORM_LIBS ws2_32 user32 gdi32)
    
    # MSVC-specific settings
    if(MSVC)
        add_compile_options(/W4 /WX-)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
        add_definitions(-DNOMINMAX)
    endif()
else()
    # Linux/Unix settings
    set(PLATFORM_LIBS pthread)
    add_compile_options(-Wall -Wextra -O3)
endif()

# Include directories
include_directories(${OpenCV_INCLUDE_DIRS})
if(JSONCPP_INCLUDE_DIRS)
    include_directories(${JSONCPP_INCLUDE_DIRS})
endif()

# Source files - only add files that exist
set(POTENTIAL_SOURCES
    src/main.cpp
    src/MinecraftAI.cpp
    src/SkyblockStats.cpp
    src/HumanizationEngine.cpp
    src/MinecraftBot.cpp
    src/PlayerDetector.cpp
    src/ChatHandler.cpp
    src/VideoLearner.cpp
    src/WebGUI.cpp
    src/PerformanceOptimizations.cpp
)

# Check which source files actually exist
set(SOURCES)
foreach(SOURCE_FILE ${POTENTIAL_SOURCES})
    if(EXISTS ${CMAKE_SOURCE_DIR}/${SOURCE_FILE})
        list(APPEND SOURCES ${SOURCE_FILE})
        message(STATUS "Found source file: ${SOURCE_FILE}")
    else()
        message(WARNING "Source file not found: ${SOURCE_FILE}")
    endif()
endforeach()

# Make sure we have at least main.cpp
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/src/main.cpp)
    message(FATAL_ERROR "main.cpp not found! Please ensure src/main.cpp exists.")
endif()

# Header files
set(HEADERS
    src/MinecraftAI.h
)

# Create executable
add_executable(minecraft_ai ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(minecraft_ai 
    ${OpenCV_LIBS}
    ${PLATFORM_LIBS}
)

# Link JsonCpp with improved handling
if(jsoncpp_FOUND)
    target_link_libraries(minecraft_ai jsoncpp_lib)
elseif(JsonCpp_FOUND)
    target_link_libraries(minecraft_ai JsonCpp::JsonCpp)
elseif(JSONCPP_FOUND)
    target_link_libraries(minecraft_ai ${JSONCPP_LIBRARIES})
    if(JSONCPP_LIBRARY_DIRS)
        target_link_directories(minecraft_ai PRIVATE ${JSONCPP_LIBRARY_DIRS})
    endif()
    if(JSONCPP_INCLUDE_DIRS)
        target_include_directories(minecraft_ai PRIVATE ${JSONCPP_INCLUDE_DIRS})
    endif()
else()
    message(FATAL_ERROR "JsonCpp library not found! Please install JsonCpp using vcpkg: vcpkg install jsoncpp")
endif()

# Copy web assets and configuration files (with existence checks)
if(EXISTS ${CMAKE_SOURCE_DIR}/web_gui.html)
    configure_file(${CMAKE_SOURCE_DIR}/web_gui.html ${CMAKE_BINARY_DIR}/web_gui.html COPYONLY)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/skyblock_stats.json)
    configure_file(${CMAKE_SOURCE_DIR}/skyblock_stats.json ${CMAKE_BINARY_DIR}/skyblock_stats.json COPYONLY)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/known_players.txt)
    configure_file(${CMAKE_SOURCE_DIR}/known_players.txt ${CMAKE_BINARY_DIR}/known_players.txt COPYONLY)
endif()

# Create required directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/memory)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/training_videos)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/config)

# Set executable properties
set_target_properties(minecraft_ai PROPERTIES
    OUTPUT_NAME "minecraft_ai"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Installation rules
install(TARGETS minecraft_ai
    RUNTIME DESTINATION bin
)

# Install files only if they exist
if(EXISTS ${CMAKE_SOURCE_DIR}/web_gui.html)
    install(FILES ${CMAKE_SOURCE_DIR}/web_gui.html DESTINATION bin)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/skyblock_stats.json)
    install(FILES ${CMAKE_SOURCE_DIR}/skyblock_stats.json DESTINATION bin)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/known_players.txt)
    install(FILES ${CMAKE_SOURCE_DIR}/known_players.txt DESTINATION bin)
endif()

# Print configuration info
message(STATUS "=== Minecraft AI Configuration ===")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV libs: ${OpenCV_LIBS}")
if(jsoncpp_FOUND)
    message(STATUS "JsonCpp: Found (CMake package - lowercase)")
elseif(JsonCpp_FOUND)
    message(STATUS "JsonCpp: Found (CMake package - capitalized)")
elseif(JSONCPP_FOUND)
    message(STATUS "JsonCpp: Found (pkg-config)")
    message(STATUS "JsonCpp libs: ${JSONCPP_LIBRARIES}")
else()
    message(STATUS "JsonCpp: Not found!")
endif()
message(STATUS "Platform libs: ${PLATFORM_LIBS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "===================================")