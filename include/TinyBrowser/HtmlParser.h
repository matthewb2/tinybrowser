#pragma once

#include <string>
#include <vector>
#include <memory>

typedef struct GumboInternalOutput GumboOutput;
typedef struct GumboInternalNode GumboNode;

namespace TinyBrowser {

enum class NodeType {
  Document,
  Element,
  Text,
  Comment,
  Whitespace,
  CData,
  Template,
};

class DomNode;

struct DomAttribute {
  std::string name;
  std::string value;
};

class DomNode {
public:
  NodeType type = NodeType::Element;
  std::string tag;
  std::string text;
  std::vector<DomAttribute> attributes;
  std::vector<std::unique_ptr<DomNode>> children;
  DomNode* parent = nullptr;

  bool isElement() const { return type == NodeType::Element; }
  bool isText() const { return type == NodeType::Text; }

  const std::string* getAttribute(const std::string& name) const;
  void getElementsByTagName(const std::string& tag,
                            std::vector<DomNode*>& out) const;
  std::string innerText() const;
  void dumpTree(int indent = 0) const;
};

class HtmlParser {
public:
  HtmlParser() = default;
  ~HtmlParser();

  HtmlParser(const HtmlParser&) = delete;
  HtmlParser& operator=(const HtmlParser&) = delete;

  // Parse UTF-8 HTML string → DOM tree. Returns <html> root or nullptr.
  DomNode* parse(const std::string& html);

  std::string title() const;
  DomNode* root() const { return m_root.get(); }
  DomNode* head() const;
  DomNode* body() const;
  int errorCount() const { return m_errorCount; }

private:
  std::unique_ptr<DomNode> convertNode(const GumboNode* node);

  std::unique_ptr<DomNode> m_root;
  GumboOutput* m_output = nullptr;
  int m_errorCount = 0;
};

} // namespace TinyBrowser