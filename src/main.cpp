#include <v8.h>
#include <libplatform/libplatform.h> // V8 플랫폼 헤더 필수
#include "TinyBrowser/ScriptEngine.h"
#include <QApplication>
#include <QSurfaceFormat>

#include "TinyBrowser/MainWindow.h"

int main(int argc, char* argv[]) {
  // 1. V8 플랫폼 초기화 (가장 먼저 실행되어야 함)
    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();
    
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