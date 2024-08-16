cmake_minimum_required(VERSION 3.26)

project(doomday LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(asio REQUIRED)
find_package(FlatBuffers REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Sodium REQUIRED)

set(FLATBUFFERS_SCHEMA_FILES ${CMAKE_CURRENT_SOURCE_DIR}/schemas/messages.fbs)
set(FLATBUFFERS_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

file(MAKE_DIRECTORY ${FLATBUFFERS_GENERATED_DIR})

add_custom_target(generate_flatbuffers ALL)

foreach(FBS_FILE IN LISTS FLATBUFFERS_SCHEMA_FILES)
    get_filename_component(FBS_NAME ${FBS_FILE} NAME_WE)

    add_custom_command(
        TARGET generate_flatbuffers
        COMMAND flatc
        --cpp --cpp-ptr-type flatbuffers::unique_ptr --gen-mutable --no-includes --scoped-enums --gen-object-api -o ${FLATBUFFERS_GENERATED_DIR} ${FBS_FILE}
        MAIN_DEPENDENCY ${FBS_FILE}
        COMMENT "Built header for ${FBS_NAME} schema"
        VERBATIM
    )
endforeach()

add_executable(doomday
    src/main.cpp
)

add_dependencies(doomday generate_flatbuffers)

target_include_directories(doomday PRIVATE ${FLATBUFFERS_GENERATED_DIR}/..)
target_include_directories(doomday PRIVATE "/opt/homebrew/Cellar/llvm/18.1.8/include/c++")

target_link_libraries(doomday PRIVATE asio::asio fmt::fmt spdlog::spdlog sodium)

include(CheckIPOSupported)
check_ipo_supported(RESULT result)

if(result)
    set_property(TARGET doomday PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

set(CMAKE_CXX_FLAGS ${CMAKE_C_FLAGS} "-march=native -fno-exceptions")
set(CMAKE_CXX_FLAGS_DEBUG "-gdwarf-4")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto -gdwarf-4")
