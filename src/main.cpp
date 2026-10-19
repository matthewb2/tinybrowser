#include <QApplication>
#include <QSurfaceFormat>

#include "TinyBrowser/MainWindow.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  app.setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

  app.setApplicationName("TinyBrowser");
  app.setApplicationVersion("0.1.0");
  app.setOrganizationName("TinyBrowser");

  TinyBrowser::MainWindow window;
  window.resize(1024, 768);
  window.show();

  return app.exec();
}