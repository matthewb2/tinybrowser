// Implementation of the WebIDL generated DOM interfaces on top of the DomNode
// tree that is produced by the HTML parser.

#include "TinyBrowser/WebIDLDom.h"

#include "TinyBrowser/HtmlParser.h"
#include "WebIDLBindings.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace TinyBrowser {
namespace {

namespace native = webidl::native;

using webidl::native::CSSStyleDeclaration;
using webidl::native::Comment;
using webidl::native::Document;
using webidl::native::DOMTokenList;
using webidl::native::Element;
using webidl::native::Event;
using webidl::native::EventTarget;
using webidl::native::HTMLElement;
using webidl::native::Node;
using webidl::native::NodeList;
using webidl::native::Text;

// ---------------------------------------------------------------------
// DomNode helpers
// ---------------------------------------------------------------------

std::string toLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string toUpper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return value;
}

std::vector<std::string> splitWhitespace(const std::string& value) {
  std::vector<std::string> tokens;
  std::istringstream stream(value);
  std::string token;
  while (stream >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

std::string trim(const std::string& value) {
  const size_t begin = value.find_first_not_of(" \t\n\r\f\v");
  if (begin == std::string::npos) return "";
  const size_t end = value.find_last_not_of(" \t\n\r\f\v");
  return value.substr(begin, end - begin + 1);
}

bool isElementNode(const DomNode* node) {
  return node != nullptr &&
         (node->type == NodeType::Element || node->type == NodeType::Template);
}

bool isCharacterNode(const DomNode* node) {
  return node != nullptr &&
         (node->type == NodeType::Text || node->type == NodeType::Whitespace ||
          node->type == NodeType::CData || node->type == NodeType::Comment);
}

uint16_t nodeTypeOf(const DomNode* node) {
  if (node == nullptr) return 0;
  switch (node->type) {
    case NodeType::Element:
    case NodeType::Template:
      return 1;
    case NodeType::Text:
    case NodeType::Whitespace:
      return 3;
    case NodeType::CData:
      return 4;
    case NodeType::Comment:
      return 8;
    case NodeType::Document:
      return 9;
  }
  return 1;
}

std::string nodeNameOf(const DomNode* node) {
  if (node == nullptr) return "";
  switch (node->type) {
    case NodeType::Element:
    case NodeType::Template:
      return toUpper(node->tag);
    case NodeType::Text:
    case NodeType::Whitespace:
      return "#text";
    case NodeType::CData:
      return "#cdata-section";
    case NodeType::Comment:
      return "#comment";
    case NodeType::Document:
      return "#document";
  }
  return "";
}

std::optional<std::string> nodeValueOf(const DomNode* node) {
  if (isCharacterNode(node)) return node->text;
  return std::nullopt;
}

std::optional<std::string> textContentOf(const DomNode* node) {
  if (node == nullptr) return std::nullopt;
  if (isCharacterNode(node)) return node->text;
  return node->innerText();
}

const std::string* attributeOf(const DomNode* node, const std::string& name) {
  if (node == nullptr) return nullptr;
  return node->getAttribute(name);
}

void setAttributeOn(DomNode* node, const std::string& name, const std::string& value) {
  if (node == nullptr) return;
  for (DomAttribute& attribute : node->attributes) {
    if (attribute.name == name) {
      attribute.value = value;
      return;
    }
  }
  DomAttribute attribute;
  attribute.name = name;
  attribute.value = value;
  node->attributes.push_back(attribute);
}

void removeAttributeOn(DomNode* node, const std::string& name) {
  if (node == nullptr) return;
  node->attributes.erase(
      std::remove_if(node->attributes.begin(), node->attributes.end(),
                     [&name](const DomAttribute& attribute) {
                       return attribute.name == name;
                     }),
      node->attributes.end());
}

void escapeText(std::string& out, const std::string& text) {
  for (char c : text) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      default:
        out += c;
        break;
    }
  }
}

void escapeAttribute(std::string& out, const std::string& text) {
  for (char c : text) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '"':
        out += "&quot;";
        break;
      default:
        out += c;
        break;
    }
  }
}

void serializeNode(const DomNode* node, std::string& out) {
  if (node == nullptr) return;
  switch (node->type) {
    case NodeType::Element:
    case NodeType::Template: {
      out += "<";
      out += node->tag;
      for (const DomAttribute& attribute : node->attributes) {
        out += " ";
        out += attribute.name;
        out += "=\"";
        escapeAttribute(out, attribute.value);
        out += "\"";
      }
      out += ">";
      for (const auto& child : node->children) {
        serializeNode(child.get(), out);
      }
      out += "</";
      out += node->tag;
      out += ">";
      break;
    }
    case NodeType::Text:
    case NodeType::Whitespace:
      escapeText(out, node->text);
      break;
    case NodeType::CData:
      out += "<![CDATA[";
      out += node->text;
      out += "]]>";
      break;
    case NodeType::Comment:
      out += "<!--";
      out += node->text;
      out += "-->";
      break;
    case NodeType::Document:
      for (const auto& child : node->children) {
        serializeNode(child.get(), out);
      }
      break;
  }
}

std::string innerHTMLOf(const DomNode* node) {
  std::string out;
  if (node == nullptr) return out;
  for (const auto& child : node->children) {
    serializeNode(child.get(), out);
  }
  return out;
}

std::string outerHTMLOf(const DomNode* node) {
  std::string out;
  serializeNode(node, out);
  return out;
}

// ---------------------------------------------------------------------
// Selector support
// ---------------------------------------------------------------------

struct SimpleSelector {
  std::string tag;
  std::vector<std::string> classes;
  std::string id;
};

SimpleSelector parseSimpleSelector(const std::string& text) {
  SimpleSelector selector;
  size_t index = 0;
  while (index < text.size()) {
    const char c = text[index];
    if (c == '#' || c == '.') {
      ++index;
      const size_t start = index;
      while (index < text.size() && text[index] != '.' && text[index] != '#') {
        ++index;
      }
      const std::string value = text.substr(start, index - start);
      if (c == '#') {
        selector.id = value;
      } else {
        selector.classes.push_back(value);
      }
      continue;
    }
    const size_t start = index;
    while (index < text.size() && text[index] != '.' && text[index] != '#') {
      ++index;
    }
    selector.tag = toLower(text.substr(start, index - start));
  }
  return selector;
}

