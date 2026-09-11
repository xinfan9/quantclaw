//
// 由 xinfang 创建于 2026/9/4.
//

#include "CalculatorTool.h"
#include <cmath>
#include <iostream>

namespace quantclaw::tools {
CalculatorTool::CalculatorTool() {
  name = "calculator";
  description = "Evaluate a mathematical expression. Supports +, -, *, /, parentheses, and decimal numbers.";
  parameters = {
    {"type", "object"},
    {"properties",
     {{"expression",
       {{"type", "string"},
        {"description", "The mathematical expression to evaluate, e.g. '1 + 2 * 3'"}}}}},
    {"required", nlohmann::json::array({"expression"})}};
}

class ExpressionEvaluator {
public:
  explicit ExpressionEvaluator(const std::string& expr) : _expr(expr), _pos(0) {}

  double Evaluate() {
    SkipSpaces();
    double value = ParseExpression();
    SkipSpaces();
    if (_pos < _expr.size()) {
      throw std::runtime_error("Unexpected character at position " + std::to_string(_pos));
    }
    return value;
  }

private:
  std::string _expr;
  size_t _pos;

  void SkipSpaces() {
    // 所有使用 <cctype> 中的函数（isspace、isdigit、isalpha、toupper、tolower 等）时，
    // 对 char 类型的参数都加上 static_cast<unsigned char>。这是一种防御性编程的做法，可以避免潜在的未定义行为。
    while (_pos < _expr.size() && std::isspace(static_cast<unsigned char>(_expr[_pos]))) ++_pos;
  }

  double ParseExpression() {
    double value = ParseTerm();
    while (true) {
      SkipSpaces();
      if (_pos >= _expr.size()) break;
      char op = _expr[_pos];
      if (op != '+' && op != '-') break;
      ++_pos;
      double term = ParseTerm();
      if (op == '+') {
        value += term;
      } else {
        value -= term;
      }
    }

    return value;
  }

  double ParseTerm() {
    double value = ParseFactor();
    while (true) {
      SkipSpaces();
      if (_pos >= _expr.size()) break;

      char op = _expr[_pos];
      if (op != '*' && op != '/') break;
      ++_pos;
      double factor = ParseFactor();
      if (op == '*') value *= factor;
      else if (factor == 0.0) throw std::runtime_error("Division by zero");
      else value /= factor;
    }

    return value;
  }
  double ParseFactor() {
    SkipSpaces();
    if (_pos >= _expr.size()) {
      throw std::runtime_error("Unexpected end of expression");
    }
   
    if (_expr[_pos] == '(') {
      ++_pos;
      double value = ParseExpression();
      SkipSpaces();
      if (_pos >= _expr.size() || _expr[_pos] != ')') {
        throw std::runtime_error("Missing closing parenthesis");
      }
      ++_pos;
      return value;
    }
   
    return ParseNumber();
  }
   
  double ParseNumber() {
    SkipSpaces();
    size_t start = _pos;
    bool has_dot = false;
   
    while (_pos < _expr.size()) {
      char c = _expr[_pos];
      if (std::isdigit(static_cast<unsigned char>(c))) {
        ++_pos;
      } else if (c == '.' && !has_dot) {
        has_dot = true;
        ++_pos;
      } else {
        break;
      }
    }
   
    if (start == _pos) {
      throw std::runtime_error("Expected number at position " +
                               std::to_string(_pos));
    }
   
    return std::stod(_expr.substr(start, _pos - start));
  }
};

static double EvaluateExpression(const std::string& expr) {
  ExpressionEvaluator evaluator(expr);
  return evaluator.Evaluate();
}

std::string CalculatorTool::Execute(const nlohmann::json& args) const {
  if (!args.contains("expression") || !args["expression"].is_string()) {
    throw std::runtime_error("Missing 'expression' argument");
  }

  std::string expression = args["expression"].get<std::string>();
  double result = EvaluateExpression(expression);

  // 去掉小数点后多余的 0
  if (std::floor(result) == result) {
    return std::to_string(static_cast<long long>(result));
  }
  return std::to_string(result);
}
}
