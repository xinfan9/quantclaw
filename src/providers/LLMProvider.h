#pragma once
#include<string>
#include<vector>
#include <nlohmann/json.hpp>

namespace quantclaw::tools {
class ToolRegistry;
}

namespace quantclaw::providers {

struct Message {
  std::string role;
  std::string content;

  std::string tool_call_id;
  std::vector<struct ToolCall> tool_calls;
};

struct ToolCall {
  std::string id;
  std::string name;
  nlohmann::json arguments;
};

struct ChatResponse {
  std::string content;
  std::vector<ToolCall> tool_calls;

  [[nodiscard]] bool isToolCall() const {
    return !tool_calls.empty();
  }
};


class LLMProvider {
public:
  /***
  虚函数是cpp中运行时多态的核心机制
  析构函数声明为虚函数，确保派生类对象在通过基类指针删除时能够正确调用析构函数
  = default 表示使用编译器自动生成的默认实现
  基类用 virtual 声明函数。
  派生类可以**重写（override）**这个函数。
  通过基类指针或引用调用时，实际执行的是派生类版本。

  编译器为每个包含虚函数的类生成一张虚函数表（vtable），里面存放该类所有虚函数的地址。
  创建对象时，对象内存布局中会多一个隐藏指针 vptr，指向该类的 vtable：
  译器不会直接调用 LLMProvider::chat，而是：
  1.通过 p 找到 vptr。
  2.通过 vptr 找到 vtable。
  3.在 vtable 中查找 chat 的地址。

  包含纯虚函数的类称为抽象类（abstract class）。
  抽象类不能实例化：
  ***/
  virtual ~LLMProvider() = default;
  virtual std::string Chat(std::vector<Message>&) = 0;
  virtual ChatResponse Chat(const std::vector<Message>& messages, const tools::ToolRegistry& tools) = 0;
};
} // providers
