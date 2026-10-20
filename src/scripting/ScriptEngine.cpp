#include "TinyBrowser/ScriptEngine.h"
#include "TinyBrowser/V8DOMBridge.h"

#include <v8.h>
#include <v8-context.h>
#include <v8-template.h>
#include <v8-function-callback.h>
#include <v8-local-handle.h>
#include <v8-isolate.h>
#include <v8-script.h>
#include <v8-exception.h>
#include <v8-object.h>
#include <v8-primitive.h>

namespace TinyBrowser {

ScriptEngine* ScriptEngine::s_instance = nullptr;

ScriptEngine::ScriptEngine() {
  if (s_instance == nullptr) {
    s_instance = this;
  }
}

ScriptEngine::~ScriptEngine() {
  shutdown();
  if (s_instance == this) {
    s_instance = nullptr;
  }
}

void ScriptEngine::documentWrite(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (s_instance == nullptr) return;
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (int i = 0; i < args.Length(); i++) {
    v8::Local<v8::Value> arg = args[i];
    v8::Local<v8::String> str;
    if (arg->IsString()) {
      str = arg.As<v8::String>();
    } else {
      if (!arg->ToString(context).ToLocal(&str)) continue;
    }
    v8::String::Utf8Value utf8(isolate, str);
    if (*utf8) {
      s_instance->m_output += *utf8;
    }
  }
}

bool ScriptEngine::initialize() {
  shutdown();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  m_isolate = v8::Isolate::New(create_params);
  if (!m_isolate) return false;
  v8::HandleScope handle_scope(m_isolate);
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(m_isolate);
  v8::Local<v8::ObjectTemplate> document = v8::ObjectTemplate::New(m_isolate);
  document->Set(m_isolate, "write", v8::FunctionTemplate::New(m_isolate, &ScriptEngine::documentWrite));
  global->Set(m_isolate, "document", document);
  V8DOMBridge::Initialize(m_isolate, global);
  v8::Local<v8::Context> context = v8::Context::New(m_isolate, nullptr, global);
  m_context = new v8::Global<v8::Context>(m_isolate, context);
  return true;
}

void ScriptEngine::shutdown() {
  if (m_context) {
    delete m_context;
    m_context = nullptr;
  }
  if (m_isolate) {
    m_isolate->Dispose();
    m_isolate = nullptr;
  }
}

bool ScriptEngine::executeScript(const std::string& source) {
  if (!m_isolate || !m_context) return false;
  v8::HandleScope handle_scope(m_isolate);
  v8::Local<v8::Context> context = m_context->Get(m_isolate);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::String> script_source = v8::String::NewFromUtf8(m_isolate, source.c_str()).ToLocalChecked();
  v8::TryCatch try_catch(m_isolate);
  v8::Local<v8::Script> script;
  if (!v8::Script::Compile(context, script_source).ToLocal(&script)) {
    if (try_catch.HasCaught()) {
      v8::Local<v8::Message> message = try_catch.Message();
      if (!message.IsEmpty()) {
        v8::String::Utf8Value error(m_isolate, message->Get());
        if (*error) m_output += *error;
      }
    }
    return false;
  }
  v8::Local<v8::Value> result;
  if (!script->Run(context).ToLocal(&result)) {
    if (try_catch.HasCaught()) {
      v8::Local<v8::Message> message = try_catch.Message();
      if (!message.IsEmpty()) {
        v8::String::Utf8Value error(m_isolate, message->Get());
        if (*error) m_output += *error;
      }
    }
    return false;
  }
  return true;
}

} // namespace TinyBrowser
