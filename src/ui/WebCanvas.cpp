#include "TinyBrowser/WebCanvas.h"
#include "TinyBrowser/LayoutEngine.h"

#include <QFileInfo>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSize>
#include <QString>

#include <algorithm>
#include <cctype>
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

} // namespace

WebCanvas::WebCanvas(QWidget* parent) : QWidget(parent) {
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

    if (m_root) {
        m_renderEngine.renderLayout(m_root, width, height);
    } else {
        m_renderEngine.drawHello();
    }
    m_lastWidth = width;
    m_lastHeight = height;
}

void WebCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    QImage img = m_renderEngine.renderToQImage(width(), height());
    painter.drawImage(0, 0, img);
}

void WebCanvas::resizeEvent(QResizeEvent* event) {
    const QSize sz = event->size();
    m_renderEngine.resize(sz.width(), sz.height());
    if (sz.width() != m_lastWidth || sz.height() != m_lastHeight) {
        relayout(sz.width(), sz.height());
    }
    QWidget::resizeEvent(event);
    update();
}

} // namespace TinyBrowser
