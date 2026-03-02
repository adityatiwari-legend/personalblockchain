cmake_minimum_required(VERSION 3.16)
project(PersonalBlockchain VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Try to find Boost; if not found, we handle it gracefully
find_package(Boost 1.70 REQUIRED COMPONENTS system)

# Collect all source files
file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

# Create executable
add_executable(blockchain_node ${SOURCES})

# Include directories
target_include_directories(blockchain_node PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(blockchain_node PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Threads::Threads
)

# Platform-specific settings
if(WIN32)
    target_link_libraries(blockchain_node PRIVATE ws2_32 wsock32)
    target_compile_definitions(blockchain_node PRIVATE _WIN32_WINNT=0x0601)
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blockchain_node PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(blockchain_node PRIVATE /W4)
endif()
