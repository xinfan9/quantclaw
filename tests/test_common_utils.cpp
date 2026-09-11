#include <gtest/gtest.h>

#include <string>
#include <vector>

// 测试一些基础工具函数（如 URL/字符串处理）。
namespace quantclaw {

// 简单的字符串切分辅助函数测试。
std::vector<std::string> SplitByComma(const std::string& s) {
  std::vector<std::string> result;
  std::string current;
  for (char c : s) {
    if (c == ',') {
      if (!current.empty()) result.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }
  if (!current.empty()) result.push_back(current);
  return result;
}

TEST(CommonUtilsTest, SplitByComma) {
  auto parts = SplitByComma("a,b,c");
  ASSERT_EQ(parts.size(), 3);
  EXPECT_EQ(parts[0], "a");
  EXPECT_EQ(parts[1], "b");
  EXPECT_EQ(parts[2], "c");
}

TEST(CommonUtilsTest, SplitByCommaEmpty) {
  auto parts = SplitByComma("");
  EXPECT_TRUE(parts.empty());
}

}  // namespace quantclaw
