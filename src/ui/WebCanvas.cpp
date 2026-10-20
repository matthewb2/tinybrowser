#include "TinyBrowser/WebCanvas.h"
#include "TinyBrowser/LayoutEngine.h"

#include <QEvent>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QWheelEvent>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace TinyBrowser {
namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string dirName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

std::string hitTestLink(const std::shared_ptr<LayoutNode>& node, float x, float y) {
    if (!node) return {};
    if (node->borderBox.isValid() && !node->borderBox.contains(x, y)) return {};
    for (const auto& child : node->children) {
        std::string r = hitTestLink(child, x, y);
        if (!r.empty()) return r;
    }
    if (!node->href.empty() && node->borderBox.isValid() &&
        node->borderBox.contains(x, y)) {
        return node->href;
    }
    return {};
}

} // namespace

WebCanvas::WebCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);

    m_vScroll = new QScrollBar(Qt::Vertical, this);
    m_vScroll->setSingleStep(30);
    m_vScroll->setVisible(false);
    connect(m_vScroll, &QScrollBar::valueChanged, this, [this](int v) {
        m_scrollY = v;
        update();
    });

    m_renderEngine.initialize(800, 600);
    loadFile("resources/index.html");
}

void WebCanvas::loadFile(const std::string& filePath) {
    std::string html = readFile(filePath);
    if (html.empty()) {
        std::cerr << "[WebCanvas] Cannot open: " << filePath << std::endl;
        m_renderEngine.drawHello();
        return;
    }
    m_baseDir = dirName(filePath);

    DomNode* dom = m_parser.parse(html);
    if (!dom) {
        std::cerr << "[WebCanvas] HTML parse failed" << std::endl;
        m_renderEngine.drawHello();
        return;
    }

    std::string css;

    std::vector<DomNode*> styleNodes;
    dom->getElementsByTagName("style", styleNodes);
    for (DomNode* s : styleNodes) {
        css += s->innerText();
        css += "\n";
    }

    std::vector<DomNode*> linkNodes;
    dom->getElementsByTagName("link", linkNodes);
    for (DomNode* l : linkNodes) {
        const std::string* rel = l->getAttribute("rel");
        const std::string* href = l->getAttribute("href");
        if (!rel || !href) continue;
        if (toLower(*rel).find("stylesheet") == std::string::npos) continue;
        std::string full = m_baseDir + "/" + *href;
        std::string fileCss = readFile(full);
        if (fileCss.empty()) {
            std::cerr << "[WebCanvas] Cannot read stylesheet: " << full << std::endl;
        } else {
            css += fileCss;
            css += "\n";
        }
    }

    m_styleSheet = StyleSheet();
    m_styleSheet.parse(css);
    std::cout << "[WebCanvas] Loaded " << m_styleSheet.ruleCount()
              << " CSS rules from " << filePath << std::endl;

    int w = width() > 1 ? width() : 800;
    int h = height() > 1 ? height() : 600;
    relayout(w, h);
}

void WebCanvas::relayout(int width, int height) {
    if (width <= 0 || height <= 0) return;

    m_styleSheet.setViewport(static_cast<float>(width), static_cast<float>(height));

    LayoutEngine engine;
    engine.setViewport(static_cast<float>(width), static_cast<float>(height));
    m_root = engine.buildLayout(m_parser.root(), m_styleSheet);

    int docHeight = static_cast<int>(std::ceil(engine.documentHeight()));
    if (docHeight < height) docHeight = height;
    m_docHeight = docHeight;

    if (m_root) {
        m_renderEngine.renderLayout(m_root, width, docHeight);
    } else {
        m_renderEngine.drawHello();
    }

    if (m_vScroll) {
        int maxScroll = std::max(0, m_docHeight - height);
        m_vScroll->setRange(0, maxScroll);
        m_vScroll->setPageStep(height);
        m_vScroll->setVisible(maxScroll > 0);
        if (m_scrollY > maxScroll) m_scrollY = maxScroll;
        if (m_scrollY < 0) m_scrollY = 0;
        m_vScroll->setValue(m_scrollY);
    }

    m_lastWidth = width;
    m_lastHeight = height;
}

std::string WebCanvas::hitTestLink(float docX, float docY) const {
    return TinyBrowser::hitTestLink(m_root, docX, docY);
}

void WebCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    const QImage& img = m_renderEngine.image();
    if (!img.isNull()) {
        painter.drawImage(0, -m_scrollY, img);
    }
}

void WebCanvas::resizeEvent(QResizeEvent* event) {
    const QSize sz = event->size();

    int sbW = m_vScroll ? m_vScroll->sizeHint().width() : 0;
    int viewportW = std::max(1, sz.width() - sbW);

    if (m_vScroll) {
        m_vScroll->setGeometry(sz.width() - sbW, 0, sbW, sz.height());
    }

    relayout(viewportW, sz.height());

    QWidget::resizeEvent(event);
    update();
}

void WebCanvas::mouseMoveEvent(QMouseEvent* event) {
    float docX = static_cast<float>(event->position().x());
    float docY = static_cast<float>(event->position().y() + m_scrollY);

    std::string href = hitTestLink(docX, docY);
    QString url = QString::fromStdString(href);

    if (url != m_lastHoverUrl) {
        m_lastHoverUrl = url;
        emit hoverUrlChanged(url);
        setCursor(href.empty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    }
    QWidget::mouseMoveEvent(event);
}

void WebCanvas::leaveEvent(QEvent* event) {
    if (!m_lastHoverUrl.isEmpty()) {
        m_lastHoverUrl.clear();
        emit hoverUrlChanged(QString());
        setCursor(Qt::ArrowCursor);
    }
    QWidget::leaveEvent(event);
}

void WebCanvas::wheelEvent(QWheelEvent* event) {
    if (m_vScroll && m_vScroll->isVisible()) {
        int steps = event->angleDelta().y() / 120;
        m_vScroll->setValue(m_vScroll->value() - steps * m_vScroll->singleStep());
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

} // namespace TinyBrowser
