#pragma once

#include <glad/glad.h>

#include <memory>

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QTimer>

#include "ui/widgets/RenderStatistics.h"

class DtiVolumeScene;
class QTSceneInspector;
class QtInspectionMovement;
class Scene;

class DTIViewportWidget : public QOpenGLWidget
{
  Q_OBJECT
  Q_PROPERTY(RenderStatistics *renderStatistics READ renderStatistics NOTIFY renderStatisticsChanged)

public:
  explicit DTIViewportWidget(QWidget *parent = nullptr);
  ~DTIViewportWidget() override;

  QString dwiPath() const;
  QString bvalPath() const;
  QString bvecPath() const;
  RenderStatistics *renderStatistics() const;
  QTSceneInspector &inspectAdapter() const;

  void setDwiPath(const QString &path);
  void setBvalPath(const QString &path);
  void setBvecPath(const QString &path);

signals:
  void dwiPathChanged();
  void bvalPathChanged();
  void bvecPathChanged();
  void renderStatisticsChanged();

protected:
  void initializeGL() override;
  void resizeGL(int width, int height) override;
  void paintGL() override;

  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  void initializeScene();

  QString dwiPathValue;
  QString bvalPathValue;
  QString bvecPathValue;

  std::unique_ptr<DtiVolumeScene> scene;
  QtInspectionMovement *movement = nullptr;
  RenderStatistics *renderStatisticsObject = nullptr;
  QTSceneInspector *inspectAdapterObject = nullptr;

  QTimer frameTimer;
  QElapsedTimer elapsedTimer;
  qint64 lastFrameTimeNs = 0;
};
