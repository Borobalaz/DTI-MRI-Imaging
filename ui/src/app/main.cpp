#include <QApplication>
#include <QStyleFactory>
#include <QSurfaceFormat>

#include "windows/WidgetsMainWindow.h"

int main(int argc, char *argv[])
{
  QCoreApplication::setOrganizationName("DTI-MRI-Imaging");
  QCoreApplication::setApplicationName("Engine");

  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

  // Set up the default OpenGL surface format for the application.
  QSurfaceFormat format;
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setSwapInterval(0);  // disable vsync for lowest possible latency
  QSurfaceFormat::setDefaultFormat(format);

  // Use Fusion rather than the native Windows style: the native style ("windowsvista"/
  //  "Windows11") partially native-themes certain controls (e.g. QDockWidget titles) and
  //  ignores QSS/palette color overrides for them, which broke theme toggling. Fusion fully
  //  respects QSS, so the custom dark/light themes apply consistently everywhere.
  QApplication::setStyle(QStyleFactory::create("Fusion"));

  // Declare app
  QApplication app(argc, argv);

  // Create and show the main widgets window
  WidgetsMainWindow window;
  window.show();

  // Start the application event loop
  return app.exec();
}
