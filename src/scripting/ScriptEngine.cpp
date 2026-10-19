#include "TinyBrowser/ScriptEngine.h"

namespace TinyBrowser {

bool ScriptEngine::initialize() {
  return true;
}

void ScriptEngine::shutdown() {
}

bool ScriptEngine::executeScript(const std::string& source) {
  (void)source;
  return true;
}

} // namespace TinyBrowser