#include "TinyBrowser/ScriptEngine.h"
#include "TinyBrowser/WebIDLDom.h"

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

#include <iostream>

namespace TinyBrowser {

ScriptEngine::ScriptEngine() = default;

ScriptEngine::~ScriptEngine() {
  shutdown();
}

bool ScriptEngine::initialize() {
  shutdown();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  m_isolate = v8::Isolate::New(create_params);
  if (!m_isolate) return false;

  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);

  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(m_isolate);

  // WebIDL로 생성된 DOM 인터페이스(ctor)를 글로벌 템플릿에 설치한다.
  WebIDLDom::Install(m_isolate, global);

  v8::Local<v8::Context> context = v8::Context::New(m_isolate, nullptr, global);

  m_context = new v8::Global<v8::Context>(m_isolate, context);
  return true;
}

void ScriptEngine::setDocument(DomNode* root) {
  if (!m_isolate || !m_context) return;
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  v8::Local<v8::Context> context = m_context->Get(m_isolate);
  v8::Context::Scope context_scope(context);
  WebIDLDom::SetDocument(m_isolate, root);
}

void ScriptEngine::shutdown() {
  if (m_isolate) {
    // The DOM objects own persistent V8 handles and have to go first.
    WebIDLDom::Reset();
  }
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
  // ★ 핵심 수정: Isolate Scope 진입
  v8::Isolate::Scope isolate_scope(m_isolate);
  
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
