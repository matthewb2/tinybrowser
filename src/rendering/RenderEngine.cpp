#include "TinyBrowser/RenderEngine.h"
#include "TinyBrowser/LayoutNode.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QImage>
#include <QColor>
#include <QPen>

namespace TinyBrowser {

RenderEngine::RenderEngine()
    : m_width(0), m_height(0), m_dirty(true) {
}

RenderEngine::~RenderEngine() = default;

bool RenderEngine::initialize(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    ensureSurface(width, height);
    m_width = width;
    m_height = height;
    m_dirty = true;
    return true;
}

void RenderEngine::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (m_width == width && m_height == height) return;
    ensureSurface(width, height);
    m_width = width;
    m_height = height;
    m_dirty = true;
}

void RenderEngine::clear() {
    if (!m_image.isNull()) {
        m_image.fill(Qt::white);
    }
    m_dirty = true;
}

void RenderEngine::drawHello() {
    if (m_image.isNull()) return;

    QPainter p(&m_image);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.fillRect(0, 0, m_width, m_height, Qt::white);

    p.setPen(QPen(QColor(192, 192, 192), 2));
    p.drawRect(8, 8, m_width - 16, m_height - 16);

    p.setPen(QColor(0, 192, 0));
    QFont font("Sans Serif", 28);
    p.setFont(font);

    QFontMetrics fm(font);
    QString text = "Hello TinyBrowser";
    int tw = fm.horizontalAdvance(text);
    int x = (m_width - tw) / 2;
    int y = (m_height - fm.height()) / 2 + fm.ascent();
    p.drawText(x, y, text);

    m_dirty = true;
}

namespace {

QFont makeStyleFont(const ComputedStyle& s) {
    QFont f(QString::fromStdString(s.fontFamily));
    f.setPixelSize(std::max(1, static_cast<int>(std::round(s.fontSize))));
    f.setBold(s.fontWeight >= 600);
    f.setItalic(s.italic);
    return f;
}

} // namespace

void RenderEngine::drawLayoutNode(QPainter& painter, const std::shared_ptr<LayoutNode>& node) {
    if (!node) return;

    if (node->isText) {
        if (node->textLines.empty()) return;
        QFont font = makeStyleFont(node->style);
        painter.setFont(font);
        QFontMetricsF fm(font);
        for (const auto& line : node->textLines) {
            const QRectF& r = line.first;
            const std::string& txt = line.second;
            if (txt.empty()) continue;
            float baseline = static_cast<float>(r.top() + fm.ascent());
            painter.setPen(node->style.color);
            painter.drawText(QPointF(r.left(), baseline), QString::fromStdString(txt));
            if (node->style.underline) {
                float y = baseline + 1.0f;
                painter.drawLine(QPointF(r.left(), y),
                                 QPointF(r.left() + r.width(), y));
            }
        }
        return;
    }

    if (node->borderBox.isValid()) {
        if (node->style.background.alpha() > 0) {
            painter.fillRect(node->borderBox, node->style.background);
        }
        float bw = std::max(std::max(node->border[0], node->border[1]),
                            std::max(node->border[2], node->border[3]));
        if (bw > 0.0f) {
            painter.setPen(QPen(node->style.borderColor, bw));
            painter.setBrush(Qt::NoBrush);
            float inset = bw / 2.0f;
            painter.drawRect(node->borderBox.adjusted(inset, inset, -inset, -inset));
        }
    }

    for (const auto& child : node->children) {
        drawLayoutNode(painter, child);
    }
}


void RenderEngine::renderLayout(const std::shared_ptr<LayoutNode>& root, int width, int height) {
    ensureSurface(width, height);
    if (m_image.isNull()) return;

    QPainter painter(&m_image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(0, 0, width, height, Qt::white);

    if (root) {
        drawLayoutNode(painter, root);
    }

    m_dirty = true;
}

QImage RenderEngine::renderToQImage(int width, int height) {
    if (width <= 0 || height <= 0) return QImage();

    if (m_image.isNull()) {
        QImage fallback(width, height, QImage::Format_ARGB32_Premultiplied);
        fallback.fill(Qt::white);
        return fallback;
    }

    if (m_image.width() != width || m_image.height() != height) {
        return m_image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    return m_image.copy();
}

bool RenderEngine::isValid() const {
    return !m_image.isNull() && m_width > 0 && m_height > 0;
}

void RenderEngine::ensureSurface(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (!m_image.isNull() && m_width == width && m_height == height) return;

    m_image = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
    m_image.fill(Qt::white);
    m_width = width;
    m_height = height;
    m_dirty = true;
}

} // namespace TinyBrowser