bool matchesSelector(const DomNode* node, const SimpleSelector& selector) {
  if (!isElementNode(node)) return false;
  if (!selector.tag.empty() && toLower(node->tag) != selector.tag) return false;
  if (!selector.id.empty()) {
    const std::string* id = node->getAttribute("id");
    if (id == nullptr || *id != selector.id) return false;
  }
  if (!selector.classes.empty()) {
    const std::string* value = node->getAttribute("class");
    const std::vector<std::string> tokens =
        value != nullptr ? splitWhitespace(*value) : std::vector<std::string>();
    for (const std::string& required : selector.classes) {
      if (std::find(tokens.begin(), tokens.end(), required) == tokens.end()) {
        return false;
      }
    }
  }
  return true;
}

std::vector<SimpleSelector> parseSelectorList(const std::string& selectors) {
  std::vector<SimpleSelector> result;
  std::istringstream stream(selectors);
  std::string part;
  while (std::getline(stream, part, ',')) {
    const std::string trimmed = part;
    if (trimmed.empty()) continue;
    result.push_back(parseSimpleSelector(trimmed));
  }
  return result;
}

void collectDescendants(DomNode* node, std::vector<DomNode*>& out) {
  if (node == nullptr) return;
  for (const auto& child : node->children) {
    out.push_back(child.get());
    collectDescendants(child.get(), out);
  }
}

void collectBySelector(DomNode* root, const std::vector<SimpleSelector>& selectors,
                       std::vector<DomNode*>& out) {
  std::vector<DomNode*> candidates;
  collectDescendants(root, candidates);
  for (DomNode* candidate : candidates) {
    for (const SimpleSelector& selector : selectors) {
      if (matchesSelector(candidate, selector)) {
        out.push_back(candidate);
        break;
      }
    }
  }
}

void collectByTagName(DomNode* root, const std::string& tag,
                      std::vector<DomNode*>& out) {
  std::vector<DomNode*> candidates;
  collectDescendants(root, candidates);
  const std::string wanted = toLower(tag);
  for (DomNode* candidate : candidates) {
    if (!isElementNode(candidate)) continue;
    if (wanted == "*" || toLower(candidate->tag) == wanted) {
      out.push_back(candidate);
    }
  }
}

void collectByClassName(DomNode* root, const std::string& classNames,
                        std::vector<DomNode*>& out) {
  const std::vector<std::string> required = splitWhitespace(classNames);
  if (required.empty()) return;
  std::vector<DomNode*> candidates;
  collectDescendants(root, candidates);
  for (DomNode* candidate : candidates) {
    if (!isElementNode(candidate)) continue;
    const std::string* value = candidate->getAttribute("class");
    const std::vector<std::string> tokens =
        value != nullptr ? splitWhitespace(*value) : std::vector<std::string>();
    bool matches = true;
    for (const std::string& token : required) {
      if (std::find(tokens.begin(), tokens.end(), token) == tokens.end()) {
        matches = false;
        break;
      }
    }
    if (matches) out.push_back(candidate);
  }
}

// ---------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------

Node* wrapNode(DomNode* node);
NodeList* makeNodeList(const std::vector<DomNode*>& nodes);
NodeList* makeChildNodeList(DomNode* node);
DOMTokenList* makeTokenList(DomNode* element);
CSSStyleDeclaration* makeStyleDeclaration(DomNode* element);
Event* makeEvent(const std::string& type, EventTarget* target);
void replaceChildren(DomNode* element, const std::string& html);
void setTextContentOn(DomNode* node, const std::optional<std::string>& value);
std::unique_ptr<DomNode> cloneDomNode(DomNode* node, bool deep);
std::unique_ptr<DomNode> detachNode(DomNode* node);
void attachNode(DomNode* parent, std::unique_ptr<DomNode> node, DomNode* reference);
void detachChild(DomNode* parent, DomNode* child);
std::unique_ptr<DomNode> createElementNode(const std::string& tag);
std::unique_ptr<DomNode> createCharacterNode(const std::string& data, NodeType type);
void ownNode(std::unique_ptr<DomNode> node);
std::unique_ptr<DomNode> releaseOwnedNode(DomNode* node);

// ---------------------------------------------------------------------
// Shared state of every DOM object
// ---------------------------------------------------------------------

class DomBacking {
public:
  explicit DomBacking(DomNode* node) : m_node(node) {}
  virtual ~DomBacking() = default;

  DomNode* node() const { return m_node; }

  void addEventListener(const std::string& type, v8::Local<v8::Object> listener);
  void removeEventListener(const std::string& type, v8::Local<v8::Object> listener);
  bool dispatchEvent(Event* event);

private:
  struct Listener {
    std::string type;
    v8::Global<v8::Object> callback;
  };

  DomNode* m_node;
  std::vector<Listener> m_listeners;
};

template <typename T>
DomBacking* backingOf(T* object) {
  return dynamic_cast<DomBacking*>(object);
}

template <typename T>
DomNode* backingNode(const T* object) {
  const DomBacking* backing = dynamic_cast<const DomBacking*>(object);
  return backing != nullptr ? backing->node() : nullptr;
}

DomNode* domNodeOf(Node* node) {
  DomBacking* backing = dynamic_cast<DomBacking*>(node);
  return backing != nullptr ? backing->node() : nullptr;
}

bool isHtmlTag(const std::string& tag) {
  static const char* kTags[] = {
      "a", "abbr", "article", "aside", "b", "body", "br", "button", "canvas",
      "caption", "code", "dd", "div", "dl", "dt", "em", "footer", "form",
      "h1", "h2", "h3", "h4", "h5", "h6", "head", "header", "html", "i",
      "img", "input", "label", "li", "main", "nav", "ol", "p", "pre",
      "section", "span", "strong", "table", "tbody", "td", "textarea", "tfoot",
      "th", "thead", "tr", "ul"};
  const std::string lowered = toLower(tag);
  for (const char* candidate : kTags) {
    if (lowered == candidate) return true;
  }
  return false;
}

