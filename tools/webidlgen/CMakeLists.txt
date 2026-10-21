cmake_minimum_required(VERSION 3.16)
project(WebIDLGen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(webidlgen
    src/main.cpp
    src/IdlLexer.cpp
    src/IdlParser.cpp
    src/TypeMapper.cpp
    src/CodeGenerator.cpp
)

target_include_directories(webidlgen
    PRIVATE
        src
)

install(TARGETS webidlgen
    RUNTIME DESTINATION bin
)
