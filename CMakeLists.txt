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
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
        GIT_SHALLOW TRUE
)

FetchContent_Declare(
        ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/ixwebsocket.git
        GIT_TAG v11.4.5
        GIT_SHALLOW TRUE
)

FetchContent_Declare(
        cpp_httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.15.3
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(nlohmann_json spdlog ixwebsocket cpp_httplib)

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
        src/tools/Tool.h
        src/tools/ToolRegistry.cc
        src/tools/ToolRegistry.h
        src/tools/CalculatorTool.cc
        src/tools/CalculatorTool.h
        src/core/MemoryEngine.cc
        src/core/MemoryEngine.h
        src/mcp/StdioTransport.cc
        src/mcp/StdioTransport.h
        src/mcp/MCPClient.cc
        src/mcp/MCPClient.h
        src/mcp/MCPTool.cc
        src/mcp/MCPTool.h
        src/gateway/JsonRpcMessage.cc
        src/gateway/JsonRpcMessage.h
        src/gateway/GatewayServer.cc
        src/gateway/GatewayServer.h
        src/gateway/GatewayClient.cc
        src/gateway/GatewayClient.h
        src/plugins/SidecarManager.cc
        src/plugins/SidecarManager.h
        src/plugins/SidecarTool.cc
        src/plugins/SidecarTool.h
        src/web/WebServer.cc
        src/web/WebServer.h
        src/cli/CliManager.cc
        src/cli/CliManager.h
        src/providers/ProviderRegistry.cc
        src/providers/ProviderRegistry.h
        src/providers/ProviderError.h
        src/providers/CooldownTracker.cc
        src/providers/CooldownTracker.h
        src/providers/FailoverResolver.cc
        src/providers/FailoverResolver.h
        src/providers/EmbeddingProvider.h
        src/providers/OpenAIEmbeddingProvider.cc
        src/providers/OpenAIEmbeddingProvider.h
        src/core/VectorIndex.cc
        src/core/VectorIndex.h
        src/core/ContextPruner.cc
        src/core/ContextPruner.h
        src/core/ContextEngine.cc
        src/core/ContextEngine.h
        src/security/ToolPermissionChecker.cc
        src/security/ToolPermissionChecker.h
        src/security/ExecApprovalManager.cc
        src/security/ExecApprovalManager.h
)


target_include_directories(quantclaw PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(quantclaw PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        ixwebsocket::ixwebsocket
        httplib::httplib
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(quantclaw PRIVATE -Wall -Wextra -O2)
endif()
