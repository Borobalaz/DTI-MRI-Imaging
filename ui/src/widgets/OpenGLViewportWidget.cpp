#include "widgets/OpenGLViewportWidget.h"

#include <chrono>
#include <memory>

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QDebug>
#include <QEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QtGlobal>

#include "qt-adapters/QTSceneInspector.h"
#include "qt-adapters/QTEngineSettingsAdapter.h"
#include "qt-adapters/QTSceneDashboardAdapter.h"
#include "rendering/QtGraphicsContextProvider.h"
#include "state/RenderStatistics.h"

OpenGLViewportWidget::OpenGLViewportWidget(QWidget *parent)
  : QWidget(parent)
{
  renderStatisticsObject = std::make_unique<RenderStatistics>();
  inspectAdapterObject = std::make_unique<QTSceneInspector>();
  settingsAdapterObject = std::make_unique<QTEngineSettingsAdapter>();
  sceneDashboardAdapterObject = std::make_unique<QTSceneDashboardAdapter>();

  QObject::connect(renderStatisticsObject.get(), &RenderStatistics::statisticsChanged, this, [this]()
  {
    emit renderStatisticsChanged();
  });

  // paintEvent always covers the whole widget (frame or fill color), so Qt needn't erase it first.
  setAttribute(Qt::WA_OpaquePaintEvent);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
}

// Members are destroyed in reverse declaration order, which would destroy the adapters while the
//  engine thread could still reach them; shutting down explicitly first avoids that.
OpenGLViewportWidget::~OpenGLViewportWidget()
{
  ShutdownEngine();
}

RenderStatistics *OpenGLViewportWidget::renderStatistics() const
{
  return renderStatisticsObject.get();
}

// Exposes the inspectable objects in the scene to Qt widgets.
QTSceneInspector &OpenGLViewportWidget::inspectAdapter() const
{
  Q_ASSERT(inspectAdapterObject != nullptr);
  return *inspectAdapterObject;
}

// Exposes the engine's settings (ISettingsService) to Qt widgets.
QTEngineSettingsAdapter &OpenGLViewportWidget::settingsAdapter() const
{
  Q_ASSERT(settingsAdapterObject != nullptr);
  return *settingsAdapterObject;
}

// Exposes the active scene's dashboard fields (ISceneDashboardService) to Qt widgets.
QTSceneDashboardAdapter &OpenGLViewportWidget::sceneDashboardAdapter() const
{
  Q_ASSERT(sceneDashboardAdapterObject != nullptr);
  return *sceneDashboardAdapterObject;
}

// Exposes the engine's log service to Qt widgets. Returns nullptr while the engine isn't running.
ILogService *OpenGLViewportWidget::logService() const
{
  return engine ? &engine->GetLogService() : nullptr;
}

void OpenGLViewportWidget::SetFillColor(const glm::vec3& color)
{
  fillColor = color;
  if (engine)
  {
    engine->SetFillColor(fillColor);
  }
  update();
}

void OpenGLViewportWidget::StartEngine()
{
  if (engine)
  {
    return;
  }

  graphicsContextProvider = std::make_shared<QtGraphicsContextProvider>();
  engine = std::make_unique<Engine>();

  // Runs on the engine thread: only queue the pull, coalescing bursts into a single pending call.
  engine->OnFrameReady([this](ScreenId)
  {
    if (!framePullPending.exchange(true))
    {
      QMetaObject::invokeMethod(this, [this]() { pullLatestFrame(); }, Qt::QueuedConnection);
    }
  });

  if (!engine->Start(graphicsContextProvider))
  {
    qWarning() << "OpenGLViewportWidget: failed to start the engine.";
    engine.reset();
    graphicsContextProvider.reset();
    return;
  }

  engine->SetFillColor(fillColor);
  screenId = engine->CreateScreen();
  updateScreenSize();

  attachAdapters();
}

void OpenGLViewportWidget::attachAdapters()
{
  if (inspectAdapterObject)
  {
    inspectAdapterObject->SetInspectionService(&engine->GetInspectionService());
  }

  if (settingsAdapterObject)
  {
    settingsAdapterObject->SetSettingsService(&engine->GetSettingsService());
  }

  if (sceneDashboardAdapterObject)
  {
    sceneDashboardAdapterObject->SetSceneDashboardService(&engine->GetSceneDashboardService());
  }

  emit engineInitialized();
}

void OpenGLViewportWidget::ShutdownEngine()
{
  if (!engine)
  {
    return;
  }

  if (inspectAdapterObject)
  {
    inspectAdapterObject->StopSyncTimer();
  }
  if (settingsAdapterObject)
  {
    settingsAdapterObject->StopSyncTimer();
  }
  if (sceneDashboardAdapterObject)
  {
    sceneDashboardAdapterObject->StopSyncTimer();
  }

  if (inspectAdapterObject)
  {
    inspectAdapterObject->SetInspectionService(nullptr);
  }
  if (settingsAdapterObject)
  {
    settingsAdapterObject->SetSettingsService(nullptr);
  }
  if (sceneDashboardAdapterObject)
  {
    sceneDashboardAdapterObject->SetSceneDashboardService(nullptr);
  }
  // The log widget lives outside this widget; its owner detaches it from this signal.
  emit engineShuttingDown();

  engine->DestroyScreen(screenId);
  screenId = kInvalidScreenId;

  engine->Stop();
  engine.reset();

  // Destroys the offscreen surface on the GUI thread, now that the engine thread is joined.
  graphicsContextProvider.reset();

  frameImage = QImage();
  latestFrame = ScreenFrame{};
}