// ---------------------------------------------------------------------
// Interface implementations
//
// The level templates implement the members of one WebIDL interface.  Every
// concrete object derives from exactly one native interface chain, so the
// generated bindings can pick the matching prototype with a dynamic_cast.
// ---------------------------------------------------------------------

template <typename Base>
struct NodeLevel : Base {
  uint16_t nodeType() const override { return nodeTypeOf(backingNode(this)); }

  std::string nodeName() const override { return nodeNameOf(backingNode(this)); }

  std::optional<std::string> nodeValue() const override {
    return nodeValueOf(backingNode(this));
  }

  void setNodeValue(std::optional<std::string> value) override {
    DomNode* node = backingNode(this);
    if (node != nullptr && value.has_value() && isCharacterNode(node)) {
      node->text = *value;
    }
  }

  std::optional<std::string> textContent() const override {
    return textContentOf(backingNode(this));
  }

  void setTextContent(std::optional<std::string> value) override {
    setTextContentOn(backingNode(this), value);
  }

  std::optional<Node*> parentNode() const override {
    DomNode* node = backingNode(this);
    if (node == nullptr || node->parent == nullptr) return std::nullopt;
    return std::optional<Node*>(wrapNode(node->parent));
  }

  std::optional<Node*> firstChild() const override {
    DomNode* node = backingNode(this);
    if (node == nullptr || node->children.empty()) return std::nullopt;
    return std::optional<Node*>(wrapNode(node->children.front().get()));
  }

  std::optional<Node*> lastChild() const override {
    DomNode* node = backingNode(this);
    if (node == nullptr || node->children.empty()) return std::nullopt;
    return std::optional<Node*>(wrapNode(node->children.back().get()));
  }

  std::optional<Node*> nextSibling() const override {
    DomNode* node = backingNode(this);
    if (node == nullptr || node->parent == nullptr) return std::nullopt;
    const auto& siblings = node->parent->children;
    for (size_t i = 0; i + 1 < siblings.size(); ++i) {
      if (siblings[i].get() == node) {
        return std::optional<Node*>(wrapNode(siblings[i + 1].get()));
      }
    }
    return std::nullopt;
  }

  std::optional<Node*> previousSibling() const override {
    DomNode* node = backingNode(this);
    if (node == nullptr || node->parent == nullptr) return std::nullopt;
    const auto& siblings = node->parent->children;
    for (size_t i = 1; i < siblings.size(); ++i) {
      if (siblings[i].get() == node) {
        return std::optional<Node*>(wrapNode(siblings[i - 1].get()));
      }
    }
    return std::nullopt;
  }

  bool isConnected() const override {
    DomNode* node = backingNode(this);
    while (node != nullptr) {
      if (node->type == NodeType::Document) return true;
      node = node->parent;
    }
    return false;
  }

  NodeList* childNodes() const override { return makeChildNodeList(backingNode(this)); }

  Node* appendChild(Node* child) override {
    DomNode* parent = backingNode(this);
    DomNode* childNode = domNodeOf(child);
    if (parent == nullptr || childNode == nullptr) return child;
    std::unique_ptr<DomNode> owned = detachNode(childNode);
    if (owned == nullptr) return child;
    owned->parent = parent;
    parent->children.push_back(std::move(owned));
    return child;
  }

  Node* removeChild(Node* child) override {
    DomNode* parent = backingNode(this);
    DomNode* childNode = domNodeOf(child);
    if (parent == nullptr || childNode == nullptr) return nullptr;
    detachChild(parent, childNode);
    return child;
  }

  Node* insertBefore(Node* newChild, std::optional<Node*> refChild) override {
    DomNode* parent = backingNode(this);
    DomNode* childNode = domNodeOf(newChild);
    if (parent == nullptr || childNode == nullptr) return newChild;
    DomNode* reference = nullptr;
    if (refChild.has_value() && refChild.value() != nullptr) {
      reference = domNodeOf(refChild.value());
    }
    std::unique_ptr<DomNode> owned = detachNode(childNode);
    if (owned == nullptr) return newChild;
    attachNode(parent, std::move(owned), reference);
    return newChild;
  }

  Node* cloneNode(bool deep) override {
    std::unique_ptr<DomNode> copy = cloneDomNode(backingNode(this), deep);
    if (copy == nullptr) return nullptr;
    DomNode* raw = copy.get();
    ownNode(std::move(copy));
    return wrapNode(raw);
  }

  bool contains(std::optional<Node*> other) override {
    DomNode* node = backingNode(this);
    if (node == nullptr || !other.has_value() || other.value() == nullptr) {
      return false;
    }
    DomNode* candidate = domNodeOf(other.value());
    while (candidate != nullptr) {
      if (candidate == node) return true;
      candidate = candidate->parent;
    }
    return false;
  }

  bool hasChildNodes() override {
    DomNode* node = backingNode(this);
    return node != nullptr && !node->children.empty();
  }

  void addEventListener(std::string type, v8::Local<v8::Object> listener) override {
    backingOf(this)->addEventListener(type, listener);
  }

  void removeEventListener(std::string type, v8::Local<v8::Object> listener) override {
    backingOf(this)->removeEventListener(type, listener);
  }

  bool dispatchEvent(Event* event) override {
    return backingOf(this)->dispatchEvent(event);
  }
};

template <typename Base>
struct ElementLevel : NodeLevel<Base> {
  std::string tagName() const override { return nodeNameOf(backingNode(this)); }

  std::string localName() const override {
    DomNode* node = backingNode(this);
    return node != nullptr ? node->tag : "";
  }

  std::string id() const override {
    const std::string* value = attributeOf(backingNode(this), "id");
    return value != nullptr ? *value : "";
  }

  void setId(std::string value) override {
    setAttributeOn(backingNode(this), "id", value);
  }

  std::string className() const override {
    const std::string* value = attributeOf(backingNode(this), "class");
    return value != nullptr ? *value : "";
  }

  void setClassName(std::string value) override {
    setAttributeOn(backingNode(this), "class", value);
  }

  DOMTokenList* classList() const override {
    return makeTokenList(backingNode(this));
  }

  std::string innerHTML() const override { return innerHTMLOf(backingNode(this)); }

