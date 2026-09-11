#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>

#include "config.h"

namespace quantclaw {

// 临时清除/恢复环境变量的 RAII 辅助类。
class EnvGuard {
 public:
  explicit EnvGuard(const std::string& name) : name_(name) {
    const char* value = std::getenv(name_.c_str());
    if (value) {
      saved_ = value;
      unsetenv(name_.c_str());
    }
  }
  ~EnvGuard() {
    if (saved_.has_value()) {
      setenv(name_.c_str(), saved_->c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
  }

 private:
  std::string name_;
  std::optional<std::string> saved_;
};

TEST(ConfigTest, DefaultPathExpansion) {
  std::string path = Config::ExpandHome("~/test.json");
  const char* home = std::getenv("HOME");
  if (home) {
    EXPECT_NE(path.find(home), std::string::npos);
  } else {
    EXPECT_EQ(path, "~/test.json");
  }
}

TEST(ConfigTest, LoadFromMissingFileReturnsDefaults) {
  EnvGuard guard1("CLAW_API_KEY");
  EnvGuard guard2("CLAW_MODEL");
  EnvGuard guard3("CLAW_BASE_URL");

  auto cfg = Config::LoadFromFile("/nonexistent/path/config.json");
  EXPECT_TRUE(cfg.model.empty());
  EXPECT_TRUE(cfg.api_key.empty());
}

TEST(ConfigTest, LoadFromFileParsesMinimalConfig) {
  EnvGuard guard1("CLAW_API_KEY");
  EnvGuard guard2("CLAW_MODEL");
  EnvGuard guard3("CLAW_BASE_URL");

  std::string temp_path =
      (std::filesystem::temp_directory_path() / "quantclaw_test_config.json").string();
  {
    std::ofstream f(temp_path);
    f << R"({"llm":{"apiKey":"test-key","baseUrl":"https://example.com","model":"gpt-test"}})";
  }

  auto cfg = Config::LoadFromFile(temp_path);
  EXPECT_EQ(cfg.api_key, "test-key");
  EXPECT_EQ(cfg.base_url, "https://example.com");
  EXPECT_EQ(cfg.model, "gpt-test");

  std::filesystem::remove(temp_path);
}

}  // namespace quantclaw
