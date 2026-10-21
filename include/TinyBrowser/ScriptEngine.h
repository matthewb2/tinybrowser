#pragma once

#include <string>
#include <memory>
#include <v8.h>
#include <v8-function-callback.h>

namespace v8 {
class Isolate;
template<class T> class Global;
class Context;
}

namespace TinyBrowser {

class DomNode;

class ScriptEngine {
public:
  ScriptEngine();
  ~ScriptEngine();

  bool initialize();
  void shutdown();
  bool executeScript(const std::string& source);

  // Publishes the parsed document as the "document" global of the script
  // context.  Has to be called after initialize().
  void setDocument(DomNode* root);
  
  static void documentWrite(const v8::FunctionCallbackInfo<v8::Value>& args);
  
  
  std::string getOutput() const { return m_output; }
  void clearOutput() { m_output.clear(); }

private:
  
  static ScriptEngine* s_instance;
  
  v8::Isolate* m_isolate = nullptr;
  v8::Global<v8::Context>* m_context = nullptr;
  std::string m_output;
};
} // namespace TinyBrowser