  void setInnerHTML(std::string value) override {
    replaceChildren(backingNode(this), value);
  }

  std::string outerHTML() const override { return outerHTMLOf(backingNode(this)); }

  void setOuterHTML(std::string value) override {
    DomNode* node = backingNode(this);
    if (node == nullptr) return;
    replaceChildren(node, value);
  }

  std::optional<std::string> getAttribute(std::string qualifiedName) override {
    const std::string* value = attributeOf(backingNode(this), qualifiedName);
    if (value == nullptr) return std::nullopt;
    return *value;
  }

  void setAttribute(std::string qualifiedName, std::string value) override {
    setAttributeOn(backingNode(this), qualifiedName, value);
  }

  void removeAttribute(std::string qualifiedName) override {
    removeAttributeOn(backingNode(this), qualifiedName);
  }

  bool hasAttribute(std::string qualifiedName) override {
    return attributeOf(backingNode(this), qualifiedName) != nullptr;
  }

  std::optional<Element*> querySelector(std::string selectors) override {
    DomNode* node = backingNode(this);
    if (node == nullptr) return std::nullopt;
    std::vector<DomNode*> found;
    collectBySelector(node, parseSelectorList(selectors), found);
    if (found.empty()) return std::nullopt;
    return std::optional<Element*>(dynamic_cast<Element*>(wrapNode(found.front())));
  }

  NodeList* querySelectorAll(std::string selectors) override {
    DomNode* node = backingNode(this);
    std::vector<DomNode*> found;
    if (node != nullptr) {
      collectBySelector(node, parseSelectorList(selectors), found);
    }
    return makeNodeList(found);
  }

  NodeList* getElementsByTagName(std::string localName) override {
    DomNode* node = backingNode(this);
    std::vector<DomNode*> found;
    if (node != nullptr) {
      collectByTagName(node, localName, found);
    }
    return makeNodeList(found);
  }

  NodeList* getElementsByClassName(std::string classNames) override {
    DomNode* node = backingNode(this);
    std::vector<DomNode*> found;
    if (node != nullptr) {
      collectByClassName(node, classNames, found);
    }
    return makeNodeList(found);
  }

  void append(Node* node) override {
    DomNode* parent = backingNode(this);
    DomNode* childNode = domNodeOf(node);
    if (parent == nullptr || childNode == nullptr) return;
    std::unique_ptr<DomNode> owned = detachNode(childNode);
    if (owned == nullptr) return;
    owned->parent = parent;
    parent->children.push_back(std::move(owned));
  }

  void remove() override {
    DomNode* node = backingNode(this);
    if (node == nullptr || node->parent == nullptr) return;
    detachChild(node->parent, node);
  }

  CSSStyleDeclaration* style() const override {
    return makeStyleDeclaration(backingNode(this));
  }
};

template <typename Base>
struct CharacterDataLevel : NodeLevel<Base> {
  std::string data() const override {
    DomNode* node = backingNode(this);
    return node != nullptr ? node->text : "";
  }

  void setData(std::string value) override {
    DomNode* node = backingNode(this);
    if (node != nullptr) node->text = value;
  }

  uint32_t length() const override {
    DomNode* node = backingNode(this);
    return node != nullptr ? static_cast<uint32_t>(node->text.size()) : 0u;
  }

  void appendData(std::string data) override {
    DomNode* node = backingNode(this);
    if (node != nullptr) node->text += data;
  }

  std::string substringData(uint32_t offset, uint32_t count) override {
    DomNode* node = backingNode(this);
    if (node == nullptr) return "";
    if (offset >= node->text.size()) return "";
    return node->text.substr(offset, count);
  }
};

template <typename Base>
struct TextLevel : CharacterDataLevel<Base> {
  std::string wholeText() const override {
    DomNode* node = backingNode(this);
    return node != nullptr ? node->text : "";
  }
};

template <typename Base>
struct DocumentLevel : NodeLevel<Base> {
  Element* documentElement() const override {
    DomNode* node = backingNode(this);
    if (node == nullptr) return nullptr;
    for (const auto& child : node->children) {
      if (!isElementNode(child.get())) continue;
      return dynamic_cast<Element*>(wrapNode(child.get()));
    }
    return nullptr;
  }

  std::optional<Element*> body() const override {
    DomNode* node = backingNode(this);
    if (node == nullptr) return std::nullopt;
    std::vector<DomNode*> bodies;
    node->getElementsByTagName("body", bodies);
    if (bodies.empty()) return std::nullopt;
    return std::optional<Element*>(dynamic_cast<Element*>(wrapNode(bodies.front())));
  }

  std::optional<Element*> head() const override {
    DomNode* node = backingNode(this);
    if (node == nullptr) return std::nullopt;
    std::vector<DomNode*> heads;
    node->getElementsByTagName("head", heads);
    if (heads.empty()) return std::nullopt;
    return std::optional<Element*>(dynamic_cast<Element*>(wrapNode(heads.front())));
  }

  std::string title() const override {
    DomNode* node = backingNode(this);
    if (node == nullptr) return "";
    std::vector<DomNode*> titles;
    node->getElementsByTagName("title", titles);
    if (titles.empty()) return "";
    return titles.front()->innerText();
  }

  void setTitle(std::string value) override {
    DomNode* node = backingNode(this);
    if (node == nullptr) return;
    std::vector<DomNode*> titles;
    node->getElementsByTagName("title", titles);
    if (!titles.empty()) {
      titles.front()->text = value;
      return;
    }
    // Create a <title> element inside <head> when the document has none.
    DomNode* head = nullptr;
    for (const auto& child : node->children) {
      if (isElementNode(child.get()) && toLower(child->tag) == "head") {
        head = child.get();
        break;
      }
    }
    if (head == nullptr) return;
    std::unique_ptr<DomNode> title = createElementNode("title");
    std::unique_ptr<DomNode> text = createCharacterNode(value, NodeType::Text);
    text->parent = title.get();
    title->children.push_back(std::move(text));
    title->parent = head;
    head->children.push_back(std::move(title));
  }

