//
// Created by xinfang on 2026/9/9.
//

#include "CliManager.h"

#include <iostream>
#include <ostream>

namespace quantclaw::cli {

void CliManager::AddCommand(Command cmd) {
  commands_.push_back(std::move(cmd));
}

const Command* CliManager::FindCommand(const std::string& name) const {
  for (const auto& cmd : commands_) {
    if (cmd.name == name) return &cmd;
    for (const auto& alias : cmd.aliases) {
      if (alias == name) return &cmd;
    }
  }

  return nullptr;
}

int CliManager::Run(int argc, char** argv) {
  std::string program_name = argc > 0 ? argv[0] : "quantclaw";

  if (argc < 2) {
    PrintHelp(program_name);
    return 1;
  }

  std::string cmd_name(argv[1]);

  if (cmd_name == "--gateway" || cmd_name == "--web" || cmd_name == "--clear" || cmd_name == "clear") {
    if (const auto* cmd = FindCommand(cmd_name)) return cmd->handler(argc, argv);
  }

  auto* cmd = FindCommand(cmd_name);
  if (!cmd) {
    auto* chat_cmd = FindCommand("chat");
    if (chat_cmd) return chat_cmd->handler(argc, argv);

    std::cerr << "Unknown command: " << cmd_name << std::endl;
    PrintHelp(program_name);
    return 1;
  }

  return cmd->handler(argc, argv);
}

void CliManager::PrintHelp(const std::string& program_name) const {
  std::cerr << "Usage: " << program_name << " <command> [args...]\n";
  std::cerr << "       " << program_name << " <user message>\n\n";
  std::cerr << "Commands:\n";

  for (const auto& cmd : commands_) {
    std::cerr << " " << cmd.name;
    if (!cmd.aliases.empty()) {
      std::cerr << " (";
      for (size_t i = 0; i < cmd.aliases.size(); ++i) {
        if (i > 0) std::cerr << ", ";
        std::cerr << cmd.aliases[i];
      }

      std::cerr << ")";
    }

    std::cerr << "\n";
    if (!cmd.description.empty()) std::cerr << "   " << cmd.description << "\n";
  }
}





}
