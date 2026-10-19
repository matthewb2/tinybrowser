#pragma once

#include <QWidget>
#include <memory>
#include <string>

#include "TinyBrowser/RenderEngine.h"
#include "TinyBrowser/HtmlParser.h"
#include "TinyBrowser/StyleSheet.h"
#include "TinyBrowser/LayoutNode.h"

namespace TinyBrowser {

class WebCanvas : public QWidget {
  Q_OBJECT

public:
  explicit WebCanvas(QWidget* parent = nullptr);
  ~WebCanvas() override = default;

  void loadFile(const std::string& filePath);

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  void relayout(int width, int height);

  RenderEngine m_renderEngine;
  HtmlParser m_parser;
  StyleSheet m_styleSheet;
  std::shared_ptr<LayoutNode> m_root;
  std::string m_baseDir;
  int m_lastWidth = 0;
  int m_lastHeight = 0;
};

} // namespace TinyBrowser