  std::optional<Element*> getElementById(std::string elementId) override {
    DomNode* node = backingNode(this);
    if (node == nullptr) return std::nullopt;
    std::vector<DomNode*> candidates;
    collectDescendants(node, candidates);
    for (DomNode* candidate : candidates) {
      if (!isElementNode(candidate)) continue;
      const std::string* id = candidate->getAttribute("id");
      if (id != nullptr && *id == elementId) {
        return std::optional<Element*>(dynamic_cast<Element*>(wrapNode(candidate)));
      }
    }
    return std::nullopt;
  }

  Element* createElement(std::string localName) override {
    std::unique_ptr<DomNode> node = createElementNode(localName);
    DomNode* raw = node.get();
    ownNode(std::move(node));
    return dynamic_cast<Element*>(wrapNode(raw));
  }

  Node* createTextNode(std::string data) override {
    std::unique_ptr<DomNode> node = createCharacterNode(data, NodeType::Text);
    DomNode* raw = node.get();
    ownNode(std::move(node));
    return wrapNode(raw);
  }

  Comment* createComment(std::string data) override {
    std::unique_ptr<DomNode> node = createCharacterNode(data, NodeType::Comment);
    DomNode* raw = node.get();
    ownNode(std::move(node));
    return dynamic_cast<Comment*>(wrapNode(raw));
  }

  std::optional<Element*> querySelector(std::string selectors) override {
    DomNode* node = backingNode(this);
    if (node == nullptr) return std::nullopt;
    std::vector<DomNode*> found;
    collectBySelector(node, parseSelectorList(selectors), found);
    if (found.empty()) return std::nullopt;
    return std::optional<Element*>(dynamic_cast<Element*>(wrapNode(found.front())));
  }

  NodeList* querySelectorAll(std::string selectors) override {
    DomNode* node = backingNode(this);
    std::vector<DomNode*> found;
    if (node != nullptr) {
      collectBySelector(node, parseSelectorList(selectors), found);
    }
    return makeNodeList(found);
  }

  NodeList* getElementsByTagName(std::string localName) override {
    DomNode* node = backingNode(this);
    std::vector<DomNode*> found;
    if (node != nullptr) {
      collectByTagName(node, localName, found);
    }
    return makeNodeList(found);
  }

  NodeList* getElementsByClassName(std::string classNames) override {
    DomNode* node = backingNode(this);
    std::vector<DomNode*> found;
    if (node != nullptr) {
      collectByClassName(node, classNames, found);
    }
    return makeNodeList(found);
  }

  std::string readyState() const override { return "complete"; }

  std::optional<std::string> cookie() const override { return std::nullopt; }

  void setCookie(std::optional<std::string> value) override { (void)value; }
};

template <typename Base>
struct HtmlLevel : ElementLevel<Base> {
  std::optional<std::string> title() const override {
    const std::string* value = attributeOf(backingNode(this), "title");
    if (value == nullptr) return std::nullopt;
    return *value;
  }

  void setTitle(std::optional<std::string> value) override {
    DomNode* node = backingNode(this);
    if (value.has_value()) {
      setAttributeOn(node, "title", *value);
    } else {
      removeAttributeOn(node, "title");
    }
  }

  std::string lang() const override {
    const std::string* value = attributeOf(backingNode(this), "lang");
    return value != nullptr ? *value : "";
  }

  void setLang(std::string value) override {
    setAttributeOn(backingNode(this), "lang", value);
  }

  std::optional<std::string> innerText() const override {
    return textContentOf(backingNode(this));
  }

  void setInnerText(std::optional<std::string> value) override {
    setTextContentOn(backingNode(this), value);
  }

  void click() override {
    DomBacking* backing = backingOf(this);
    Event* event = makeEvent("click", dynamic_cast<EventTarget*>(backing));
    backing->dispatchEvent(event);
  }

  void focus() override {}
  void blur() override {}

  std::string value() const override {
    const std::string* value = attributeOf(backingNode(this), "value");
    return value != nullptr ? *value : "";
  }

  void setValue(std::string value) override {
    setAttributeOn(backingNode(this), "value", value);
  }
};

// ---------------------------------------------------------------------
// Concrete DOM objects
// ---------------------------------------------------------------------

class DomNodeImpl : public DomBacking, public NodeLevel<native::Node> {
public:
  explicit DomNodeImpl(DomNode* node) : DomBacking(node) {}
};

class DomElementImpl : public DomBacking, public ElementLevel<native::Element> {
public:
  explicit DomElementImpl(DomNode* node) : DomBacking(node) {}
};

class DomTextImpl : public DomBacking, public TextLevel<native::Text> {
public:
  explicit DomTextImpl(DomNode* node) : DomBacking(node) {}
};

class DomCommentImpl : public DomBacking, public CharacterDataLevel<native::Comment> {
public:
  explicit DomCommentImpl(DomNode* node) : DomBacking(node) {}
};

class DomDocumentImpl : public DomBacking, public DocumentLevel<native::Document> {
public:
  explicit DomDocumentImpl(DomNode* node) : DomBacking(node) {}
};

class DomHTMLElementImpl : public DomBacking, public HtmlLevel<native::HTMLElement> {
public:
  explicit DomHTMLElementImpl(DomNode* node) : DomBacking(node) {}
};

class DomInputElementImpl : public DomBacking,
                             public HtmlLevel<native::HTMLInputElement> {
public:
  explicit DomInputElementImpl(DomNode* node) : DomBacking(node) {}
};

class DomTextAreaElementImpl : public DomBacking,
                                public HtmlLevel<native::HTMLTextAreaElement> {
public:
  explicit DomTextAreaElementImpl(DomNode* node) : DomBacking(node) {}
};

// ---------------------------------------------------------------------
// Auxiliary native objects
// ---------------------------------------------------------------------

class NodeListImpl : public NodeList {
public:
  explicit NodeListImpl(std::vector<Node*> nodes) : m_nodes(std::move(nodes)) {}

  uint32_t length() const override {
    return static_cast<uint32_t>(m_nodes.size());
  }

  std::optional<Node*> item(uint32_t index) override {
    if (index >= m_nodes.size()) return std::nullopt;
    return m_nodes[index];
  }

private:
  std::vector<Node*> m_nodes;
};

