#include "TinyBrowser/MainWindow.h"
#include "TinyBrowser/WebCanvas.h"

#include <QAction>
#include <QLineEdit>
#include <QToolBar>
#include <QWidget>
#include <QVBoxLayout>

namespace TinyBrowser {

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent) {
  setupActions();
  setupUi();
  setWindowTitle("TinyBrowser");
}

void MainWindow::setupActions() {
  m_actionBack = new QAction(tr("Back"), this);
  m_actionBack->setShortcut(QKeySequence::Back);
  m_actionBack->setEnabled(false);

  m_actionForward = new QAction(tr("Forward"), this);
  m_actionForward->setShortcut(QKeySequence::Forward);
  m_actionForward->setEnabled(false);

  m_actionReload = new QAction(tr("Reload"), this);
  m_actionReload->setShortcut(QKeySequence::Refresh);

  m_actionStop = new QAction(tr("Stop"), this);
  m_actionStop->setShortcut(QKeySequence::Cancel);
  m_actionStop->setEnabled(false);

  m_actionGo = new QAction(tr("Go"), this);
  m_actionGo->setShortcut(Qt::Key_Return);

  connect(m_actionBack, &QAction::triggered, this, &MainWindow::onBack);
  connect(m_actionForward, &QAction::triggered, this, &MainWindow::onForward);
  connect(m_actionReload, &QAction::triggered, this, &MainWindow::onReload);
  connect(m_actionStop, &QAction::triggered, this, &MainWindow::onStop);
  connect(m_actionGo, &QAction::triggered, this, &MainWindow::onGo);
}

void MainWindow::setupUi() {
  m_toolbar = addToolBar(tr("Navigation"));
  m_toolbar->setMovable(false);

  m_toolbar->addAction(m_actionBack);
  m_toolbar->addAction(m_actionForward);
  m_toolbar->addSeparator();
  m_toolbar->addAction(m_actionReload);
  m_toolbar->addAction(m_actionStop);
  m_toolbar->addSeparator();

  m_addressBar = new QLineEdit(this);
  m_addressBar->setPlaceholderText(tr("Enter URL..."));
  m_addressBar->setClearButtonEnabled(true);
  m_toolbar->addWidget(m_addressBar);

  m_toolbar->addAction(m_actionGo);

  connect(m_addressBar, &QLineEdit::returnPressed, this, &MainWindow::onGo);

  m_webCanvas = new WebCanvas(this);
  setCentralWidget(m_webCanvas);
}

void MainWindow::onGo() {
}

void MainWindow::onReload() {
  if (m_webCanvas) {
    m_webCanvas->update();
  }
}

void MainWindow::onStop() {
}

void MainWindow::onBack() {
}

void MainWindow::onForward() {
}

} // namespace TinyBrowser