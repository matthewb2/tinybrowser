#pragma once

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <QRectF>

#include "TinyBrowser/ComputedStyle.h"

namespace TinyBrowser {

class LayoutNode {
public:
    std::string tagName;
    std::string textContent;
    Display display = Display::Block;
    ComputedStyle style;

    bool isText = false;
    bool isInlineElement = false;
    bool isReplaced = false;

    QRectF borderBox;
    QRectF contentBox;
    QRectF paddingBox;

    float margin[4] = {0, 0, 0, 0};
    float padding[4] = {0, 0, 0, 0};
    float border[4] = {0, 0, 0, 0};

    std::vector<std::pair<QRectF, std::string>> textLines;

    LayoutNode* parent = nullptr;
    std::vector<std::shared_ptr<LayoutNode>> children;

    LayoutNode() = default;
    explicit LayoutNode(const std::string& tag) : tagName(tag) {}
};

} // namespace TinyBrowser