class DOMTokenListImpl : public DOMTokenList {
public:
  explicit DOMTokenListImpl(DomNode* element) : m_element(element) {}

  uint32_t length() const override {
    return static_cast<uint32_t>(tokens().size());
  }

  std::optional<std::string> item(uint32_t index) override {
    const std::vector<std::string> values = tokens();
    if (index >= values.size()) return std::nullopt;
    return values[index];
  }

  bool contains(std::string token) override {
    const std::vector<std::string> values = tokens();
    return std::find(values.begin(), values.end(), token) != values.end();
  }

  void add(const std::vector<std::string>& newTokens) override {
    std::vector<std::string> values = tokens();
    for (const std::string& token : newTokens) {
      if (token.empty()) continue;
      if (std::find(values.begin(), values.end(), token) == values.end()) {
        values.push_back(token);
      }
    }
    store(values);
  }

  void remove(const std::vector<std::string>& oldTokens) override {
    std::vector<std::string> values = tokens();
    values.erase(std::remove_if(values.begin(), values.end(),
                                 [&oldTokens](const std::string& value) {
                                   return std::find(oldTokens.begin(), oldTokens.end(),
                                                    value) != oldTokens.end();
                                 }),
                 values.end());
    store(values);
  }

  bool toggle(std::string token, bool force) override {
    std::vector<std::string> values = tokens();
    const auto position = std::find(values.begin(), values.end(), token);
    const bool present = position != values.end();
    const bool shouldAdd = force || !present;
    if (shouldAdd && !present) {
      values.push_back(token);
    } else if (!shouldAdd && present) {
      values.erase(position);
    }
    store(values);
    return shouldAdd;
  }

private:
  std::vector<std::string> tokens() const {
    const std::string* value =
        m_element != nullptr ? m_element->getAttribute("class") : nullptr;
    return value != nullptr ? splitWhitespace(*value) : std::vector<std::string>();
  }

  void store(const std::vector<std::string>& values) {
    if (m_element == nullptr) return;
    std::string joined;
    for (size_t i = 0; i < values.size(); ++i) {
      if (i != 0) joined += " ";
      joined += values[i];
    }
    setAttributeOn(m_element, "class", joined);
  }

  DomNode* m_element;
};

class CSSStyleDeclarationImpl : public CSSStyleDeclaration {
public:
  explicit CSSStyleDeclarationImpl(DomNode* element) : m_element(element) {}

  std::string cssText() const override {
    const std::string* value =
        m_element != nullptr ? m_element->getAttribute("style") : nullptr;
    return value != nullptr ? *value : "";
  }

  void setCssText(std::string value) override {
    if (m_element != nullptr) setAttributeOn(m_element, "style", value);
  }

  uint32_t length() const override {
    return static_cast<uint32_t>(declarations().size());
  }

  std::string getPropertyValue(std::string property) override {
    const auto values = declarations();
    for (const auto& entry : values) {
      if (entry.first == toLower(property)) return entry.second.value;
    }
    return "";
  }

  void setProperty(std::string property, std::string value, std::string priority) override {
    if (m_element == nullptr) return;
    auto values = declarations();
    const std::string name = toLower(property);
    bool updated = false;
    for (auto& entry : values) {
      if (entry.first != name) continue;
      entry.second.value = value;
      entry.second.priority = priority;
      updated = true;
      break;
    }
    if (!updated) {
      values.emplace_back(name, Property{value, priority});
    }
    store(values);
  }

  std::string removeProperty(std::string property) override {
    if (m_element == nullptr) return "";
    auto values = declarations();
    const std::string name = toLower(property);
    for (auto it = values.begin(); it != values.end(); ++it) {
      if (it->first != name) continue;
      const std::string removed = it->second.value;
      values.erase(it);
      store(values);
      return removed;
    }
    return "";
  }

  std::string item(uint32_t index) override {
    const auto values = declarations();
    if (index >= values.size()) return "";
    return values[index].first;
  }

private:
  struct Property {
    std::string value;
    std::string priority;
  };

  std::vector<std::pair<std::string, Property>> declarations() const {
    const std::string* value =
        m_element != nullptr ? m_element->getAttribute("style") : nullptr;
    std::vector<std::pair<std::string, Property>> result;
    if (value == nullptr) return result;
    std::istringstream stream(*value);
    std::string declaration;
    while (std::getline(stream, declaration, ';')) {
      const size_t colon = declaration.find(':');
      if (colon == std::string::npos) continue;
      Property property;
      const std::string name = trim(declaration.substr(0, colon));
      const std::string raw = trim(declaration.substr(colon + 1));
      property.value = raw;
      const size_t bang = raw.find('!');
      if (bang != std::string::npos) {
        property.value = trim(raw.substr(0, bang));
        property.priority = trim(raw.substr(bang + 1));
      }
      if (name.empty()) continue;
      result.emplace_back(toLower(name), property);
    }
    return result;
  }

  void store(const std::vector<std::pair<std::string, Property>>& values) {
    if (m_element == nullptr) return;
    std::string text;
    for (const auto& entry : values) {
      if (!text.empty()) text += "; ";
      text += entry.first;
      text += ": ";
      text += entry.second.value;
      if (!entry.second.priority.empty()) {
        text += " !";
        text += entry.second.priority;
      }
    }
    setAttributeOn(m_element, "style", text);
  }

  DomNode* m_element;
};

class EventImpl : public Event {
public:
  EventImpl(std::string type, EventTarget* target)
      : m_type(std::move(type)), m_target(target) {}

  std::string type() const override { return m_type; }

  std::optional<EventTarget*> target() const override {
    if (m_target == nullptr) return std::nullopt;
    return m_target;
  }

  void preventDefault() override { m_defaultPrevented = true; }

  void stopPropagation() override { m_stopped = true; }

  bool defaultPrevented() const { return m_defaultPrevented; }
  bool stopped() const { return m_stopped; }

private:
  std::string m_type;
  EventTarget* m_target;
  bool m_defaultPrevented = false;
  bool m_stopped = false;
};

// ---------------------------------------------------------------------
// Ownership of nodes that are not part of the parsed tree
// ---------------------------------------------------------------------

