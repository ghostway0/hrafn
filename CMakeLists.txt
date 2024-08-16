cmake_minimum_required(VERSION 3.26)

project(doomday LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(asio REQUIRED)
find_package(Sodium REQUIRED)
find_package(Protobuf CONFIG)
find_package(Protobuf REQUIRED)
find_package(Absl REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(doctest CONFIG REQUIRED)

set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_FILES
    ${PROTO_DIR}/messages.proto
)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_executable(doomday
    src/main.cpp
    src/uuid.cpp
    src/multiaddr.cpp
    src/semantic_version.cpp
    ${PROTO_SRCS}
)
target_include_directories(doomday PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(doomday PRIVATE
    ${PROTO_HDRS}
    "/opt/homebrew/Cellar/llvm@17/17.0.6/include/c++"
)

target_link_libraries(doomday PRIVATE
    asio::asio
    protobuf::libprotobuf
    absl::strings
    fmt::fmt
    spdlog::spdlog
    sodium
)

add_executable(test_utils src/test_utils.cpp src/uuid.cpp)
target_link_libraries(test_utils PRIVATE doctest::doctest protobuf::libprotobuf absl::strings)

add_executable(test_semver src/test_semver.cpp src/semantic_version.cpp)
target_link_libraries(test_semver PRIVATE doctest::doctest absl::strings)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT result)

    if(result)
        set_property(TARGET doomday PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto -gdwarf-4")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "-gdwarf-4")
endif()

# Set general compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -fno-exceptions")
