#include <iostream>

#include "config.h"
#include "providers/OpenAIProvider.h"
#include "providers/ProviderFactory.h"
#include "session/ChatHistory.h"
#include "tools/CalculatorTool.h"
#include "tools/ToolRegistry.h"

// TIP 要<b>Run</b>代码，请按 <shortcut actionId="Run"/> 或点击装订区域中的 <icon src="AllIcons.Actions.Execute"/> 图标。
int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: quantclaw <user_message>" << std::endl;
    return 1;
  }

  // 汇集参数
  std::string user_message;
  for (int i = 1; i < argc; ++i) {
    if (!user_message.empty()) user_message += " ";
    user_message += argv[i];
  }

  try {
    auto cfg = quantclaw::Config::Load();

    auto provider = quantclaw::providers::CreateProvider(cfg);

    quantclaw::tools::ToolRegistry tools;
    tools.Register(std::make_unique<quantclaw::tools::CalculatorTool>());

    quantclaw::session::ChatHistory history;
    auto messages = history.Load();


    constexpr size_t kMaxHistory = 20;
    if (messages.size() > kMaxHistory) messages = std::vector(messages.end() - kMaxHistory, messages.end());
    if (messages.empty()) {
      messages.push_back({"system",
                          "You are a helpful assistant. Use tools when "
                          "they can help answer the user's question.",
                          "",
                          {}});
    }

    messages.push_back({"user", user_message, "", {}});


    std::string final_reply;
    for (int iteration = 0; iteration < 5; ++iteration) {
      auto response = provider->Chat(messages, tools);
      if (!response.isToolCall()) {
        final_reply = response.content;
        messages.push_back({"assistant", final_reply, "", {}});
        break;
      }

      std::cout << "[calling tools: ";
      for (const auto& tool_call : response.tool_calls) {
        std::cout << tool_call.name << " ";
      }
      std::cout << "]\n";

      quantclaw::providers::Message assistant_msg;
      assistant_msg.role = "assistant";
      assistant_msg.content = response.content;
      assistant_msg.tool_calls = response.tool_calls;
      messages.push_back(assistant_msg);

      for (const auto& tool_call : response.tool_calls) {
        std::string result = tools.Execute(tool_call.name, tool_call.arguments);
        messages.push_back({"tool", result, tool_call.id, {}});
      }
    }


    std::cout << final_reply << std::endl;

    history.Save(messages);

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}