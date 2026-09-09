#pragma once
#include <functional>
#include <string>


namespace quantclaw::cli {

struct Command {
  std::string name;
  std::string description;
  std::vector<std::string> aliases;
  std::function<int(int argc, char** argv)> handler;
};

class CliManager {
public:
  void AddCommand(Command cmd);

  int Run(int argc, char** argv);

  void PrintHelp(const std::string& program_name) const;

private:
  std::vector<Command> commands_;

  const Command* FindCommand(const std::string& name) const;
};

}
