#include <QGuiApplication>
#include <QUrl>
#include <QQuickView>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <QtQml/qqml.h>

#include "ui/widgets/DTISceneWidget.h"

int main(int argc, char *argv[])
{
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

  // Set up the default OpenGL surface format for the application. 
  // This will be used by all QQuickWindows created in this application.
  QSurfaceFormat format;
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);

  // Declare app
  QGuiApplication app(argc, argv);

  // Register widgets to be used in QML
  qmlRegisterType<DTISceneWidget>("ConnectomicsImaging", 1, 0, "SceneViewport");

  // Create and show the main QML view
  QQuickView view;
  view.setResizeMode(QQuickView::SizeRootObjectToView);
  view.setSource(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
  view.setTitle("DTI Visualizer");
  view.setWidth(1600);
  view.setHeight(900);
  view.show();

  // Check if the QML view loaded successfully
  if (view.status() == QQuickView::Error)
  {
    return 1;
  }

  // Start the application event loop
  return app.exec();
}
