#pragma once

#include <stdexcept>
#include <string>

#include "LLMProvider.h"

namespace quantclaw::providers {

// GitHub Copilot Provider 占位实现
class GitHubCopilotProvider : public LLMProvider {
 public:
  GitHubCopilotProvider(const std::string& /*api_key*/,
                        const std::string& model,
                        const std::string& /*base_url*/)
      : model_(model) {}

  std::string Chat(std::vector<Message>& /*messages*/) override {
    throw std::runtime_error(
        "GitHub Copilot provider is not yet implemented in my_claw");
  }

  ChatResponse Chat(const std::vector<Message>& /*messages*/,
                    const tools::ToolRegistry& /*tools*/) override {
    throw std::runtime_error(
        "GitHub Copilot provider is not yet implemented in my_claw");
  }

  std::string Name() const { return "github_copilot/" + model_; }

 private:
  std::string model_;
};

}  // namespace quantclaw::providers
