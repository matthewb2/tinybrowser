#pragma once

#include <QImage>
#include <memory>
#include "TinyBrowser/LayoutNode.h"

class QPainter;

namespace TinyBrowser {

class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();

    bool initialize(int width, int height);
    void resize(int width, int height);
    void clear();
    void drawHello();
    void renderLayout(const std::shared_ptr<LayoutNode>& root, int width, int height);
    QImage renderToQImage(int width, int height);
    const QImage& image() const { return m_image; }
    bool isValid() const;

private:
    void ensureSurface(int width, int height);
    void drawLayoutNode(QPainter& painter, const std::shared_ptr<LayoutNode>& node);

    int m_width = 0;
    int m_height = 0;
    bool m_dirty = true;
    QImage m_image;
};

} // namespace TinyBrowser