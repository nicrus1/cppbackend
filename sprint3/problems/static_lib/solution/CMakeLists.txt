cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Threads first
find_package(Threads REQUIRED)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

# ============================================================================
# STATIC LIBRARY with common game logic
# ============================================================================
add_library(game_lib STATIC
    src/model.cpp
    src/json_loader.cpp
    src/game_state.cpp
    src/loot_manager.cpp
    src/loot_generator.cpp
    src/htmldecode.cpp
)

# Include directories for the library (PUBLIC so tests can see them too)
target_include_directories(game_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries to the static library
target_link_libraries(game_lib PUBLIC
    CONAN_PKG::boost
    Threads::Threads
)

# ============================================================================
# MAIN EXECUTABLE
# ============================================================================
add_executable(game_server
    src/main.cpp
    src/request_handler.cpp
    src/http_server.cpp
    src/logger.cpp
    src/boost_json.cpp
)

target_link_libraries(game_server PRIVATE
    game_lib
    CONAN_PKG::boost
    Threads::Threads
)

# ============================================================================
# TESTS
# ============================================================================
add_executable(game_server_tests
    tests/tests.cpp
    tests/loot_tests.cpp
    tests/loot_generator_tests.cpp
)

target_link_libraries(game_server_tests PRIVATE
    game_lib
    CONAN_PKG::catch2
    CONAN_PKG::boost
    Threads::Threads
)

# ============================================================================
# OUTPUT DIRECTORY
# ============================================================================
set_target_properties(game_server game_server_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ============================================================================
# TESTING
# ============================================================================
enable_testing()
add_test(NAME game_server_tests COMMAND game_server_tests)

# Optional: Integrate with CTest for better test discovery
# include(CTest)
# include(${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake)
# catch_discover_tests(game_server_tests)