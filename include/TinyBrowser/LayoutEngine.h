#pragma once

#include <memory>
#include <QRectF>

#include "TinyBrowser/LayoutNode.h"
#include "TinyBrowser/StyleSheet.h"

namespace TinyBrowser {

class LayoutEngine {
public:
    LayoutEngine() = default;

    void setViewport(float width, float height);

    // Build a styled layout tree from a DOM document and compute geometry.
    std::shared_ptr<LayoutNode> buildLayout(const DomNode* domRoot,
                                            const StyleSheet& sheet);

    float documentHeight() const { return m_documentHeight; }

private:
    std::shared_ptr<LayoutNode> buildTree(const DomNode* dom,
                                          const ComputedStyle& parentStyle,
                                          const StyleSheet& sheet);

    float layoutBlock(const std::shared_ptr<LayoutNode>& node,
                      float x, float y, float containingWidth);

    float layoutChildren(const std::shared_ptr<LayoutNode>& node,
                         float contentX, float contentY, float contentWidth);

    float layoutInlineRun(const std::vector<std::shared_ptr<LayoutNode>>& run,
                          float x, float y, float width, TextAlign align);

    float m_viewportW = 800.0f;
    float m_viewportH = 600.0f;
    float m_documentHeight = 0.0f;
};

} // namespace TinyBrowser
