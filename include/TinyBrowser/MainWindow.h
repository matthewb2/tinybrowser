#pragma once

#include <QMainWindow>

class QLineEdit;
class QToolBar;
class QAction;

namespace TinyBrowser {

class WebCanvas;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override = default;

private slots:
  void onGo();
  void onReload();
  void onStop();
  void onBack();
  void onForward();

private:
  void setupUi();
  void setupActions();

  QToolBar* m_toolbar = nullptr;
  QLineEdit* m_addressBar = nullptr;
  WebCanvas* m_webCanvas = nullptr;

  QAction* m_actionBack = nullptr;
  QAction* m_actionForward = nullptr;
  QAction* m_actionReload = nullptr;
  QAction* m_actionStop = nullptr;
  QAction* m_actionGo = nullptr;
};

} // namespace TinyBrowser