cmake_minimum_required(VERSION 3.16)
project(PersonalBlockchain VERSION 2.0.0 LANGUAGES CXX)

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

# std::filesystem may require explicit linking on GCC < 9
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(blockchain_node PRIVATE stdc++fs)
endif()

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

option(BUILD_TESTS "Build lightweight integration tests" ON)
if(BUILD_TESTS)
    add_executable(wallet_system_tests
        tests/wallet_system_tests.cpp
        src/block/block.cpp
        src/core/blockchain.cpp
        src/state/state_manager.cpp
        src/transaction/transaction.cpp
        src/wallet/wallet.cpp
        src/crypto/sha256.cpp
        src/crypto/ecdsa.cpp
        src/consensus/pow_engine.cpp
    )

    target_include_directories(wallet_system_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
    )

    target_link_libraries(wallet_system_tests PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        Boost::system
        Threads::Threads
    )
endif()
