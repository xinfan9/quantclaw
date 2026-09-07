#pragma once
#include "LLMProvider.h"

namespace quantclaw::providers {

/***
public 继承：保持访问级别，用于 is-a 关系和多态。接口基类必须用它。
protected 继承：基类 public 变 protected，很少用。
private 继承：基类成员全变 private，只复用实现，不建立类型关系。
class 默认 private 继承，所以写继承时不要省略 public
*/
class OpenAIProvider : public LLMProvider {
public:
  OpenAIProvider(std::string api_key, std::string model, std::string base_url);
  std::string Chat(std::vector<Message>&) override;
  ChatResponse Chat(const std::vector<Message>& messages, const tools::ToolRegistry& tools) override;

private:
  std::string _api_key;
  std::string _model;
  std::string _base_url;
};

}