std::vector<std::unique_ptr<DomNode>>& ownedNodes() {
  static std::vector<std::unique_ptr<DomNode>> nodes;
  return nodes;
}

DomNode*& currentDocument() {
  static DomNode* document = nullptr;
  return document;
}

std::unique_ptr<HtmlParser>& fragmentParser() {
  static std::unique_ptr<HtmlParser> parser;
  return parser;
}

}  // namespace

// ---------------------------------------------------------------------
// Free helpers
// ---------------------------------------------------------------------

namespace {

void ownNode(std::unique_ptr<DomNode> node) {
  if (node != nullptr) ownedNodes().push_back(std::move(node));
}

std::unique_ptr<DomNode> releaseOwnedNode(DomNode* node) {
  for (auto it = ownedNodes().begin(); it != ownedNodes().end(); ++it) {
    if (it->get() == node) {
      std::unique_ptr<DomNode> released = std::move(*it);
      ownedNodes().erase(it);
      return released;
    }
  }
  return nullptr;
}

std::unique_ptr<DomNode> detachNode(DomNode* node) {
  if (node == nullptr) return nullptr;
  if (node->parent != nullptr) {
    auto& siblings = node->parent->children;
    for (auto it = siblings.begin(); it != siblings.end(); ++it) {
      if (it->get() == node) {
        std::unique_ptr<DomNode> detached = std::move(*it);
        siblings.erase(it);
        detached->parent = nullptr;
        return detached;
      }
    }
  }
  return releaseOwnedNode(node);
}

void attachNode(DomNode* parent, std::unique_ptr<DomNode> node, DomNode* reference) {
  if (parent == nullptr || node == nullptr) return;
  node->parent = parent;
  if (reference == nullptr) {
    parent->children.push_back(std::move(node));
    return;
  }
  for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
    if (it->get() == reference) {
      parent->children.insert(it, std::move(node));
      return;
    }
  }
  parent->children.push_back(std::move(node));
}

void detachChild(DomNode* parent, DomNode* child) {
  if (parent == nullptr || child == nullptr) return;
  auto& siblings = parent->children;
  for (auto it = siblings.begin(); it != siblings.end(); ++it) {
    if (it->get() != child) continue;
    std::unique_ptr<DomNode> detached = std::move(*it);
    siblings.erase(it);
    detached->parent = nullptr;
    ownNode(std::move(detached));
    return;
  }
}

std::unique_ptr<DomNode> cloneDomNode(DomNode* node, bool deep) {
  if (node == nullptr) return nullptr;
  auto copy = std::make_unique<DomNode>();
  copy->type = node->type;
  copy->tag = node->tag;
  copy->text = node->text;
  copy->attributes = node->attributes;
  if (deep) {
    for (const auto& child : node->children) {
      std::unique_ptr<DomNode> childCopy = cloneDomNode(child.get(), true);
      if (childCopy == nullptr) continue;
      childCopy->parent = copy.get();
      copy->children.push_back(std::move(childCopy));
    }
  }
  return copy;
}

void setTextContentOn(DomNode* node, const std::optional<std::string>& value) {
  if (node == nullptr) return;
  if (isCharacterNode(node)) {
    node->text = value.value_or("");
    return;
  }
  for (auto& child : node->children) {
    child->parent = nullptr;
  }
  node->children.clear();
  if (!value.has_value()) return;
  std::unique_ptr<DomNode> text = createCharacterNode(*value, NodeType::Text);
  text->parent = node;
  node->children.push_back(std::move(text));
}

void replaceChildren(DomNode* element, const std::string& html) {
  if (element == nullptr) return;
  for (auto& child : element->children) {
    child->parent = nullptr;
  }
  element->children.clear();
  if (html.empty()) return;

  if (fragmentParser() == nullptr) {
    fragmentParser() = std::make_unique<HtmlParser>();
  }
  DomNode* fragment = fragmentParser()->parse(html);
  if (fragment == nullptr) return;
  std::vector<std::unique_ptr<DomNode>> parsed = std::move(fragment->children);
  for (auto& child : parsed) {
    child->parent = element;
    element->children.push_back(std::move(child));
  }
}

std::unique_ptr<DomNode> createElementNode(const std::string& tag) {
  auto node = std::make_unique<DomNode>();
  node->type = NodeType::Element;
  node->tag = toLower(tag);
  return node;
}

std::unique_ptr<DomNode> createCharacterNode(const std::string& data, NodeType type) {
  auto node = std::make_unique<DomNode>();
  node->type = type;
  node->text = data;
  return node;
}

// ---------------------------------------------------------------------
// Wrapper registry
// ---------------------------------------------------------------------

std::unordered_map<DomNode*, Node*>& nodeRegistry() {
  static std::unordered_map<DomNode*, Node*> registry;
  return registry;
}

std::unordered_map<DomNode*, NodeList*>& childListRegistry() {
  static std::unordered_map<DomNode*, NodeList*> registry;
  return registry;
}

std::unordered_map<DomNode*, DOMTokenList*>& tokenListRegistry() {
  static std::unordered_map<DomNode*, DOMTokenList*> registry;
  return registry;
}

std::unordered_map<DomNode*, CSSStyleDeclaration*>& styleRegistry() {
  static std::unordered_map<DomNode*, CSSStyleDeclaration*> registry;
  return registry;
}

std::vector<std::unique_ptr<NodeList>>& listOwners() {
  static std::vector<std::unique_ptr<NodeList>> owners;
  return owners;
}

std::vector<std::unique_ptr<DOMTokenList>>& tokenListOwners() {
  static std::vector<std::unique_ptr<DOMTokenList>> owners;
  return owners;
}

std::vector<std::unique_ptr<CSSStyleDeclaration>>& styleOwners() {
  static std::vector<std::unique_ptr<CSSStyleDeclaration>> owners;
  return owners;
}

std::vector<std::unique_ptr<Event>>& eventOwners() {
  static std::vector<std::unique_ptr<Event>> owners;
  return owners;
}

std::vector<std::unique_ptr<Node>>& nodeOwners() {
  static std::vector<std::unique_ptr<Node>> owners;
  return owners;
}

