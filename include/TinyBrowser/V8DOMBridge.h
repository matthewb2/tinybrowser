#pragma once

#include <v8.h>
#include <gumbo.h>
#include <string>

namespace TinyBrowser {

class V8DOMBridge {
public:
    static void Initialize(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> global_template);

private:
    static v8::Local<v8::FunctionTemplate> CreateDocumentTemplate(v8::Isolate* isolate);
    static v8::Local<v8::FunctionTemplate> CreateElementTemplate(v8::Isolate* isolate);

// Document Getters / Setters (콜백 시그니처를 info만 받도록 간소화)
    static void GetDocumentElement(const v8::PropertyCallbackInfo<v8::Value>& info);
    static void GetBody(const v8::PropertyCallbackInfo<v8::Value>& info);
    static void GetHead(const v8::PropertyCallbackInfo<v8::Value>& info);
    static void GetTitle(const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetTitle(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
    
    // Document Methods
    static void GetElementById(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void CreateElement(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void QuerySelector(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void GetElementsByTagName(const v8::FunctionCallbackInfo<v8::Value>& args);

    // Helpers
    static v8::Local<v8::Value> WrapNode(v8::Isolate* isolate, GumboNode* node);
};

} // namespace TinyBrowser