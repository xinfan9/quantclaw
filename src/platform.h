#pragma once
#include <string>
namespace quantclaw :: platform {
inline std::string  home_directory() {
  const char* home = std::getenv("HOME");
  if (!home) {
    throw std::runtime_error("HOME env varialbe not set");
  }

  return home;
}
}

