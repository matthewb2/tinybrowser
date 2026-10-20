#pragma once

#include <QString>
#include <QWidget>
#include <memory>
#include <string>

#include "TinyBrowser/RenderEngine.h"
#include "TinyBrowser/HtmlParser.h"
#include "TinyBrowser/StyleSheet.h"
#include "TinyBrowser/LayoutNode.h"

class QScrollBar;

namespace TinyBrowser {

class WebCanvas : public QWidget {
  Q_OBJECT

public:
  explicit WebCanvas(QWidget* parent = nullptr);
  ~WebCanvas() override = default;

  void loadFile(const std::string& filePath);

signals:
  void hoverUrlChanged(const QString& url);

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  void relayout(int width, int height);
  std::string hitTestLink(float docX, float docY) const;

  RenderEngine m_renderEngine;
  HtmlParser m_parser;
  StyleSheet m_styleSheet;
  std::shared_ptr<LayoutNode> m_root;
  std::string m_baseDir;

  QScrollBar* m_vScroll = nullptr;
  int m_scrollY = 0;
  int m_docHeight = 0;
  int m_lastWidth = 0;
  int m_lastHeight = 0;
  QString m_lastHoverUrl;
};

} // namespace TinyBrowser
