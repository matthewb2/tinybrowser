#include "TinyBrowser/LayoutEngine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include <QFont>
#include <QFontMetricsF>
#include <QString>

namespace TinyBrowser {
namespace {

float resolveLen(const Length& l, float cb, float vw, float vh) {
    switch (l.unit) {
        case Length::Unit::Px: return l.value;
        case Length::Unit::Percent: return l.value / 100.0f * cb;
        case Length::Unit::Vh: return l.value / 100.0f * vh;
        case Length::Unit::Vw: return l.value / 100.0f * vw;
        case Length::Unit::Auto:
        default: return 0.0f;
    }
}

std::string collapseWhitespace(const std::string& s) {
    std::string out;
    bool inWs = false;
    for (char ch : s) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!inWs) { out.push_back(' '); inWs = true; }
        } else {
            out.push_back(ch);
            inWs = false;
        }
    }
    return out;
}

std::vector<std::pair<std::string, bool>> tokenizeText(const std::string& t) {
    std::vector<std::pair<std::string, bool>> out;
    size_t i = 0;
    while (i < t.size()) {
        unsigned char c = static_cast<unsigned char>(t[i]);
        if (std::isspace(c)) {
            out.emplace_back(" ", true);
            while (i < t.size() && std::isspace(static_cast<unsigned char>(t[i]))) i++;
            continue;
        }
        if (c >= 0xC0) {
            size_t len = c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : 2);
            if (i + len > t.size()) len = t.size() - i;
            out.emplace_back(t.substr(i, len), false);
            i += len;
            continue;
        }
        size_t j = i;
        while (j < t.size() && !std::isspace(static_cast<unsigned char>(t[j])) &&
               static_cast<unsigned char>(t[j]) < 0x80) {
            j++;
        }
        out.emplace_back(t.substr(i, j - i), false);
        i = j;
    }
    return out;
}

QFont makeFont(const ComputedStyle& s) {
    QFont f(QString::fromStdString(s.fontFamily));
    f.setPixelSize(std::max(1, static_cast<int>(std::round(s.fontSize))));
    f.setBold(s.fontWeight >= 600);
    f.setItalic(s.italic);
    return f;
}

struct FlatItem {
    LayoutNode* owner = nullptr;
    bool isText = false;
    bool isSpace = false;
    bool isBreak = false;
    bool isReplaced = false;
    std::string text;
    ComputedStyle style;
    float fixedW = 0.0f;
    float fixedH = 0.0f;
    std::vector<LayoutNode*> ancestors;
};

void flatten(const LayoutNode* node, std::vector<LayoutNode*>& ancestors,
             std::vector<FlatItem>& out) {
    if (!node) return;
    if (node->isText) {
        for (const auto& tok : tokenizeText(node->textContent)) {
            FlatItem it;
            it.owner = const_cast<LayoutNode*>(node);
            it.isText = true;
            it.isSpace = tok.second;
            it.text = tok.first;
            it.style = node->style;
            it.ancestors = ancestors;
            out.push_back(std::move(it));
        }
        return;
    }
    if (node->tagName == "br") {
        FlatItem it;
        it.owner = const_cast<LayoutNode*>(node);
        it.isBreak = true;
        it.style = node->style;
        it.ancestors = ancestors;
        out.push_back(std::move(it));
        return;
    }
    if (node->isReplaced) {
        FlatItem it;
        it.owner = const_cast<LayoutNode*>(node);
        it.isReplaced = true;
        it.style = node->style;
        it.fixedW = node->style.width.isAuto() ? 16.0f : node->style.width.value;
        it.fixedH = node->style.height.isAuto() ? 16.0f : node->style.height.value;
        it.ancestors = ancestors;
        out.push_back(std::move(it));
        return;
    }
    ancestors.push_back(const_cast<LayoutNode*>(node));
    for (const auto& child : node->children) {
        flatten(child.get(), ancestors, out);
    }
    ancestors.pop_back();
}

} // namespace

void LayoutEngine::setViewport(float width, float height) {
    m_viewportW = width > 0 ? width : 800.0f;
    m_viewportH = height > 0 ? height : 600.0f;
}

std::shared_ptr<LayoutNode> LayoutEngine::buildLayout(const DomNode* domRoot,
                                                      const StyleSheet& sheet) {
    const DomNode* renderRoot = domRoot;
    if (domRoot && domRoot->type == NodeType::Document) {
        std::vector<DomNode*> htmls;
        domRoot->getElementsByTagName("html", htmls);
        if (!htmls.empty()) renderRoot = htmls[0];
    }
    if (!renderRoot) return nullptr;

    ComputedStyle rootStyle;
    rootStyle.display = Display::Block;
    rootStyle.fontSize = 16.0f;
    rootStyle.color = QColor(0, 0, 0);

    auto root = buildTree(renderRoot, rootStyle, sheet);
    if (root) {
        layoutBlock(root, 0.0f, 0.0f, m_viewportW);
        m_documentHeight = root->borderBox.height();
    }
    return root;
}

