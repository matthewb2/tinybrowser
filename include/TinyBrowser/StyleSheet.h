#pragma once

#include <string>
#include <vector>
#include <utility>

#include "TinyBrowser/ComputedStyle.h"
#include "TinyBrowser/HtmlParser.h"

namespace TinyBrowser {

class StyleSheet {
public:
    struct Compound {
        std::string tag;
        std::string id;
        std::vector<std::string> classes;
        std::vector<std::pair<std::string, std::string>> attributes;
        std::vector<std::string> pseudos;
        std::vector<Compound> negations;
        int specB = 0;
        int specC = 0;
        bool valid = false;
        bool alwaysFalse = false;
    };

    struct Complex {
        std::vector<Compound> compounds;
        std::vector<char> combinators;
        int specificity = 0;
        bool valid = false;
    };

    struct Declaration {
        std::string property;
        std::string value;
        bool important = false;
    };

    struct Rule {
        std::vector<Complex> selectors;
        std::vector<Declaration> declarations;
        int order = 0;
        float mediaMinWidth = -1.0f;
        float mediaMaxWidth = -1.0f;
    };

    StyleSheet() = default;

    void setViewport(float width, float height);
    void parse(const std::string& css);

    ComputedStyle computeStyle(const DomNode* node,
                               const ComputedStyle& parent) const;

    size_t ruleCount() const { return m_rules.size(); }

private:
    bool mediaMatches(const Rule& rule) const;

    std::vector<Rule> m_rules;
    float m_viewportW = 800.0f;
    float m_viewportH = 600.0f;
};

} // namespace TinyBrowser
