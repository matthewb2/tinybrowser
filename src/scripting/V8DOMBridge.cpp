#include "TinyBrowser/V8DOMBridge.h"
#include <v8-context.h>
#include <v8-template.h>
#include <v8-function-callback.h>
#include <v8-local-handle.h>
#include <v8-isolate.h>
#include <v8-primitive.h>
#include <v8-object.h>
#include <vector>
#include <algorithm>

namespace TinyBrowser {

v8::Local<v8::Value> V8DOMBridge::WrapNode(v8::Isolate* isolate, GumboNode* node) {
    if (!node) return v8::Null(isolate);

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::FunctionTemplate> elemTmpl = CreateElementTemplate(isolate);
    v8::Local<v8::Object> obj = elemTmpl->GetFunction(context).ToLocalChecked()->NewInstance(context).ToLocalChecked();
    
    obj->SetInternalField(0, v8::External::New(isolate, node));
    return obj;
}

void V8DOMBridge::Initialize(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> global_template) {
    v8::HandleScope handle_scope(isolate);

    v8::Local<v8::FunctionTemplate> docTmpl = CreateDocumentTemplate(isolate);
    
    // 수정: V8 Template::Set 오버로드 규격에 맞게 수정 (isolate, const char*, data)
    global_template->Set(isolate, "document", docTmpl);
}

v8::Local<v8::FunctionTemplate> V8DOMBridge::CreateDocumentTemplate(v8::Isolate* isolate) {
    v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate);
    tpl->SetClassName(v8::String::NewFromUtf8(isolate, "Document").ToLocalChecked());
    
    v8::Local<v8::ObjectTemplate> instanceTmpl = tpl->InstanceTemplate();
    instanceTmpl->SetInternalFieldCount(1);

    // 수정: v8::Local<v8::Name>으로 인자 타입 변경 (또는 v8::Local<v8::Name>으로 받고 내부에서 처리)
    instanceTmpl->SetNativeDataProperty(
        v8::String::NewFromUtf8(isolate, "documentElement").ToLocalChecked(), 
        [](v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) { GetDocumentElement(info); }
    );
    instanceTmpl->SetNativeDataProperty(
        v8::String::NewFromUtf8(isolate, "body").ToLocalChecked(), 
        [](v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) { GetBody(info); }
    );
    instanceTmpl->SetNativeDataProperty(
        v8::String::NewFromUtf8(isolate, "head").ToLocalChecked(), 
        [](v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) { GetHead(info); }
    );
    instanceTmpl->SetNativeDataProperty(
        v8::String::NewFromUtf8(isolate, "title").ToLocalChecked(), 
        [](v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) { GetTitle(info); },
        [](v8::Local<v8::Name>, v8::Local<v8::Value> val, const v8::PropertyCallbackInfo<void>& info) { SetTitle(val, info); }
    );

    instanceTmpl->Set(isolate, "getElementById", v8::FunctionTemplate::New(isolate, GetElementById));
    instanceTmpl->Set(isolate, "createElement", v8::FunctionTemplate::New(isolate, CreateElement));
    instanceTmpl->Set(isolate, "querySelector", v8::FunctionTemplate::New(isolate, QuerySelector));
    instanceTmpl->Set(isolate, "getElementsByTagName", v8::FunctionTemplate::New(isolate, GetElementsByTagName));

    return tpl;
}

v8::Local<v8::FunctionTemplate> V8DOMBridge::CreateElementTemplate(v8::Isolate* isolate) {
    v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate);
    tpl->SetClassName(v8::String::NewFromUtf8(isolate, "Element").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    return tpl;
}

void V8DOMBridge::GetDocumentElement(const v8::PropertyCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(v8::Null(info.GetIsolate()));
}

void V8DOMBridge::GetBody(const v8::PropertyCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(v8::Null(info.GetIsolate()));
}

void V8DOMBridge::GetHead(const v8::PropertyCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(v8::Null(info.GetIsolate()));
}

void V8DOMBridge::GetTitle(const v8::PropertyCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(v8::String::NewFromUtf8(info.GetIsolate(), "").ToLocalChecked());
}

void V8DOMBridge::SetTitle(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info) {
}

void V8DOMBridge::GetElementById(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(v8::Null(args.GetIsolate()));
}

void V8DOMBridge::CreateElement(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(v8::Null(args.GetIsolate()));
}

void V8DOMBridge::QuerySelector(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(v8::Null(args.GetIsolate()));
}

void V8DOMBridge::GetElementsByTagName(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(v8::Array::New(args.GetIsolate(), 0));
}

} // namespace TinyBrowser