std::shared_ptr<LayoutNode> LayoutEngine::buildTree(const DomNode* dom,
                                                    const ComputedStyle& parentStyle,
                                                    const StyleSheet& sheet) {
    if (!dom) return nullptr;
    if (dom->type == NodeType::Comment) return nullptr;

    if (dom->type == NodeType::Text || dom->type == NodeType::CData ||
        dom->type == NodeType::Whitespace) {
        std::string collapsed = collapseWhitespace(dom->text);
        if (collapsed.empty()) return nullptr;
        auto n = std::make_shared<LayoutNode>("#text");
        n->isText = true;
        n->textContent = collapsed;
        n->style = sheet.computeStyle(dom, parentStyle);
        n->display = Display::Inline;
        return n;
    }

    if (dom->type == NodeType::Document) {
        auto n = std::make_shared<LayoutNode>("#document");
        n->display = Display::Block;
        n->style = parentStyle;
        for (const auto& child : dom->children) {
            auto c = buildTree(child.get(), parentStyle, sheet);
            if (c) { c->parent = n.get(); n->children.push_back(c); }
        }
        return n;
    }

    if (dom->isElement() || dom->type == NodeType::Template) {
        ComputedStyle st = sheet.computeStyle(dom, parentStyle);
        if (st.display == Display::None) return nullptr;

        auto n = std::make_shared<LayoutNode>(dom->tag);
        n->display = st.display;
        n->isInlineElement = (st.display == Display::Inline ||
                              st.display == Display::InlineBlock);

        if (dom->tag == "img") {
            n->isReplaced = true;
            if (st.width.isAuto()) {
                if (const std::string* w = dom->getAttribute("width")) {
                    st.width = Length::px(std::strtof(w->c_str(), nullptr));
                }
            }
            if (st.height.isAuto()) {
                if (const std::string* h = dom->getAttribute("height")) {
                    st.height = Length::px(std::strtof(h->c_str(), nullptr));
                }
            }
            if (st.width.isAuto()) st.width = Length::px(16.0f);
            if (st.height.isAuto()) st.height = Length::px(16.0f);
        }

        n->style = st;
        for (const auto& child : dom->children) {
            auto c = buildTree(child.get(), st, sheet);
            if (c) { c->parent = n.get(); n->children.push_back(c); }
        }
        return n;
    }

    return nullptr;
}

float LayoutEngine::layoutBlock(const std::shared_ptr<LayoutNode>& node,
                                float x, float y, float cbWidth) {
    ComputedStyle& s = node->style;

    float mt = resolveLen(s.margin[0], cbWidth, m_viewportW, m_viewportH);
    float mr = resolveLen(s.margin[1], cbWidth, m_viewportW, m_viewportH);
    float mb = resolveLen(s.margin[2], cbWidth, m_viewportW, m_viewportH);
    float ml = resolveLen(s.margin[3], cbWidth, m_viewportW, m_viewportH);
    node->margin[0] = mt; node->margin[1] = mr; node->margin[2] = mb; node->margin[3] = ml;

    float bt = s.borderWidth[0], br = s.borderWidth[1];
    float bb = s.borderWidth[2], bl = s.borderWidth[3];
    node->border[0] = bt; node->border[1] = br; node->border[2] = bb; node->border[3] = bl;

    float pt = resolveLen(s.padding[0], cbWidth, m_viewportW, m_viewportH);
    float pr = resolveLen(s.padding[1], cbWidth, m_viewportW, m_viewportH);
    float pb = resolveLen(s.padding[2], cbWidth, m_viewportW, m_viewportH);
    float pl = resolveLen(s.padding[3], cbWidth, m_viewportW, m_viewportH);
    node->padding[0] = pt; node->padding[1] = pr; node->padding[2] = pb; node->padding[3] = pl;

    float contentWidth;
    if (!s.width.isAuto()) {
        contentWidth = resolveLen(s.width, cbWidth, m_viewportW, m_viewportH);
    } else {
        contentWidth = cbWidth - ml - mr - bl - br - pl - pr;
    }
    if (!s.maxWidth.isAuto()) {
        float mw = resolveLen(s.maxWidth, cbWidth, m_viewportW, m_viewportH);
        if (contentWidth > mw) contentWidth = mw;
    }
    if (contentWidth < 0.0f) contentWidth = 0.0f;

    float borderX = x + ml;
    float borderY = y + mt;
    float contentX = borderX + bl + pl;
    float contentY = borderY + bt + pt;

    float contentHeight = layoutChildren(node, contentX, contentY, contentWidth);

    if (!s.height.isAuto()) {
        float hh = resolveLen(s.height, cbWidth, m_viewportW, m_viewportH);
        if (hh > contentHeight) contentHeight = hh;
    }
    if (!s.minHeight.isAuto()) {
        float mh = resolveLen(s.minHeight, cbWidth, m_viewportW, m_viewportH);
        if (mh > contentHeight) contentHeight = mh;
    }

    node->contentBox = QRectF(contentX, contentY, contentWidth, contentHeight);
    node->paddingBox = QRectF(borderX + bl, borderY + bt,
                              contentWidth + pl + pr, contentHeight + pt + pb);
    node->borderBox = QRectF(borderX, borderY,
                             contentWidth + pl + pr + bl + br,
                             contentHeight + pt + pb + bt + bb);
    return mt + node->borderBox.height() + mb;
}