Node* createWrapper(DomNode* node) {
  switch (node->type) {
    case NodeType::Document:
      return new DomDocumentImpl(node);
    case NodeType::Element:
    case NodeType::Template: {
      const std::string tag = toLower(node->tag);
      if (tag == "input") return new DomInputElementImpl(node);
      if (tag == "textarea") return new DomTextAreaElementImpl(node);
      if (isHtmlTag(tag)) return new DomHTMLElementImpl(node);
      return new DomElementImpl(node);
    }
    case NodeType::Text:
    case NodeType::Whitespace:
    case NodeType::CData:
      return new DomTextImpl(node);
    case NodeType::Comment:
      return new DomCommentImpl(node);
  }
  return new DomNodeImpl(node);
}

Node* wrapNode(DomNode* node) {
  if (node == nullptr) return nullptr;
  auto& registry = nodeRegistry();
  const auto position = registry.find(node);
  if (position != registry.end()) return position->second;
  Node* wrapper = createWrapper(node);
  nodeOwners().push_back(std::unique_ptr<Node>(wrapper));
  registry[node] = wrapper;
  return wrapper;
}

NodeList* makeNodeList(const std::vector<DomNode*>& nodes) {
  std::vector<Node*> wrapped;
  wrapped.reserve(nodes.size());
  for (DomNode* node : nodes) {
    wrapped.push_back(wrapNode(node));
  }
  auto list = std::make_unique<NodeListImpl>(std::move(wrapped));
  NodeList* result = list.get();
  listOwners().push_back(std::move(list));
  return result;
}

NodeList* makeChildNodeList(DomNode* node) {
  if (node == nullptr) return makeNodeList({});
  auto& registry = childListRegistry();
  const auto position = registry.find(node);
  if (position != registry.end()) return position->second;
  std::vector<DomNode*> children;
  children.reserve(node->children.size());
  for (const auto& child : node->children) {
    children.push_back(child.get());
  }
  NodeList* list = makeNodeList(children);
  registry[node] = list;
  return list;
}

DOMTokenList* makeTokenList(DomNode* element) {
  if (element == nullptr) return nullptr;
  auto& registry = tokenListRegistry();
  const auto position = registry.find(element);
  if (position != registry.end()) return position->second;
  auto list = std::make_unique<DOMTokenListImpl>(element);
  DOMTokenList* result = list.get();
  tokenListOwners().push_back(std::move(list));
  registry[element] = result;
  return result;
}

CSSStyleDeclaration* makeStyleDeclaration(DomNode* element) {
  if (element == nullptr) return nullptr;
  auto& registry = styleRegistry();
  const auto position = registry.find(element);
  if (position != registry.end()) return position->second;
  auto style = std::make_unique<CSSStyleDeclarationImpl>(element);
  CSSStyleDeclaration* result = style.get();
  styleOwners().push_back(std::move(style));
  registry[element] = result;
  return result;
}

Event* makeEvent(const std::string& type, EventTarget* target) {
  auto event = std::make_unique<EventImpl>(type, target);
  Event* result = event.get();
  eventOwners().push_back(std::move(event));
  return result;
}

// ---------------------------------------------------------------------
// EventTarget implementation
// ---------------------------------------------------------------------

void DomBacking::addEventListener(const std::string& type,
                                  v8::Local<v8::Object> listener) {
  if (listener.IsEmpty()) return;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (isolate == nullptr) return;
  for (const Listener& existing : m_listeners) {
    if (existing.type != type) continue;
    if (existing.callback.Get(isolate)->StrictEquals(listener)) return;
  }
  Listener entry;
  entry.type = type;
  entry.callback.Reset(isolate, listener);
  m_listeners.push_back(std::move(entry));
}

void DomBacking::removeEventListener(const std::string& type,
                                     v8::Local<v8::Object> listener) {
  if (listener.IsEmpty()) return;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (isolate == nullptr) return;
  m_listeners.erase(
      std::remove_if(m_listeners.begin(), m_listeners.end(),
                     [&](const Listener& existing) {
                       if (existing.type != type) return false;
                       return existing.callback.Get(isolate)->StrictEquals(listener);
                     }),
      m_listeners.end());
}

bool DomBacking::dispatchEvent(Event* event) {
  if (event == nullptr) return false;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (isolate == nullptr) return false;
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) return false;

  v8::Local<v8::Object> eventObject = webidl::EventBinding::Wrap(isolate, event);
  for (const Listener& listener : m_listeners) {
    if (listener.type != event->type()) continue;
    v8::Local<v8::Value> callback = listener.callback.Get(isolate);
    if (callback.IsEmpty() || !callback->IsFunction()) continue;
    v8::TryCatch tryCatch(isolate);
    v8::Local<v8::Value> args[] = {eventObject};
    const v8::MaybeLocal<v8::Value> result =
        callback.As<v8::Function>()->Call(context, context->Global(), 1, args);
    (void)result;
  }
  return true;
}

}  // namespace

// ---------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------

void WebIDLDom::Install(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> global) {
  webidl::InstallAll(isolate, global);
}

void WebIDLDom::SetDocument(v8::Isolate* isolate, DomNode* root) {
  if (isolate == nullptr) return;
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) return;

  Document* document = dynamic_cast<Document*>(wrapNode(root));
  currentDocument() = root;
  v8::Local<v8::Object> object = webidl::DocumentBinding::Wrap(isolate, document);
  if (object.IsEmpty()) return;
  context->Global()
      ->Set(context, v8::String::NewFromUtf8(isolate, "document").ToLocalChecked(), object)
      .Check();
}

DomNode* WebIDLDom::GetDocument() { return currentDocument(); }

void WebIDLDom::Reset() {
  // The DOM objects hold persistent V8 handles, so they have to be released
  // before the isolate is disposed.
  nodeRegistry().clear();
  childListRegistry().clear();
  tokenListRegistry().clear();
  styleRegistry().clear();
  nodeOwners().clear();
  listOwners().clear();
  tokenListOwners().clear();
  styleOwners().clear();
  eventOwners().clear();
  ownedNodes().clear();
  currentDocument() = nullptr;

  // The generated bindings cache templates and prototypes in persistent
  // handles as well.
  webidl::Reset();
}

}  // namespace TinyBrowser
