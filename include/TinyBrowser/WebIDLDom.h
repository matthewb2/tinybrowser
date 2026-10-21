#pragma once

#include <v8.h>

namespace TinyBrowser {

class DomNode;

// Host side implementation of the interfaces described by the WebIDL
// definitions.
//
// The generated bindings (WebIDLBindings.h) provide the JavaScript view of the
// DOM: prototypes, attribute accessors and operations.  This class implements
// the `native::` interfaces of that header on top of the DomNode tree that is
// produced by the HTML parser.
class WebIDLDom {
public:
  // Installs every generated interface constructor on the global template.
  // Has to be called before the context is created.
  static void Install(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> global);

  // Publishes the parsed document as the "document" global of the current
  // context.  Passing nullptr installs an empty document.
  static void SetDocument(v8::Isolate* isolate, DomNode* root);

  // The document that is currently bound to the script context.
  static DomNode* GetDocument();

  // Releases every DOM object that was created for the script context.  Has to
  // be called while the isolate is still alive because the event listeners own
  // persistent V8 handles.
  static void Reset();
};

}  // namespace TinyBrowser
