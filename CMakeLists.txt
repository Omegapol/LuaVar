cmake_minimum_required(VERSION 3.22)
project(LuaVar)

set(CMAKE_CXX_STANDARD 20)

set(LUA_SRC_DIR "" CACHE PATH "Path to lua headers dir")
set(LUA_LIB "" CACHE PATH "Path to lua binary")
option(BUILD_TESTS "Build unit tests")
option(BUILD_BENCHMARKS "Build benchmarks")

include_directories("${PROJECT_SOURCE_DIR}/include")

find_package("Lua" REQUIRED)
add_library(Lua::Lua INTERFACE IMPORTED)
set_target_properties(Lua::Lua PROPERTIES
        INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
)

set(INCLUDE_FILES include/luavar/luavar.h include/luavar/binding_utils.h include/luavar/type_traits.h include/luavar/config.h)
set(SOURCE_FILES source/luavar/luavar.cpp source/luavar/binding_utils.cpp)
set(MAIN_FILE main.cpp)

add_executable(LuaVar ${MAIN_FILE} ${SOURCE_FILES} ${INCLUDE_FILES})
target_link_libraries(LuaVar PRIVATE Lua::Lua)

if (BUILD_TESTS OR BUILD_BENCHMARKS)
    enable_testing()
endif ()

if (BUILD_TESTS)
    set(TEST_FILES tests/test.cpp)
    add_executable(tests ${SOURCE_FILES} ${INCLUDE_FILES} ${TEST_FILES})
    find_package(Catch2 CONFIG REQUIRED)
    target_link_libraries(tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)
    target_link_libraries(tests PRIVATE Lua::Lua)
    add_test(tests tests)
endif ()

if (BUILD_BENCHMARKS)
    set(BENCH_FILES benchmarks/bench.cpp)
    add_executable(benchmark ${SOURCE_FILES} ${INCLUDE_FILES} ${BENCH_FILES})
    find_package(Catch2 CONFIG REQUIRED)
    target_link_libraries(benchmark PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)
    target_link_libraries(benchmark PRIVATE Lua::Lua)
    add_test(benchmark benchmark)
endif ()
