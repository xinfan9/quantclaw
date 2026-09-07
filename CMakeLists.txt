cmake_minimum_required(VERSION 4.0)
project(quantclaw)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(CURL REQUIRED)

include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(quantclaw src/main.cpp
        src/config.h
        src/providers/LLMProvider.h
        src/providers/OpenAIProvider.cc
        src/providers/OpenAIProvider.h
        src/providers/AnthropicProvider.cc
        src/providers/AnthropicProvider.h
        src/session/ChatHistory.cc
        src/session/ChatHistory.h
        src/platform.h
        src/providers/HttpClient.cc
        src/providers/HttpClient.h
        src/providers/ProviderFactory.cc
        src/providers/ProviderFactory.h
        src/tools/Tool.h
        src/tools/ToolRegistry.cc
        src/tools/ToolRegistry.h
        src/tools/CalculatorTool.cc
        src/tools/CalculatorTool.h
)


target_include_directories(quantclaw PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(quantclaw PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(quantclaw PRIVATE -Wall -Wextra -O2)
endif()
