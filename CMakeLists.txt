cmake_minimum_required(VERSION 3.14)


project(CW LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS  OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

# для работы с JSON
include(FetchContent)
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

set(PROJECT_SOURCES
    src/main.cpp
    src/parser/ast.cpp
    src/parser/parser.cpp
    src/parser/lexer.cpp

    src/platform/console.cpp

    src/execution/executor.cpp
    src/execution/serializer.cpp

    src/storage/storage.cpp
    src/storage/disk/disk_manager.cpp
    src/storage/pager/pager.cpp

    src/index/index_manager.cpp
    src/index/btree.cpp
)

add_executable(prog ${PROJECT_SOURCES})
target_include_directories(prog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(prog PRIVATE nlohmann_json::nlohmann_json)


# юнит-тесты, по умолчанию выкл: cmake -DBUILD_TESTS=ON ..
option(BUILD_TESTS "Build GoogleTest unit tests" OFF)

if(BUILD_TESTS)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # runtime под MSVC
    FetchContent_MakeAvailable(googletest)

    # исходники без main.cpp
    set(TEST_SOURCES ${PROJECT_SOURCES})
    list(REMOVE_ITEM TEST_SOURCES src/main.cpp)

    add_executable(cw_tests
        ${TEST_SOURCES}
        tests/test_lexer.cpp
        tests/test_parser.cpp
        tests/test_executor.cpp
        tests/test_btree.cpp
    )
    target_include_directories(cw_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(cw_tests PRIVATE nlohmann_json::nlohmann_json GTest::gtest_main)

    enable_testing()
    add_test(NAME cw_tests COMMAND cw_tests)
endif()
