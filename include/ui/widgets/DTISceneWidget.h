#pragma once

#include <atomic>
#include <memory>

#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickFramebufferObject>
#include <QTimer>
#include <QWheelEvent>
#include <QString>

#include "ui/qt-adapters/QtInspectionMovement.h"

class DtiVolumeScene;
class QOpenGLFramebufferObject;
class QSize;

class DTISceneWidget : public QQuickFramebufferObject
{
  // This class represents a QML item that displays a 3D scene using OpenGL. 
  // It allows setting paths for DWI, bval, and bvec files, and it handles user input for camera movement through a QtInspectionMovement proxy.
  Q_OBJECT
  Q_PROPERTY(QString dwiPath READ dwiPath WRITE setDwiPath NOTIFY dwiPathChanged)
  Q_PROPERTY(QString bvalPath READ bvalPath WRITE setBvalPath NOTIFY bvalPathChanged)
  Q_PROPERTY(QString bvecPath READ bvecPath WRITE setBvecPath NOTIFY bvecPathChanged)

public:
  explicit DTISceneWidget(QQuickItem *parent = nullptr);
  ~DTISceneWidget() override;

  QQuickFramebufferObject::Renderer *createRenderer() const override;

  QString dwiPath() const;
  QString bvalPath() const;
  QString bvecPath() const;

  void setDwiPath(const QString &path);
  void setBvalPath(const QString &path);
  void setBvecPath(const QString &path);

  void setMovementProxy(QtInspectionMovement *movement);

signals:
  void dwiPathChanged();
  void bvalPathChanged();
  void bvecPathChanged();

protected:
  void componentComplete() override;
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  class Renderer : public QQuickFramebufferObject::Renderer
  {
  public:
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
    void synchronize(QQuickFramebufferObject *item) override;
    void render() override;

  private:
    void initializeScene();

    DTISceneWidget *viewportItem = nullptr;
    std::unique_ptr<DtiVolumeScene> scene;
    QtInspectionMovement *movement = nullptr;
    QElapsedTimer elapsedTimer;
    qint64 lastFrameTimeNs = 0;
  };

  QString dwiPathValue;
  QString bvalPathValue;
  QString bvecPathValue;

  std::atomic<QtInspectionMovement *> movementProxy{nullptr};
  QTimer frameTimer;
};
