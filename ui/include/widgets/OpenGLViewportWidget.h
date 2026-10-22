#pragma once

#include <atomic>
#include <memory>

#include <QImage>
#include <QWidget>

#include "Engine.h"
#include "state/RenderStatistics.h"

class QTSceneInspector;
class QTEngineSettingsAdapter;
class QtGraphicsContextProvider;

/**
 * @brief The single concrete scene viewport widget.
 *
 * Owns the Engine instance, which renders on its own thread into an offscreen screen; this
 *  widget only displays the CPU frames the engine publishes (it makes no OpenGL calls), and
 *  forwards input and its size to the engine. Also records render statistics.
 */
class OpenGLViewportWidget : public QWidget
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

  // Starts the engine thread, creates the viewport's screen and attaches the adapters. Call once,
  //  on the GUI thread, after the event loop is running (the offscreen surface needs it).
  void StartEngine();

  // Detaches the adapters, destroys the screen and stops the engine thread. Idempotent; must run
  //  before the window's widgets are torn down.
  void ShutdownEngine();

signals:
  void renderStatisticsChanged();
  void engineInitialized();
  // Emitted (synchronously) at the start of ShutdownEngine(), so holders of the engine's
  //  services outside this widget can detach before the engine stops.
  void engineShuttingDown();

protected:
  bool event(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

  // Attaches the adapters to the running engine's services.
  void attachAdapters();

  // Scene management
  std::unique_ptr<Engine> engine;

private:
  // Takes the newest frame from the engine (GUI thread) and schedules a repaint.
  void pullLatestFrame();
  void updateScreenSize();

  std::shared_ptr<QtGraphicsContextProvider> graphicsContextProvider;
  ScreenId screenId = kInvalidScreenId;

  // Set by the engine thread when a pullLatestFrame() is queued, cleared by it on the GUI thread,
  //  so at most one pull is ever pending however fast the engine publishes.
  std::atomic<bool> framePullPending{false};
  // frameImage wraps latestFrame.pixels without copying, so it must be rebuilt every time
  //  GetScreenFrame() swaps that buffer.
  ScreenFrame latestFrame;
  QImage frameImage;

  std::unique_ptr<RenderStatistics> renderStatisticsObject; // Owned by this widget; exposes rendering metrics for external display
  std::unique_ptr<QTSceneInspector> inspectAdapterObject; // Owned by this widget; exposes the scene's inspectable objects to Qt widgets
  std::unique_ptr<QTEngineSettingsAdapter> settingsAdapterObject; // Owned by this widget; exposes engine-wide settings to Qt widgets
  glm::vec3 fillColor{0.0f};
};
