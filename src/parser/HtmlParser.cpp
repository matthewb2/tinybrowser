#include "TinyBrowser/HtmlParser.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>

#include <gumbo.h>

namespace TinyBrowser {

// ---- DomNode ----

const std::string* DomNode::getAttribute(const std::string& name) const {
  for (const auto& attr : attributes) {
    if (attr.name == name) return &attr.value;
  }
  return nullptr;
}

void DomNode::getElementsByTagName(const std::string& tag,
                                   std::vector<DomNode*>& out) const {
  if (isElement() && this->tag == tag) {
    out.push_back(const_cast<DomNode*>(this));
  }
  for (const auto& child : children) {
    child->getElementsByTagName(tag, out);
  }
}

std::string DomNode::innerText() const {
  if (isText()) return text;
  std::string result;
  for (const auto& child : children) {
    result += child->innerText();
  }
  return result;
}

void DomNode::dumpTree(int indent) const {
  for (int i = 0; i < indent; i++) printf("  ");
  switch (type) {
    case NodeType::Document:   printf("#document\n"); break;
    case NodeType::Element:    printf("<%s>\n", tag.c_str()); break;
    case NodeType::Text:       printf("\"%s\"\n", text.c_str()); break;
    case NodeType::Whitespace: printf("(ws)\n"); break;
    case NodeType::Comment:    printf("<!-- %s -->\n", text.c_str()); break;
    default:                   printf("(node)\n"); break;
  }
  for (const auto& child : children) {
    child->dumpTree(indent + 1);
  }
}

// ---- HtmlParser ----

HtmlParser::~HtmlParser() {
  if (m_output) {
    gumbo_destroy_output(&kGumboDefaultOptions, m_output);
    m_output = nullptr;
  }
}

DomNode* HtmlParser::parse(const std::string& html) {
  if (m_output) {
    gumbo_destroy_output(&kGumboDefaultOptions, m_output);
    m_output = nullptr;
  }
  m_root.reset();
  m_errorCount = 0;

  m_output = gumbo_parse(html.c_str());
  if (!m_output) return nullptr;

  m_errorCount = static_cast<int>(m_output->errors.length);

  auto docNode = std::make_unique<DomNode>();
  docNode->type = NodeType::Document;

  const GumboVector* children = &m_output->document->v.document.children;
  for (unsigned int i = 0; i < children->length; i++) {
    auto child = convertNode(static_cast<const GumboNode*>(children->data[i]));
    if (child) {
      child->parent = docNode.get();
      docNode->children.push_back(std::move(child));
    }
  }

  m_root = std::move(docNode);
  return m_root.get();
}

std::string HtmlParser::title() const {
  DomNode* h = head();
  if (!h) return {};
  std::vector<DomNode*> titles;
  h->getElementsByTagName("title", titles);
  if (titles.empty()) return {};
  return titles[0]->innerText();
}

DomNode* HtmlParser::head() const {
  if (!m_root) return nullptr;
  std::vector<DomNode*> heads;
  m_root->getElementsByTagName("head", heads);
  return heads.empty() ? nullptr : heads[0];
}

DomNode* HtmlParser::body() const {
  if (!m_root) return nullptr;
  std::vector<DomNode*> bodies;
  m_root->getElementsByTagName("body", bodies);
  return bodies.empty() ? nullptr : bodies[0];
}

std::unique_ptr<DomNode> HtmlParser::convertNode(const GumboNode* node) {
  if (!node) return nullptr;

  auto dom = std::make_unique<DomNode>();

  switch (node->type) {
    case GUMBO_NODE_DOCUMENT: {
      dom->type = NodeType::Document;
      const GumboVector* ch = &node->v.document.children;
      for (unsigned int i = 0; i < ch->length; i++) {
        auto child = convertNode(static_cast<const GumboNode*>(ch->data[i]));
        if (child) {
          child->parent = dom.get();
          dom->children.push_back(std::move(child));
        }
      }
      break;
    }

    case GUMBO_NODE_ELEMENT:
    case GUMBO_NODE_TEMPLATE: {
      dom->type = (node->type == GUMBO_NODE_TEMPLATE)
                      ? NodeType::Template
                      : NodeType::Element;

      const GumboElement& elem = node->v.element;
      dom->tag = gumbo_normalized_tagname(elem.tag);

      if (elem.tag == GUMBO_TAG_UNKNOWN && elem.original_tag.data) {
        GumboStringPiece piece = elem.original_tag;
        gumbo_tag_from_original_text(&piece);
        if (piece.data && piece.length > 0) {
          dom->tag.assign(piece.data, piece.length);
        }
      }

      const GumboVector* attrs = &elem.attributes;
      for (unsigned int i = 0; i < attrs->length; i++) {
        const GumboAttribute* attr =
            static_cast<const GumboAttribute*>(attrs->data[i]);
        DomAttribute da;
        da.name = attr->name ? attr->name : "";
        da.value = attr->value ? attr->value : "";
        dom->attributes.push_back(std::move(da));
      }

      const GumboVector* ch = &elem.children;
      for (unsigned int i = 0; i < ch->length; i++) {
        auto child = convertNode(static_cast<const GumboNode*>(ch->data[i]));
        if (child) {
          child->parent = dom.get();
          dom->children.push_back(std::move(child));
        }
      }
      break;
    }

    case GUMBO_NODE_TEXT:
    case GUMBO_NODE_CDATA:
    case GUMBO_NODE_WHITESPACE: {
      dom->type = (node->type == GUMBO_NODE_WHITESPACE)
                      ? NodeType::Whitespace
                      : (node->type == GUMBO_NODE_CDATA
                             ? NodeType::CData
                             : NodeType::Text);
      dom->text = node->v.text.text ? node->v.text.text : "";
      break;
    }

    case GUMBO_NODE_COMMENT: {
      dom->type = NodeType::Comment;
      dom->text = node->v.text.text ? node->v.text.text : "";
      break;
    }
  }

  return dom;
}

} // namespace TinyBrowser
