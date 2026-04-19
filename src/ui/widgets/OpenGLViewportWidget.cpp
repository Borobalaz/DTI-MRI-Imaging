#include "ui/widgets/OpenGLViewportWidget.h"

#include <iostream>
#include <memory>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>
#include <QtGlobal>

#include "engine/Scene/Scene.h"
#include "Camera/InspectionCameraMovement.h"
#include "ui/qt-adapters/QTSceneInspector.h"
#include "ui/state/RenderStatistics.h"

namespace
{
  void *QtGetProcAddress(const char *name)
  {
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx)
    {
      return nullptr;
    }

    return reinterpret_cast<void *>(ctx->getProcAddress(name));
  }
}

/**
 * @brief Construct a new OpenGLViewportWidget::OpenGLViewportWidget object
 * 
 * @param parent 
 */
OpenGLViewportWidget::OpenGLViewportWidget(QWidget *parent)
  : QOpenGLWidget(parent)
{
  renderStatisticsObject = std::make_unique<RenderStatistics>();
  inspectAdapterObject = std::make_unique<QTSceneInspector>();

  QObject::connect(renderStatisticsObject.get(), &RenderStatistics::statisticsChanged, this, [this]()
  {
    emit renderStatisticsChanged();
  });

  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);

  frameTimer.setTimerType(Qt::PreciseTimer);
  frameTimer.setInterval(0);
  QObject::connect(&frameTimer, &QTimer::timeout, this, [this]()
  {
    update();
  });
  frameTimer.start();
}

/**
 * @brief Destroy the OpenGLViewportWidget::OpenGLViewportWidget object
 * 
 */
OpenGLViewportWidget::~OpenGLViewportWidget() = default;

/**
 * @brief Gets the render statistics object.
 * 
 * @return RenderStatistics* 
 */
RenderStatistics *OpenGLViewportWidget::renderStatistics() const
{
  return renderStatisticsObject.get();
}

/**
 * @brief Gets the scene inspector adapter for this viewport, 
 *  which exposes the inspectable objects in the scene to Qt widgets.
 * 
 * @return QTSceneInspector& 
 */
QTSceneInspector &OpenGLViewportWidget::inspectAdapter() const
{
  Q_ASSERT(inspectAdapterObject != nullptr);
  return *inspectAdapterObject;
}

/**
 * @brief Override of QOpenGLWidget::initializeGL. 
 *  This is called once when the OpenGL context is ready.
 * 
 */
void OpenGLViewportWidget::initializeGL()
{
  if (!gladLoadGLLoader(QtGetProcAddress))
  {
    std::cout << "Failed to initialize GLAD in widgets OpenGL context.\n";
    return;
  }

  initializeScene();
}

/**
 * @brief Override of QOpenGLWidget::resizeGL. This is called when the widget is resized.
 *  Set the camera aspect ratio to match the new viewport dimensions.
 * 
 * @param width 
 * @param height 
 */
void OpenGLViewportWidget::resizeGL(int width, int height)
{
  if (scene && height > 0)
  {
    scene->SetCameraAspect(static_cast<float>(width) / static_cast<float>(height));
  }
}

/**
 * @brief Override of QOpenGLWidget::paintGL. This is called every frame to render the scene.
 * 
 */
void OpenGLViewportWidget::paintGL()
{
  if (!scene)
  {
    return;
  }

  // Keep inspector state in sync with currently exposed scene providers.
  inspectAdapterObject->Update(scene->GetInspectProviders());

  if (height() > 0)
  {
    scene->SetCameraAspect(static_cast<float>(width()) / static_cast<float>(height()));
  }

  // hot reload shaders
  scene->ReloadShadersIfChanged();

  // delta time
  const qint64 nowNs = elapsedTimer.nsecsElapsed();
  float deltaSeconds = 0.0f;
  if (lastFrameTimeNs == 0)
  {
    deltaSeconds = 1.0f / 60.0f;
  }
  else
  {
    deltaSeconds = static_cast<float>(nowNs - lastFrameTimeNs) / 1e9f;
  }
  lastFrameTimeNs = nowNs;

  const qint64 renderStartNs = elapsedTimer.nsecsElapsed();

  // update and render
  scene->SetInputState(pendingInputState);
  scene->Update(deltaSeconds);
  scene->Render();
  pendingInputState.ResetFrameTransientState();

  // Update render statistics
  const qint64 renderDurationNs = elapsedTimer.nsecsElapsed() - renderStartNs;
  const double renderTimeMs = static_cast<double>(renderDurationNs) / 1e6;
  const double fps = deltaSeconds > 1e-6f ? 1.0 / static_cast<double>(deltaSeconds) : 0.0;

  if (renderStatisticsObject)
  {
    renderStatisticsObject->recordFrame(fps, renderTimeMs, elapsedTimer.nsecsElapsed());
  }
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
  pendingInputState.SetKeyDown(event->key(), true);

  event->accept();
}

void OpenGLViewportWidget::keyReleaseEvent(QKeyEvent *event)
{
  pendingInputState.SetKeyDown(event->key(), false);

  event->accept();
}

void OpenGLViewportWidget::mousePressEvent(QMouseEvent *event)
{
  setFocus();

  pendingInputState.SetMouseButtonDown(static_cast<int>(event->button()), true);
  pendingInputState.SetMousePosition(glm::vec2(static_cast<float>(event->position().x()),
                                               static_cast<float>(event->position().y())));

  event->accept();
}

void OpenGLViewportWidget::mouseReleaseEvent(QMouseEvent *event)
{
  pendingInputState.SetMouseButtonDown(static_cast<int>(event->button()), false);
  pendingInputState.SetMousePosition(glm::vec2(static_cast<float>(event->position().x()),
                                               static_cast<float>(event->position().y())));

  event->accept();
}

void OpenGLViewportWidget::mouseMoveEvent(QMouseEvent *event)
{
  pendingInputState.SetMousePosition(glm::vec2(static_cast<float>(event->position().x()),
                                               static_cast<float>(event->position().y())));

  event->accept();
}

void OpenGLViewportWidget::wheelEvent(QWheelEvent *event)
{
  const QPoint angleDelta = event->angleDelta();
  pendingInputState.AddScrollDelta(static_cast<float>(angleDelta.y()) / 120.0f);

  event->accept();
}
