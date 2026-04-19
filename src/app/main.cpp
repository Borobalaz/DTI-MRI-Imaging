#include <QApplication>
#include <QSurfaceFormat>

#include "ui/windows/WidgetsMainWindow.h"

int main(int argc, char *argv[])
{
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

  // Set up the default OpenGL surface format for the application.
  QSurfaceFormat format;
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setSwapInterval(0);  // disable vsync for lowest possible latency
  QSurfaceFormat::setDefaultFormat(format);

  // Declare app
  QApplication app(argc, argv);

  // Create and show the main widgets window
  WidgetsMainWindow window;
  window.show();

  // Start the application event loop
  return app.exec();
}
