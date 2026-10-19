#pragma once

#include <string>
#include <QColor>

namespace TinyBrowser {

enum class Display {
  None,
  Block,
  Inline,
  InlineBlock,
  Flex
};

enum class TextAlign {
  Left,
  Center,
  Right
};

struct Length {
  enum class Unit { Auto, Px, Percent, Vh, Vw };

  float value = 0.0f;
  Unit unit = Unit::Auto;

  bool isAuto() const { return unit == Unit::Auto; }

  static Length autoLen() { return Length{0.0f, Unit::Auto}; }
  static Length px(float v) { return Length{v, Unit::Px}; }
  static Length percent(float v) { return Length{v, Unit::Percent}; }
};

struct ComputedStyle {
  Display display = Display::Inline;

  Length margin[4];
  Length padding[4];
  float borderWidth[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  QColor borderColor = QColor(0, 0, 0);

  Length width;
  Length height;
  Length maxWidth;
  Length minHeight;

  float fontSize = 16.0f;
  int fontWeight = 400;
  bool italic = false;
  std::string fontFamily = "Arial";

  QColor color = QColor(0, 0, 0);
  QColor background = QColor(Qt::transparent);

  TextAlign textAlign = TextAlign::Left;

  float lineHeightPx = 0.0f;
  bool underline = false;

  float effectiveLineHeight() const {
    return lineHeightPx > 0.0f ? lineHeightPx : fontSize * 1.2f;
  }
};

} // namespace TinyBrowser
