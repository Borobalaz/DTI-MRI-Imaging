#pragma once

#include <glad/glad.h>

#include <memory>

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QTimer>

#include "Engine.h"
#include "state/RenderStatistics.h"

class QTSceneInspector;

/**
 * @brief Base class for OpenGL-based scene rendering viewports.
 * 
 * Provides core functionality for rendering a 3D scene in a Qt OpenGL context,
 * including input handling, frame timing, and render statistics.
 * 
 * Subclasses should override initializeScene() to set up their specific scene type.
 */
class OpenGLViewportWidget : public QOpenGLWidget
{
  Q_OBJECT
  Q_PROPERTY(RenderStatistics *renderStatistics READ renderStatistics NOTIFY renderStatisticsChanged)

public:
  OpenGLViewportWidget(QWidget *parent = nullptr);
  ~OpenGLViewportWidget() override;

  RenderStatistics *renderStatistics() const;
  QTSceneInspector &inspectAdapter() const;
  void SetFillColor(const glm::vec3& color);

signals:
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

  /**
   * @brief Initialize the 3D scene. Called once after OpenGL context is ready.
   * Subclasses should override this to set up their specific scene.
   */
  void initializeScene();

  // Scene management
  std::unique_ptr<Engine> engine;

private:
  std::unique_ptr<RenderStatistics> renderStatisticsObject; // Owned by this widget; exposes rendering metrics for external display
  std::unique_ptr<QTSceneInspector> inspectAdapterObject; // Owned by this widget; exposes the scene's inspectable objects to Qt widgets
  QTimer frameTimer;
  QElapsedTimer elapsedTimer;
  qint64 lastFrameTimeNs = 0;
  glm::vec3 fillColor{0.0f};
};
