cmake_minimum_required(VERSION 3.20)
project(shardcache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(SHARDCACHE_BUILD_TESTS "Build unit tests (fetches GoogleTest)" ON)
option(SHARDCACHE_TSAN "Build with ThreadSanitizer" OFF)
option(SHARDCACHE_ASAN "Build with AddressSanitizer + UBSan" OFF)

if(SHARDCACHE_TSAN)
  add_compile_options(-fsanitize=thread -g -O1)
  add_link_options(-fsanitize=thread)
endif()
if(SHARDCACHE_ASAN)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1)
  add_link_options(-fsanitize=address,undefined)
endif()

find_package(Threads REQUIRED)

add_library(shardcache
  src/lru_cache.cc
  src/locked_cache.cc
  src/sharded_cache.cc
  src/protocol.cc
  src/server.cc
  src/client.cc
)
target_include_directories(shardcache PUBLIC include)
target_link_libraries(shardcache PUBLIC Threads::Threads)
target_compile_options(shardcache PRIVATE -Wall -Wextra -Wpedantic)

add_executable(shardcache-server src/main.cc)
target_link_libraries(shardcache-server PRIVATE shardcache)

add_executable(shardcache-bench bench/bench.cc)
target_link_libraries(shardcache-bench PRIVATE shardcache)

add_executable(shardcache-loadgen bench/loadgen.cc)
target_link_libraries(shardcache-loadgen PRIVATE shardcache)

if(SHARDCACHE_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(shardcache-tests
    tests/lru_cache_test.cc
    tests/cache_concurrency_test.cc
    tests/protocol_test.cc
    tests/server_test.cc
  )
  target_link_libraries(shardcache-tests PRIVATE shardcache GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(shardcache-tests DISCOVERY_TIMEOUT 30)
endif()