void OpenGLViewportWidget::pullLatestFrame()
{
  framePullPending.store(false);

  if (!engine || screenId == kInvalidScreenId)
  {
    return;
  }

  if (!engine->GetScreenFrame(screenId, latestFrame))
  {
    return;
  }

  // The swap handed the previous pixel buffer back to the engine, so the image must stop
  //  referencing it right away. Frames are top-down already; RGBX ignores the alpha channel.
  const std::size_t expectedBytes = static_cast<std::size_t>(latestFrame.width) * static_cast<std::size_t>(latestFrame.height) * 4u;
  if (latestFrame.width <= 0 || latestFrame.height <= 0 || latestFrame.pixels.size() < expectedBytes)
  {
    frameImage = QImage();
    return;
  }

  frameImage = QImage(latestFrame.pixels.data(), latestFrame.width, latestFrame.height,
                      latestFrame.width * 4, QImage::Format_RGBX8888);

  if (renderStatisticsObject)
  {
    const qint64 nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
    renderStatisticsObject->recordFrame(latestFrame.fps, latestFrame.frameTimeMs, nowNs);
  }

  update();
}

// Sends the widget's size in device pixels to the engine's screen.
void OpenGLViewportWidget::updateScreenSize()
{
  if (!engine || screenId == kInvalidScreenId)
  {
    return;
  }

  const qreal dpr = devicePixelRatioF();
  engine->SetScreenSize(screenId, static_cast<int>(width() * dpr), static_cast<int>(height() * dpr));
}

bool OpenGLViewportWidget::event(QEvent *event)
{
  // A floating dock moved to a monitor with a different scale changes the device-pixel size only.
  if (event->type() == QEvent::DevicePixelRatioChange)
  {
    updateScreenSize();
  }

  return QWidget::event(event);
}

void OpenGLViewportWidget::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  updateScreenSize();
}

// Scales the latest frame to the widget, which also covers frames still rendered at the
//  previous size while a resize is in flight.
void OpenGLViewportWidget::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  if (frameImage.isNull())
  {
    painter.fillRect(rect(), QColor::fromRgbF(fillColor.r, fillColor.g, fillColor.b));
    return;
  }

  painter.drawImage(rect(), frameImage);
}

/************************************************** 
 * 
 * Input event handlers. 
 * These forward input events to the scene's camera movement component, 
 * which will update the camera based on the input.
 * 
**************************************************/
void OpenGLViewportWidget::keyPressEvent(QKeyEvent *event)
{
  if (engine)
  {
    engine->OnKeyChanged(event->key(), true);
  }

  event->accept();
}

void OpenGLViewportWidget::keyReleaseEvent(QKeyEvent *event)
{
  if (engine)
  {
    engine->OnKeyChanged(event->key(), false);
  }

  event->accept();
}

/**
 * @brief Handles mouse press events. 
 *        On Ctrl + left button press, cast a ray into the scene to select an object under the cursor.
 *        Also updates the pending input state with the new mouse button state and position. 
 * 
 * @param event 
 */
void OpenGLViewportWidget::mousePressEvent(QMouseEvent *event)
{
  setFocus();

  if (!engine)
  {
    event->accept();
    return;
  }

  // The event's own modifiers, not the engine's key state: key events are replayed on the engine
  //  thread asynchronously, and Ctrl may have been pressed while another widget had focus.
  if (event->modifiers().testFlag(Qt::ControlModifier) &&
    event->button() == Qt::LeftButton)
  {
    const float viewportWidth = static_cast<float>(std::max(1, width()));
    const float viewportHeight = static_cast<float>(std::max(1, height()));
    const float mouseX = static_cast<float>(event->position().x());
    const float mouseY = static_cast<float>(event->position().y());

    const float ndcX = (2.0f * mouseX / viewportWidth) - 1.0f;
    const float ndcY = 1.0f - (2.0f * mouseY / viewportHeight);

    const Engine::Ray ray = engine->ScreenPointToRay(ndcX, ndcY);
    inspectAdapterObject->selectObjectByRay(ray.origin, ray.direction);
  }

  engine->OnMouseButtonChanged(static_cast<int>(event->button()), true);
  engine->OnMousePositionChanged(glm::vec2(static_cast<float>(event->position().x()),
                                           static_cast<float>(event->position().y())));

  event->accept();
}

void OpenGLViewportWidget::mouseReleaseEvent(QMouseEvent *event)
{
  if (engine)
  {
    engine->OnMouseButtonChanged(static_cast<int>(event->button()), false);
    engine->OnMousePositionChanged(glm::vec2(static_cast<float>(event->position().x()),
                                             static_cast<float>(event->position().y())));
  }

  event->accept();
}

void OpenGLViewportWidget::mouseMoveEvent(QMouseEvent *event)
{
  if (engine)
  {
    engine->OnMousePositionChanged(glm::vec2(static_cast<float>(event->position().x()),
                                             static_cast<float>(event->position().y())));
  }

  event->accept();
}

void OpenGLViewportWidget::wheelEvent(QWheelEvent *event)
{
  if (engine)
  {
    const QPoint angleDelta = event->angleDelta();
    engine->OnScroll(static_cast<float>(angleDelta.y()) / 120.0f);
  }

  event->accept();
}
