#include "TinyBrowser/StyleSheet.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>

#include "CSSParser.h"
#include "CSSDeNest.h"

namespace TinyBrowser {
namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> splitWhitespace(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

std::vector<std::string> splitTopLevel(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    for (char c : s) {
        if (c == '(' || c == '[') depth++;
        else if (c == ')' || c == ']') depth--;
        if (c == sep && depth <= 0) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

bool parseFloat(const std::string& s, float& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    float v = std::strtof(s.c_str(), &end);
    if (end == s.c_str()) return false;
    out = v;
    return true;
}

bool isElementNode(const DomNode* n) {
    return n && n->type == NodeType::Element;
}

const DomNode* parentElement(const DomNode* n) {
    const DomNode* p = n ? n->parent : nullptr;
    return isElementNode(p) ? p : nullptr;
}

const DomNode* prevElementSibling(const DomNode* n) {
    const DomNode* p = n ? n->parent : nullptr;
    if (!p) return nullptr;
    const DomNode* prev = nullptr;
    for (const auto& child : p->children) {
        if (child.get() == n) return prev;
        if (isElementNode(child.get())) prev = child.get();
    }
    return nullptr;
}

const DomNode* nextElementSibling(const DomNode* n) {
    const DomNode* p = n ? n->parent : nullptr;
    if (!p) return nullptr;
    bool found = false;
    for (const auto& child : p->children) {
        if (child.get() == n) { found = true; continue; }
        if (found && isElementNode(child.get())) return child.get();
    }
    return nullptr;
}

std::string attrValue(const DomNode* n, const std::string& name) {
    if (!n) return {};
    const std::string* v = n->getAttribute(name);
    return v ? *v : std::string();
}

QColor parseColor(const std::string& in, bool& ok) {
    ok = false;
    std::string s = toLower(trim(in));
    if (s.empty()) return QColor(0, 0, 0);

    static const std::map<std::string, QColor> named = {
        {"transparent", QColor(0, 0, 0, 0)},
        {"black", QColor(0, 0, 0)},       {"white", QColor(255, 255, 255)},
        {"red", QColor(255, 0, 0)},       {"green", QColor(0, 128, 0)},
        {"blue", QColor(0, 0, 255)},      {"gray", QColor(128, 128, 128)},
        {"grey", QColor(128, 128, 128)},  {"silver", QColor(192, 192, 192)},
        {"maroon", QColor(128, 0, 0)},    {"yellow", QColor(255, 255, 0)},
        {"olive", QColor(128, 128, 0)},   {"lime", QColor(0, 255, 0)},
        {"aqua", QColor(0, 255, 255)},    {"teal", QColor(0, 128, 128)},
        {"navy", QColor(0, 0, 128)},      {"fuchsia", QColor(255, 0, 255)},
        {"purple", QColor(128, 0, 128)},  {"orange", QColor(255, 165, 0)},
        {"whitesmoke", QColor(245, 245, 245)},
        {"gainsboro", QColor(220, 220, 220)},
    };

    if (s[0] == '#') {
        std::string hex = s.substr(1);
        auto hexv = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        if (hex.size() == 3 || hex.size() == 4) {
            int r = hexv(hex[0]), g = hexv(hex[1]), b = hexv(hex[2]);
            int a = hex.size() == 4 ? hexv(hex[3]) : 15;
            if (r < 0 || g < 0 || b < 0 || a < 0) return QColor(0, 0, 0);
            ok = true;
            return QColor(r * 17, g * 17, b * 17, a * 17);
        }
        if (hex.size() == 6 || hex.size() == 8) {
            int r = hexv(hex[0]) * 16 + hexv(hex[1]);
            int g = hexv(hex[2]) * 16 + hexv(hex[3]);
            int b = hexv(hex[4]) * 16 + hexv(hex[5]);
            int a = hex.size() == 8 ? hexv(hex[6]) * 16 + hexv(hex[7]) : 255;
            ok = true;
            return QColor(r, g, b, a);
        }
        return QColor(0, 0, 0);
    }

    if (s.rfind("rgb", 0) == 0) {
        size_t lp = s.find('(');
        size_t rp = s.find(')');
        if (lp != std::string::npos && rp != std::string::npos) {
            std::string inner = s.substr(lp + 1, rp - lp - 1);
            std::replace(inner.begin(), inner.end(), ',', ' ');
            auto parts = splitWhitespace(inner);
            if (parts.size() >= 3) {
                float r = 0, g = 0, b = 0, a = 255;
                parseFloat(parts[0], r);
                parseFloat(parts[1], g);
                parseFloat(parts[2], b);
                if (parts.size() >= 4) {
                    float av = 0;
                    if (parseFloat(parts[3], av)) a = av <= 1.0f ? av * 255.0f : av;
                }
                ok = true;
                return QColor(static_cast<int>(r), static_cast<int>(g),
                              static_cast<int>(b), static_cast<int>(a));
            }
        }
    }

    auto it = named.find(s);
    if (it != named.end()) {
        ok = true;
        return it->second;
    }
    return QColor(0, 0, 0);
}

Length parseLengthValue(const std::string& tok, float emBase) {
    std::string t = toLower(trim(tok));
    if (t.empty() || t == "auto") return Length::autoLen();

    size_t i = 0;
    if (i < t.size() && (t[i] == '+' || t[i] == '-')) i++;
    while (i < t.size() &&
           (std::isdigit(static_cast<unsigned char>(t[i])) || t[i] == '.')) {
        i++;
    }
    std::string numPart = t.substr(0, i);
    std::string unit = t.substr(i);

    float v = 0.0f;
    if (!parseFloat(numPart, v)) return Length::autoLen();

    if (unit == "px") return Length::px(v);
    if (unit == "pt") return Length::px(v * 96.0f / 72.0f);
    if (unit == "em") return Length::px(v * emBase);
    if (unit == "rem") return Length::px(v * 16.0f);
    if (unit == "%") return Length::percent(v);
    if (unit == "vh") return Length{v, Length::Unit::Vh};
    if (unit == "vw") return Length{v, Length::Unit::Vw};
    if (unit.empty()) return Length::px(v);
    return Length::autoLen();
}

float resolveLength(const Length& l, float containingWidth, float vw, float vh) {
    switch (l.unit) {
        case Length::Unit::Px: return l.value;
        case Length::Unit::Percent: return l.value / 100.0f * containingWidth;
        case Length::Unit::Vh: return l.value / 100.0f * vh;
        case Length::Unit::Vw: return l.value / 100.0f * vw;
        case Length::Unit::Auto:
        default: return 0.0f;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Selector parsing
// ---------------------------------------------------------------------------

namespace {

void parseCompoundStr(const std::string& str, StyleSheet::Compound& out) {
    out.valid = false;
    size_t i = 0;
    const size_t n = str.size();

    if (i < n && (std::isalpha(static_cast<unsigned char>(str[i])) || str[i] == '_' ||
                  str[i] == '*')) {
        std::string type;
        while (i < n && (std::isalnum(static_cast<unsigned char>(str[i])) ||
                         str[i] == '-' || str[i] == '_' || str[i] == '*')) {
            type.push_back(str[i]);
            i++;
        }
        if (type != "*") {
            out.tag = toLower(type);
            out.specC += 1;
        }
    }

    while (i < n) {
        char c = str[i];
        if (c == '.') {
            i++;
            std::string cls;
            while (i < n && (std::isalnum(static_cast<unsigned char>(str[i])) ||
                             str[i] == '-' || str[i] == '_')) {
                cls.push_back(str[i]);
                i++;
            }
            if (cls.empty()) { out.valid = false; return; }
            out.classes.push_back(cls);
            out.specB += 1;
        } else if (c == '#') {
            i++;
            std::string id;
            while (i < n && (std::isalnum(static_cast<unsigned char>(str[i])) ||
                             str[i] == '-' || str[i] == '_')) {
                id.push_back(str[i]);
                i++;
            }
            if (id.empty()) { out.valid = false; return; }
            out.id = id;
            out.specB += 1;
        } else if (c == '[') {
            size_t close = str.find(']', i);
            if (close == std::string::npos) { out.valid = false; return; }
            std::string body = str.substr(i + 1, close - i - 1);
            std::string name, value;
            size_t eq = body.find('=');
            if (eq == std::string::npos) {
                name = trim(body);
            } else {
                name = trim(body.substr(0, eq));
                value = trim(body.substr(eq + 1));
                if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
                    value = value.substr(1, value.size() - 2);
                }
            }
            if (name.empty()) { out.valid = false; return; }
            out.attributes.emplace_back(name, value);
            out.specB += 1;
            i = close + 1;
        } else if (c == ':') {
            if (i + 1 < n && str[i + 1] == ':') {
                out.valid = false;
                return;
            }
            i++;
            std::string pseudo;
            while (i < n && (std::isalnum(static_cast<unsigned char>(str[i])) ||
                             str[i] == '-')) {
                pseudo.push_back(str[i]);
                i++;
            }
            pseudo = toLower(pseudo);
            if (pseudo == "not" && i < n && str[i] == '(') {
                size_t close = str.find(')', i);
                if (close == std::string::npos) { out.valid = false; return; }
                std::string inner = str.substr(i + 1, close - i - 1);
                StyleSheet::Compound neg;
                parseCompoundStr(trim(inner), neg);
                if (!neg.valid) { out.valid = false; return; }
                out.specB += neg.specB;
                out.specC += neg.specC;
                out.negations.push_back(neg);
                i = close + 1;
            } else {
                out.pseudos.push_back(pseudo);
                if (pseudo != "root" && pseudo != "first-child" &&
                    pseudo != "last-child" && pseudo != "first-of-type" &&
                    pseudo != "last-of-type" && pseudo != "empty" &&
                    pseudo != "link" && pseudo != "first-child") {
                    out.alwaysFalse = true;
                }
                if (pseudo == "not") out.alwaysFalse = true;
                out.specB += 1;
            }
        } else {
            out.valid = false;
            return;
        }
    }
    out.valid = true;
}

StyleSheet::Complex parseComplexSelector(const std::string& str) {
    StyleSheet::Complex cx;
    std::vector<std::string> compoundStrs;
    std::vector<char> combs;

    size_t i = 0;
    const size_t n = str.size();
    while (i < n && std::isspace(static_cast<unsigned char>(str[i]))) i++;

    char pending = 0;
    bool haveCompound = false;
    while (i < n) {
        char c = str[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            i++;
            if (haveCompound && pending == 0) pending = ' ';
            continue;
        }
        if (c == '>' || c == '+' || c == '~') {
            pending = c;
            i++;
            continue;
        }
        std::string comp;
        int depth = 0;
        while (i < n) {
            char d = str[i];
            if (d == '[' || d == '(') depth++;
            else if (d == ']' || d == ')') depth--;
            if (depth == 0 &&
                (std::isspace(static_cast<unsigned char>(d)) || d == '>' || d == '+' ||
                 d == '~')) {
                break;
            }
            comp.push_back(d);
            i++;
        }
        if (haveCompound) combs.push_back(pending ? pending : ' ');
        compoundStrs.push_back(comp);
        haveCompound = true;
        pending = 0;
    }

    if (compoundStrs.empty()) return cx;

    int specA = 0;
    for (const auto& cs : compoundStrs) {
        StyleSheet::Compound comp;
        parseCompoundStr(cs, comp);
        if (!comp.valid) {
            cx.valid = false;
            return cx;
        }
        if (!comp.id.empty()) specA += 1;
        cx.compounds.push_back(std::move(comp));
    }
    cx.combinators = combs;

    int b = 0, c = 0;
    for (const auto& comp : cx.compounds) {
        b += comp.specB;
        c += comp.specC;
    }
    cx.specificity = specA * 10000 + b * 100 + c;
    cx.valid = true;
    return cx;
}

bool matchCompound(const StyleSheet::Compound& comp, const DomNode* node);

bool matchCompound(const StyleSheet::Compound& comp, const DomNode* node) {
    if (!isElementNode(node)) return false;
    if (comp.alwaysFalse) return false;

    if (!comp.tag.empty() && toLower(node->tag) != comp.tag) return false;
    if (!comp.id.empty() && attrValue(node, "id") != comp.id) return false;

    if (!comp.classes.empty()) {
        std::string cls = attrValue(node, "class");
        auto tokens = splitWhitespace(cls);
        for (const auto& want : comp.classes) {
            bool found = false;
            for (const auto& have : tokens) {
                if (have == want) { found = true; break; }
            }
            if (!found) return false;
        }
    }

    for (const auto& attr : comp.attributes) {
        const std::string* have = node->getAttribute(attr.first);
        if (!have) return false;
        if (!attr.second.empty() && *have != attr.second) return false;
    }

    for (const auto& neg : comp.negations) {
        if (matchCompound(neg, node)) return false;
    }

    for (const auto& pseudo : comp.pseudos) {
        if (pseudo == "root") {
            if (parentElement(node) != nullptr) return false;
        } else if (pseudo == "first-child") {
            if (prevElementSibling(node) != nullptr) return false;
        } else if (pseudo == "last-child") {
            if (nextElementSibling(node) != nullptr) return false;
        } else if (pseudo == "first-of-type") {
            const DomNode* p = prevElementSibling(node);
            while (p) {
                if (p->tag == node->tag) return false;
                p = prevElementSibling(p);
            }
        } else if (pseudo == "last-of-type") {
            const DomNode* s = nextElementSibling(node);
            while (s) {
                if (s->tag == node->tag) return false;
                s = nextElementSibling(s);
            }
        } else if (pseudo == "empty") {
            for (const auto& ch : node->children) {
                if (isElementNode(ch.get())) return false;
                if (ch->isText() && !trim(ch->text).empty()) return false;
            }
        } else if (pseudo == "link") {
            if (toLower(node->tag) != "a") return false;
            if (!node->getAttribute("href")) return false;
        } else {
            return false;
        }
    }

    return true;
}

bool matchComplexAt(const StyleSheet::Complex& cx, int idx, const DomNode* node) {
    if (!node || !isElementNode(node)) return false;
    if (!matchCompound(cx.compounds[idx], node)) return false;
    if (idx == 0) return true;

    char comb = cx.combinators[idx - 1];
    if (comb == '>') {
        return matchComplexAt(cx, idx - 1, parentElement(node));
    }
    if (comb == ' ') {
        for (const DomNode* a = parentElement(node); a; a = parentElement(a)) {
            if (matchComplexAt(cx, idx - 1, a)) return true;
        }
        return false;
    }
    if (comb == '+') {
        return matchComplexAt(cx, idx - 1, prevElementSibling(node));
    }
    if (comb == '~') {
        for (const DomNode* s = prevElementSibling(node); s; s = prevElementSibling(s)) {
            if (matchComplexAt(cx, idx - 1, s)) return true;
        }
        return false;
    }
    return false;
}

bool matchComplex(const StyleSheet::Complex& cx, const DomNode* node) {
    if (!cx.valid || cx.compounds.empty()) return false;
    return matchComplexAt(cx, static_cast<int>(cx.compounds.size()) - 1, node);
}

// ---------------------------------------------------------------------------
// Value application
// ---------------------------------------------------------------------------

void applyFontSize(ComputedStyle& s, const std::string& value, float parentFont) {
    Length l = parseLengthValue(value, parentFont);
    if (l.unit == Length::Unit::Px) s.fontSize = l.value;
    else if (l.unit == Length::Unit::Percent) s.fontSize = parentFont * l.value / 100.0f;
    else if (l.unit == Length::Unit::Auto) {
        if (toLower(trim(value)) == "small") s.fontSize = 13.0f;
        else if (toLower(trim(value)) == "large") s.fontSize = 18.0f;
        else if (toLower(trim(value)) == "x-large") s.fontSize = 24.0f;
    }
    if (s.fontSize < 1.0f) s.fontSize = 1.0f;
}

void applyBoxShorthand(Length box[4], const std::string& value, float emBase) {
    auto parts = splitWhitespace(value);
    if (parts.empty()) return;
    if (parts.size() == 1) {
        Length v = parseLengthValue(parts[0], emBase);
        for (int i = 0; i < 4; i++) box[i] = v;
    } else if (parts.size() == 2) {
        Length v = parseLengthValue(parts[0], emBase);
        Length h = parseLengthValue(parts[1], emBase);
        box[0] = v; box[1] = h; box[2] = v; box[3] = h;
    } else if (parts.size() == 3) {
        box[0] = parseLengthValue(parts[0], emBase);
        box[1] = parseLengthValue(parts[1], emBase);
        box[2] = parseLengthValue(parts[2], emBase);
        box[3] = box[1];
    } else {
        for (int i = 0; i < 4; i++) box[i] = parseLengthValue(parts[i], emBase);
    }
}

void applyBorder(ComputedStyle& s, const std::string& value, float emBase) {
    std::string v = toLower(trim(value));
    if (v.empty() || v == "none" || v == "0") {
        for (int i = 0; i < 4; i++) s.borderWidth[i] = 0.0f;
        return;
    }
    auto parts = splitWhitespace(v);
    float width = 1.0f;
    bool haveWidth = false;
    bool ok = false;
    for (const auto& p : parts) {
        QColor col = parseColor(p, ok);
        if (ok) { s.borderColor = col; continue; }
        if (p == "thin") { width = 1.0f; haveWidth = true; continue; }
        if (p == "medium") { width = 3.0f; haveWidth = true; continue; }
        if (p == "thick") { width = 5.0f; haveWidth = true; continue; }
        if (p == "solid" || p == "dashed" || p == "dotted" || p == "double") continue;
        Length l = parseLengthValue(p, emBase);
        if (l.unit == Length::Unit::Px) { width = l.value; haveWidth = true; }
    }
    if (!haveWidth) width = 1.0f;
    for (int i = 0; i < 4; i++) s.borderWidth[i] = width;
}

void applyDeclaration(ComputedStyle& s, const std::string& rawProp,
                      const std::string& rawValue, float parentFont) {
    std::string prop = toLower(trim(rawProp));
    std::string value = trim(rawValue);
    if (prop.empty() || value.empty()) return;

    float emBase = parentFont;
    bool colorOk = false;

    if (prop == "display") {
        std::string v = toLower(value);
        if (v == "none") s.display = Display::None;
        else if (v == "block") s.display = Display::Block;
        else if (v == "inline") s.display = Display::Inline;
        else if (v == "inline-block") s.display = Display::InlineBlock;
        else if (v == "flex" || v == "inline-flex") s.display = Display::Flex;
        else if (v == "list-item") s.display = Display::Block;
    } else if (prop == "color") {
        QColor c = parseColor(value, colorOk);
        if (colorOk) s.color = c;
    } else if (prop == "background" || prop == "background-color") {
        for (const auto& tok : splitWhitespace(value)) {
            if (tok.rfind("url", 0) == 0) continue;
            QColor c = parseColor(tok, colorOk);
            if (colorOk) { s.background = c; break; }
        }
    } else if (prop == "font-size") {
        applyFontSize(s, value, parentFont);
    } else if (prop == "font-weight") {
        std::string v = toLower(value);
        if (v == "bold" || v == "bolder") s.fontWeight = 700;
        else if (v == "normal" || v == "lighter") s.fontWeight = 400;
        else {
            float w = 0;
            if (parseFloat(v, w)) s.fontWeight = static_cast<int>(w);
        }
    } else if (prop == "font-style") {
        std::string v = toLower(value);
        s.italic = (v == "italic" || v == "oblique");
    } else if (prop == "font-family") {
        std::string first = splitTopLevel(value, ',').front();
        first = trim(first);
        if (!first.empty() && (first.front() == '"' || first.front() == '\'')) {
            first = first.substr(1, first.size() - 2);
        }
        if (!first.empty()) s.fontFamily = first;
    } else if (prop == "line-height") {
        std::string v = toLower(trim(value));
        if (v == "normal") {
            s.lineHeightPx = 0.0f;
        } else if (v.find('.') != std::string::npos || v.find('%') != std::string::npos) {
            if (v.back() == '%') {
                float num = 0;
                if (parseFloat(v.substr(0, v.size() - 1), num))
                    s.lineHeightPx = s.fontSize * num / 100.0f;
            } else {
                float num = 0;
                if (parseFloat(v, num)) s.lineHeightPx = s.fontSize * num;
            }
        } else {
            Length l = parseLengthValue(v, s.fontSize);
            if (l.unit == Length::Unit::Px) s.lineHeightPx = l.value;
        }
    } else if (prop == "text-align") {
        std::string v = toLower(value);
        if (v == "center") s.textAlign = TextAlign::Center;
        else if (v == "right") s.textAlign = TextAlign::Right;
        else s.textAlign = TextAlign::Left;
    } else if (prop == "text-decoration" || prop == "text-decoration-line") {
        std::string v = toLower(value);
        if (v.find("underline") != std::string::npos) s.underline = true;
        else if (v.find("none") != std::string::npos) s.underline = false;
    } else if (prop == "margin") {
        applyBoxShorthand(s.margin, value, emBase);
    } else if (prop == "margin-top") {
        s.margin[0] = parseLengthValue(value, emBase);
    } else if (prop == "margin-right") {
        s.margin[1] = parseLengthValue(value, emBase);
    } else if (prop == "margin-bottom") {
        s.margin[2] = parseLengthValue(value, emBase);
    } else if (prop == "margin-left") {
        s.margin[3] = parseLengthValue(value, emBase);
    } else if (prop == "padding") {
        applyBoxShorthand(s.padding, value, emBase);
    } else if (prop == "padding-top") {
        s.padding[0] = parseLengthValue(value, emBase);
    } else if (prop == "padding-right") {
        s.padding[1] = parseLengthValue(value, emBase);
    } else if (prop == "padding-bottom") {
        s.padding[2] = parseLengthValue(value, emBase);
    } else if (prop == "padding-left") {
        s.padding[3] = parseLengthValue(value, emBase);
    } else if (prop == "border") {
        applyBorder(s, value, emBase);
    } else if (prop == "border-top" || prop == "border-bottom" ||
               prop == "border-left" || prop == "border-right") {
        std::string v = toLower(trim(value));
        float width = 0.0f;
        if (v != "none" && v != "0") {
            width = 1.0f;
            auto parts = splitWhitespace(v);
            for (const auto& p : parts) {
                QColor c = parseColor(p, colorOk);
                if (colorOk) { s.borderColor = c; continue; }
                Length l = parseLengthValue(p, emBase);
                if (l.unit == Length::Unit::Px) width = l.value;
            }
        }
        int idx = prop == "border-top" ? 0 : prop == "border-right" ? 1
                : prop == "border-bottom" ? 2 : 3;
        s.borderWidth[idx] = width;
    } else if (prop == "border-color") {
        QColor c = parseColor(value, colorOk);
        if (colorOk) s.borderColor = c;
    } else if (prop == "width") {
        s.width = parseLengthValue(value, emBase);
    } else if (prop == "height") {
        s.height = parseLengthValue(value, emBase);
    } else if (prop == "max-width") {
        s.maxWidth = parseLengthValue(value, emBase);
    } else if (prop == "min-height") {
        s.minHeight = parseLengthValue(value, emBase);
    }
}

void applyDeclarationList(ComputedStyle& s, const std::string& styleAttr,
                          float parentFont) {
    for (const auto& decl : splitTopLevel(styleAttr, ';')) {
        size_t colon = decl.find(':');
        if (colon == std::string::npos) continue;
        applyDeclaration(s, decl.substr(0, colon), decl.substr(colon + 1), parentFont);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// StyleSheet
// ---------------------------------------------------------------------------

void StyleSheet::setViewport(float width, float height) {
    m_viewportW = width > 0 ? width : 800.0f;
    m_viewportH = height > 0 ? height : 600.0f;
}

bool StyleSheet::mediaMatches(const Rule& rule) const {
    if (rule.mediaMinWidth >= 0.0f && m_viewportW < rule.mediaMinWidth) return false;
    if (rule.mediaMaxWidth >= 0.0f && m_viewportW > rule.mediaMaxWidth) return false;
    return true;
}

void StyleSheet::parse(const std::string& css) {
    if (trim(css).empty()) return;

    std::string denested = CSSDeNest::denest_css(css);

    CSSParser parser;
    parser.set_level("CSS3.0");
    parser.parse_css(denested);

    struct AtCtx {
        bool skip = false;
        float minW = -1.0f;
        float maxW = -1.0f;
    };
    std::vector<AtCtx> atStack;

    std::string currentSelector;
    std::vector<Declaration> currentDecls;
    std::string currentProp;
    int order = 0;

    auto parseMedia = [](const std::string& data, float& minW, float& maxW) {
        std::string d = toLower(data);
        auto readWidth = [&](const char* key) -> float {
            size_t p = d.find(key);
            if (p == std::string::npos) return -1.0f;
            size_t colon = d.find(':', p);
            if (colon == std::string::npos) return -1.0f;
            size_t i = colon + 1;
            while (i < d.size() && std::isspace(static_cast<unsigned char>(d[i]))) i++;
            size_t j = i;
            while (j < d.size() && (std::isdigit(static_cast<unsigned char>(d[j])) ||
                                    d[j] == '.')) {
                j++;
            }
            float v = 0.0f;
            if (!parseFloat(d.substr(i, j - i), v)) return -1.0f;
            std::string unit = d.substr(j, 2);
            if (unit == "em") v *= 16.0f;
            return v;
        };
        minW = readWidth("min-width");
        maxW = readWidth("max-width");
    };

    CSSParser::token tok = parser.get_next_token();
    while (tok.type != CSSParser::CSS_END) {
        switch (tok.type) {
            case CSSParser::AT_START: {
                AtCtx ctx;
                std::string d = toLower(tok.data);
                if (d.find("@media") != std::string::npos) {
                    parseMedia(tok.data, ctx.minW, ctx.maxW);
                } else if (d.find("@supports") != std::string::npos ||
                           d.find("@keyframes") != std::string::npos ||
                           d.find("@font-face") != std::string::npos ||
                           d.find("@-webkit-keyframes") != std::string::npos) {
                    ctx.skip = true;
                }
                atStack.push_back(ctx);
                break;
            }
            case CSSParser::AT_END:
                if (!atStack.empty()) atStack.pop_back();
                break;
            case CSSParser::SEL_START:
                currentSelector = tok.data;
                currentDecls.clear();
                break;
            case CSSParser::PROPERTY:
                currentProp = tok.data;
                break;
            case CSSParser::VALUE: {
                std::string val = tok.data;
                bool important = false;
                size_t bang = val.find('!');
                if (bang != std::string::npos) {
                    std::string rest = toLower(val.substr(bang + 1));
                    if (rest.find("important") != std::string::npos) important = true;
                    val = val.substr(0, bang);
                }
                if (!currentProp.empty()) {
                    currentDecls.push_back({trim(currentProp), trim(val), important});
                }
                break;
            }
            case CSSParser::SEL_END: {
                bool skip = false;
                float minW = -1.0f, maxW = -1.0f;
                for (auto it = atStack.rbegin(); it != atStack.rend(); ++it) {
                    if (it->skip || it->minW >= 0.0f || it->maxW >= 0.0f) {
                        skip = it->skip;
                        minW = it->minW;
                        maxW = it->maxW;
                        break;
                    }
                }
                if (!skip && !currentDecls.empty() && !trim(currentSelector).empty()) {
                    Rule rule;
                    rule.declarations = currentDecls;
                    rule.order = order++;
                    rule.mediaMinWidth = minW;
                    rule.mediaMaxWidth = maxW;
                    for (const auto& selStr : splitTopLevel(currentSelector, ',')) {
                        Complex cx = parseComplexSelector(trim(selStr));
                        if (cx.valid) rule.selectors.push_back(std::move(cx));
                    }
                    if (!rule.selectors.empty()) m_rules.push_back(std::move(rule));
                }
                currentSelector.clear();
                currentDecls.clear();
                break;
            }
            default:
                break;
        }
        tok = parser.get_next_token();
    }
}

ComputedStyle StyleSheet::computeStyle(const DomNode* node,
                                       const ComputedStyle& parent) const {
    ComputedStyle s;

    // Inheritance defaults.
    s.fontSize = parent.fontSize;
    s.fontWeight = parent.fontWeight;
    s.italic = parent.italic;
    s.fontFamily = parent.fontFamily;
    s.color = parent.color;
    s.textAlign = parent.textAlign;
    s.lineHeightPx = parent.lineHeightPx;
    s.underline = parent.underline;
    s.display = Display::Inline;

    if (!isElementNode(node)) {
        return s;
    }

    // User-agent (presentational) defaults for common HTML tags.
    std::string tag = toLower(node->tag);
    static const std::vector<std::string> blockTags = {
        "html", "body", "div", "p", "h1", "h2", "h3", "h4", "h5", "h6",
        "ul", "ol", "li", "dl", "dt", "dd", "section", "article", "aside",
        "header", "footer", "main", "nav", "figure", "figcaption",
        "blockquote", "pre", "hr", "form", "fieldset", "table", "address",
        "details", "summary", "center"};
    static const std::vector<std::string> noneTags = {
        "head", "script", "style", "meta", "link", "title", "base", "template"};
    static const std::vector<std::string> inlineBlockTags = {
        "img", "input", "select", "textarea", "button", "canvas", "video"};

    for (const auto& t : noneTags) {
        if (tag == t) { s.display = Display::None; return s; }
    }
    for (const auto& t : blockTags) {
        if (tag == t) { s.display = Display::Block; break; }
    }
    for (const auto& t : inlineBlockTags) {
        if (tag == t) { s.display = Display::InlineBlock; break; }
    }

    if (tag == "body") {
        s.margin[0] = s.margin[1] = s.margin[2] = s.margin[3] = Length::px(8.0f);
    } else if (tag == "p") {
        s.margin[0] = s.margin[2] = Length::px(1.0f * parent.fontSize);
    } else if (tag == "h1") {
        s.fontSize = parent.fontSize * 2.0f; s.fontWeight = 700;
        s.margin[0] = s.margin[2] = Length::px(0.67f * parent.fontSize);
    } else if (tag == "h2") {
        s.fontSize = parent.fontSize * 1.5f; s.fontWeight = 700;
        s.margin[0] = s.margin[2] = Length::px(0.83f * parent.fontSize);
    } else if (tag == "h3") {
        s.fontSize = parent.fontSize * 1.17f; s.fontWeight = 700;
        s.margin[0] = s.margin[2] = Length::px(1.0f * parent.fontSize);
    } else if (tag == "h4" || tag == "h5" || tag == "h6") {
        s.fontWeight = 700;
        s.margin[0] = s.margin[2] = Length::px(1.0f * parent.fontSize);
    } else if (tag == "ul" || tag == "ol") {
        s.margin[0] = s.margin[2] = Length::px(1.0f * parent.fontSize);
        s.padding[3] = Length::px(40.0f);
    } else if (tag == "strong" || tag == "b") {
        s.fontWeight = 700;
    } else if (tag == "em" || tag == "i") {
        s.italic = true;
    } else if (tag == "pre" || tag == "code" || tag == "kbd" || tag == "samp") {
        s.fontFamily = "Courier New";
    } else if (tag == "a") {
        if (node->getAttribute("href")) {
            s.color = QColor(0, 0, 238);
            s.underline = true;
        }
    }

    // Collect matching author declarations.
    struct Match {
        int specificity;
        int order;
        bool important;
        const Declaration* decl;
    };
    std::vector<Match> matches;

    for (const auto& rule : m_rules) {
        if (!mediaMatches(rule)) continue;
        int bestSpec = -1;
        for (const auto& sel : rule.selectors) {
            if (matchComplex(sel, node)) {
                if (sel.specificity > bestSpec) bestSpec = sel.specificity;
            }
        }
        if (bestSpec < 0) continue;
        for (const auto& decl : rule.declarations) {
            matches.push_back({bestSpec, rule.order, decl.important, &decl});
        }
    }

    std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
        if (a.important != b.important) return a.important < b.important;
        if (a.specificity != b.specificity) return a.specificity < b.specificity;
        return a.order < b.order;
    });

    for (const auto& m : matches) {
        applyDeclaration(s, m.decl->property, m.decl->value, parent.fontSize);
    }

    // Inline style attribute wins.
    if (const std::string* styleAttr = node->getAttribute("style")) {
        applyDeclarationList(s, *styleAttr, parent.fontSize);
    }

    return s;
}

} // namespace TinyBrowser
