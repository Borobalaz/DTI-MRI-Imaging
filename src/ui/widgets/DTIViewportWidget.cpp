#include "ui/widgets/DTIViewportWidget.h"

#include <iostream>
#include <memory>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>
#include <QtGlobal>

#include "core/DtiVolumeScene.h"
#include "ui/qt-adapters/QTSceneInspector.h"
#include "ui/qt-adapters/QtInspectionMovement.h"
#include "ui/widgets/RenderStatistics.h"

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
 * @brief Construct a new DTIViewportWidget::DTIViewportWidget object
 * 
 * @param parent 
 */
DTIViewportWidget::DTIViewportWidget(QWidget *parent)
  : QOpenGLWidget(parent)
{
  renderStatisticsObject = new RenderStatistics(this);
  inspectAdapterObject = new QTSceneInspector(this);

  QObject::connect(renderStatisticsObject, &RenderStatistics::statisticsChanged, this, [this]()
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
 * @brief Destroy the DTIViewportWidget::DTIViewportWidget object
 * 
 */
DTIViewportWidget::~DTIViewportWidget() = default;

/**
 * @brief Gets the path to the DWI file.
 * 
 * @return QString 
 */
QString DTIViewportWidget::dwiPath() const
{
  return dwiPathValue;
}

/**
 * @brief Gets the path to the bval file.
 * 
 * @return QString 
 */
QString DTIViewportWidget::bvalPath() const
{
  return bvalPathValue;
}

/**
 * @brief Gets the path to the bvec file.
 * 
 * @return QString 
 */
QString DTIViewportWidget::bvecPath() const
{
  return bvecPathValue;
}

/**
 * @brief Gets the render statistics object.
 * 
 * @return RenderStatistics* 
 */
RenderStatistics *DTIViewportWidget::renderStatistics() const
{
  return renderStatisticsObject;
}

/**
 * @brief Gets the scene inspector adapter for this viewport, 
 *  which exposes the inspectable objects in the scene to Qt widgets.
 * 
 * @return QTSceneInspector& 
 */
QTSceneInspector &DTIViewportWidget::inspectAdapter() const
{
  Q_ASSERT(inspectAdapterObject != nullptr);
  return *inspectAdapterObject;
}

/**
 * @brief Sets the path to the DWI file. Emits dwiPathChanged if the path changes.
 * 
 * @param path 
 */
void DTIViewportWidget::setDwiPath(const QString &path)
{
  if (dwiPathValue == path)
  {
    return;
  }

  dwiPathValue = path;
  emit dwiPathChanged();
}

/**
 * @brief Sets the path to the bval file. Emits bvalPathChanged if the path changes.
 * 
 * @param path 
 */
void DTIViewportWidget::setBvalPath(const QString &path)
{
  if (bvalPathValue == path)
  {
    return;
  }

  bvalPathValue = path;
  emit bvalPathChanged();
}

/**
 * @brief Sets the path to the bvec file. Emits bvecPathChanged if the path changes.
 * 
 * @param path 
 */
void DTIViewportWidget::setBvecPath(const QString &path)
{
  if (bvecPathValue == path)
  {
    return;
  }

  bvecPathValue = path;
  emit bvecPathChanged();
}

/**
 * @brief Override of QOpenGLWidget::initializeGL. 
 *  This is called once when the OpenGL context is ready.
 * 
 */
void DTIViewportWidget::initializeGL()
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
void DTIViewportWidget::resizeGL(int width, int height)
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
void DTIViewportWidget::paintGL()
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
  scene->Update(deltaSeconds);
  scene->Render();

  // Update render statistics
  const qint64 renderDurationNs = elapsedTimer.nsecsElapsed() - renderStartNs;
  const double renderTimeMs = static_cast<double>(renderDurationNs) / 1e6;
  const double fps = deltaSeconds > 1e-6f ? 1.0 / static_cast<double>(deltaSeconds) : 0.0;

  if (renderStatisticsObject)
  {
    renderStatisticsObject->recordFrame(fps, renderTimeMs, elapsedTimer.nsecsElapsed());
  }
}

/**
 * @brief Initializes the 3D scene for the viewport.
 * 
 */
void DTIViewportWidget::initializeScene()
{
  // Create scene 
  scene = std::make_unique<DtiVolumeScene>();
  scene->Init();

  // DTI specific - load dataset and prepare for rendering
  if (!scene->LoadDataset(dwiPath().toStdString(), bvalPath().toStdString(), bvecPath().toStdString()))
  {
    std::cout << "DTI dataset load failed: " << scene->GetLastLoadError() << "\n";
  }

  // Set up camera movement
  if (auto sceneCamera = scene->GetCamera())
  {
    auto moveComponent = std::make_unique<QtInspectionMovement>(glm::vec3(0.0f));
    movement = moveComponent.get();
    sceneCamera->SetMoveComponent(std::move(moveComponent));
  }

  // Start timer 
  elapsedTimer.start();
  if (renderStatisticsObject)
  {
    renderStatisticsObject->reset();
  }
  lastFrameTimeNs = 0;
}

/************************************************** 
 * 
 * Input event handlers. 
 * These forward input events to the scene's camera movement component, 
 * which will update the camera based on the input.
 * 
**************************************************/
void DTIViewportWidget::keyPressEvent(QKeyEvent *event)
{
  if (movement)
  {
    movement->SetKeyPressed(event->key(), true);
  }

  event->accept();
}

void DTIViewportWidget::keyReleaseEvent(QKeyEvent *event)
{
  if (movement)
  {
    movement->SetKeyPressed(event->key(), false);
  }

  event->accept();
}

void DTIViewportWidget::mousePressEvent(QMouseEvent *event)
{
  setFocus();

  if (movement)
  {
    movement->SetMouseButtonPressed(event->button(), true);
    movement->SetMousePosition(event->position());
  }

  event->accept();
}

void DTIViewportWidget::mouseReleaseEvent(QMouseEvent *event)
{
  if (movement)
  {
    movement->SetMouseButtonPressed(event->button(), false);
    movement->SetMousePosition(event->position());
  }

  event->accept();
}

void DTIViewportWidget::mouseMoveEvent(QMouseEvent *event)
{
  if (movement)
  {
    movement->SetMousePosition(event->position());
  }

  event->accept();
}

void DTIViewportWidget::wheelEvent(QWheelEvent *event)
{
  if (movement)
  {
    const QPoint angleDelta = event->angleDelta();
    movement->AddScrollDelta(static_cast<float>(angleDelta.y()) / 120.0f);
  }

  event->accept();
}