float LayoutEngine::layoutChildren(const std::shared_ptr<LayoutNode>& node,
                                   float contentX, float contentY, float contentWidth) {
    float y = contentY;
    std::vector<std::shared_ptr<LayoutNode>> inlineRun;

    auto flushRun = [&]() {
        if (!inlineRun.empty()) {
            y += layoutInlineRun(inlineRun, contentX, y, contentWidth, node->style.textAlign);
            inlineRun.clear();
        }
    };

    for (const auto& ch : node->children) {
        if (!ch) continue;
        if (ch->display == Display::Block || ch->display == Display::Flex) {
            flushRun();
            y += layoutBlock(ch, contentX, y, contentWidth);
        } else {
            inlineRun.push_back(ch);
        }
    }
    flushRun();
    return y - contentY;
}

float LayoutEngine::layoutInlineRun(const std::vector<std::shared_ptr<LayoutNode>>& run,
                                    float x0, float y0, float width, TextAlign align) {
    std::vector<FlatItem> items;
    std::vector<LayoutNode*> anc;
    for (const auto& n : run) {
        flatten(n.get(), anc, items);
    }
    if (items.empty()) return 0.0f;

    struct Buffered {
        LayoutNode* owner;
        bool isText;
        bool isReplaced;
        std::string text;
        float x;
        float w;
        float h;
        std::vector<LayoutNode*> anc;
    };

    float lineTop = y0;
    float bottom = y0;
    float x = x0;
    std::vector<Buffered> lineBuf;
    std::map<LayoutNode*, QRectF> inlineBounds;

    auto emitLine = [&]() {
        if (lineBuf.empty()) return;
        float maxH = 0.0f;
        for (const auto& b : lineBuf) maxH = std::max(maxH, b.h);

        float totalW = lineBuf.back().x + lineBuf.back().w - x0;
        float dx = 0.0f;
        if (align == TextAlign::Center) dx = (width - totalW) / 2.0f;
        else if (align == TextAlign::Right) dx = width - totalW;
        if (dx < 0.0f) dx = 0.0f;

        for (const auto& b : lineBuf) {
            float bx = b.x + dx;
            QRectF r(bx, lineTop, b.w, maxH);
            if (b.isText) {
                b.owner->textLines.push_back({r, b.text});
            } else if (b.isReplaced) {
                b.owner->borderBox = r;
                b.owner->contentBox = r;
                b.owner->paddingBox = r;
            }
            for (LayoutNode* a : b.anc) {
                auto it = inlineBounds.find(a);
                if (it == inlineBounds.end()) inlineBounds[a] = r;
                else it->second = it->second.united(r);
            }
        }
        bottom = lineTop + maxH;
        lineTop += maxH;
        lineBuf.clear();
        x = x0;
    };

    for (const auto& it : items) {
        if (it.isBreak) {
            emitLine();
            continue;
        }

        float w = 0.0f, h = 0.0f;
        if (it.isReplaced) {
            w = it.fixedW;
            h = it.fixedH;
        } else {
            QFont f = makeFont(it.style);
            QFontMetricsF fm(f);
            w = static_cast<float>(fm.horizontalAdvance(QString::fromStdString(it.text)));
            h = it.style.effectiveLineHeight();
        }

        if (it.isSpace && lineBuf.empty()) {
            continue;
        }
        if (!it.isSpace && !lineBuf.empty() && x + w > x0 + width) {
            emitLine();
        }

        lineBuf.push_back({it.owner, it.isText, it.isReplaced, it.text, x, w, h, it.ancestors});
        x += w;
    }
    emitLine();

    for (auto& kv : inlineBounds) {
        kv.first->borderBox = kv.second;
        kv.first->contentBox = kv.second;
        kv.first->paddingBox = kv.second;
    }

    return bottom - y0;
}

} // namespace TinyBrowser
