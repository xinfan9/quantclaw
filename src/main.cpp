#include <iostream>

#include "config.h"
#include "providers/OpenAIProvider.h"
#include "session/ChatHistory.h"

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

    quantclaw::providers::OpenAIProvider provider(cfg.api_key, cfg.model, cfg.base_url);

    quantclaw::session::ChatHistory history;
    auto messages = history.Load();


    constexpr size_t kMaxHistory = 20;
    if (messages.size() > kMaxHistory) messages = std::vector(messages.end() - kMaxHistory, messages.end());
    if (messages.empty()) messages.push_back({"system", "You are a helpful assistant."});

    messages.push_back({"user", user_message});
    std::string reply = provider.Chat(messages);
    std::cout << reply << std::endl;

    messages.push_back({"assistant", reply});
    history.Save(messages);

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}