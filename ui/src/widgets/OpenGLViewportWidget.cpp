#include "widgets/OpenGLViewportWidget.h"

#include <iostream>
#include <memory>

#include <algorithm>
#include <cmath>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>
#include <QtGlobal>

#include "qt-adapters/QTSceneInspector.h"
#include "state/RenderStatistics.h"

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

void OpenGLViewportWidget::SetFillColor(const glm::vec3& color)
{
  fillColor = color;
  if (engine)
  {
    engine->SetFillColor(fillColor);
    update();
  }
}

/**
 * @brief Override of QOpenGLWidget::initializeGL. 
 *  This is called once when the OpenGL context is ready.
 * 
 */
void OpenGLViewportWidget::initializeGL()
{
  engine = std::make_unique<Engine>();
  if (!engine->InitializeOpenGL(QtGetProcAddress))
  {
    std::cout << "Failed to initialize GLAD in widgets OpenGL context.\n";
    return;
  }

  initializeScene();
}

void OpenGLViewportWidget::initializeScene()
{
  engine->CreateScene();
  engine->SetFillColor(fillColor);

  if (inspectAdapterObject)
  {
    inspectAdapterObject->SetInspectionService(&engine->GetInspectionService());
  }

  elapsedTimer.start();
  lastFrameTimeNs = 0;
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
  if (engine)
  {
    engine->SetViewportSize(width, height);
  }
}

/**
 * @brief Override of QOpenGLWidget::paintGL. This is called every frame to render the scene.
 * 
 */
void OpenGLViewportWidget::paintGL()
{
  if (!engine)
  {
    return;
  }

  if (height() > 0)
  {
    engine->SetViewportSize(width(), height());
  }

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
  engine->Update(deltaSeconds);
  engine->Render();

  // Update render statistics
  const qint64 renderDurationNs = elapsedTimer.nsecsElapsed() - renderStartNs;
  const double renderTimeMs = static_cast<double>(renderDurationNs) / 1e6;
  const double fps = deltaSeconds > 1e-6f ? 1.0 / static_cast<double>(deltaSeconds) : 0.0;

  if (renderStatisticsObject)
  {
    renderStatisticsObject->recordFrame(fps, renderTimeMs, elapsedTimer.nsecsElapsed());
  }

  // Keep requesting frames so the viewport animates continuously.
  update();
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
  engine->OnKeyChanged(event->key(), true);

  event->accept();
}

void OpenGLViewportWidget::keyReleaseEvent(QKeyEvent *event)
{
  engine->OnKeyChanged(event->key(), false);

  event->accept();
}

/**
 * @brief Handles mouse press events. 
 *        On left button press, cast a ray into the scene to select an object under the cursor.
 *        Also updates the pending input state with the new mouse button state and position. 
 * 
 * @param event 
 */
void OpenGLViewportWidget::mousePressEvent(QMouseEvent *event)
{
  setFocus();

  // if Ctrl is pressed
  if (engine->IsKeyDown(Qt::Key_Control) &&
    event &&
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
  engine->OnMouseButtonChanged(static_cast<int>(event->button()), false);
  engine->OnMousePositionChanged(glm::vec2(static_cast<float>(event->position().x()),
                                           static_cast<float>(event->position().y())));

  event->accept();
}

void OpenGLViewportWidget::mouseMoveEvent(QMouseEvent *event)
{
  engine->OnMousePositionChanged(glm::vec2(static_cast<float>(event->position().x()),
                                           static_cast<float>(event->position().y())));

  event->accept();
}

void OpenGLViewportWidget::wheelEvent(QWheelEvent *event)
{
  const QPoint angleDelta = event->angleDelta();
  engine->OnScroll(static_cast<float>(angleDelta.y()) / 120.0f);

  event->accept();
}
