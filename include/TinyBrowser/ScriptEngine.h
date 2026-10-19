#pragma once

#include <string>

namespace TinyBrowser {

class ScriptEngine {
public:
  ScriptEngine() = default;
  ~ScriptEngine() = default;

  bool initialize();
  void shutdown();
  bool executeScript(const std::string& source);
};

} // namespace TinyBrowser