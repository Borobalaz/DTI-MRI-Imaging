#pragma once

#include <glad/glad.h>

#include <memory>

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QTimer>

#include "Engine.h"
#include "state/RenderStatistics.h"

class QTSceneInspector;
class QTEngineSettingsAdapter;

/**
 * @brief The single concrete OpenGL scene viewport widget.
 *
 * Owns the Engine instance and renders a 3D scene in a Qt OpenGL context,
 * including input handling, frame timing, and render statistics.
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
  QTEngineSettingsAdapter &settingsAdapter() const;
  ILogService *logService() const;
  void SetFillColor(const glm::vec3& color);

signals:
  void renderStatisticsChanged();
  void engineInitialized();

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

  // Creates the engine's default scene. Called once after the OpenGL context is ready.
  void initializeScene();

  // Scene management
  std::unique_ptr<Engine> engine;

private:
  std::unique_ptr<RenderStatistics> renderStatisticsObject; // Owned by this widget; exposes rendering metrics for external display
  std::unique_ptr<QTSceneInspector> inspectAdapterObject; // Owned by this widget; exposes the scene's inspectable objects to Qt widgets
  std::unique_ptr<QTEngineSettingsAdapter> settingsAdapterObject; // Owned by this widget; exposes engine-wide settings to Qt widgets
  QTimer frameTimer;
  QElapsedTimer elapsedTimer;
  qint64 lastFrameTimeNs = 0;
  glm::vec3 fillColor{0.0f};
};
