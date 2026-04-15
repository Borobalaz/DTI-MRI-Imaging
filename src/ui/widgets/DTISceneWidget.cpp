#include "ui/widgets/DTISceneWidget.h"

#include <iostream>
#include <memory>

#include <glad/glad.h>

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

#include "Scene/DtiVolumeScene.h"

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

QOpenGLFramebufferObject *DTISceneWidget::Renderer::createFramebufferObject(const QSize &size)
{
  QOpenGLFramebufferObjectFormat format;
  format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
  return new QOpenGLFramebufferObject(size, format);
}

void DTISceneWidget::Renderer::synchronize(QQuickFramebufferObject *item)
{
  viewportItem = static_cast<DTISceneWidget *>(item);
}

void DTISceneWidget::Renderer::render()
{
  if (!scene)
  {
    initializeScene();
    if (!scene)
    {
      return;
    }
  }

  if (viewportItem && viewportItem->height() > 0.0f)
  {
    scene->SetCameraAspect(static_cast<float>(viewportItem->width()) / static_cast<float>(viewportItem->height()));
  }

  scene->ReloadShadersIfChanged();

  const qint64 nowNs = elapsedTimer.nsecsElapsed();
  const float deltaSeconds = static_cast<float>(nowNs - lastFrameTimeNs) / 1e9f;
  lastFrameTimeNs = nowNs;

  scene->Update(deltaSeconds);
  scene->Render();
}

void DTISceneWidget::Renderer::initializeScene()
{
  if (!viewportItem)
  {
    return;
  }

  if (!gladLoadGLLoader(QtGetProcAddress))
  {
    std::cout << "Failed to initialize GLAD in QML context.\n";
    return;
  }

  scene = std::make_unique<DtiVolumeScene>();
  scene->Init();

  if (!scene->LoadDataset(viewportItem->dwiPath().toStdString(),
                          viewportItem->bvalPath().toStdString(),
                          viewportItem->bvecPath().toStdString()))
  {
    std::cout << "DTI dataset load failed: " << scene->GetLastLoadError() << "\n";
  }

  if (auto sceneCamera = scene->GetCamera())
  {
    auto moveComponent = std::make_unique<QtInspectionMovement>(glm::vec3(0.0f));
    movement = moveComponent.get();
    viewportItem->setMovementProxy(movement);
    sceneCamera->SetMoveComponent(std::move(moveComponent));
  }

  elapsedTimer.start();
  lastFrameTimeNs = elapsedTimer.nsecsElapsed();
}

DTISceneWidget::DTISceneWidget(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
  setFlag(ItemIsFocusScope, true);
  setAcceptedMouseButtons(Qt::AllButtons);
  setAcceptHoverEvents(true);
  setFocus(true);

  frameTimer.setTimerType(Qt::PreciseTimer);
  frameTimer.setInterval(16);
  QObject::connect(&frameTimer, &QTimer::timeout, this, [this]() { update(); });
  frameTimer.start();
}

DTISceneWidget::~DTISceneWidget() = default;

QQuickFramebufferObject::Renderer *DTISceneWidget::createRenderer() const
{
  return new Renderer();
}

QString DTISceneWidget::dwiPath() const
{
  return dwiPathValue;
}

QString DTISceneWidget::bvalPath() const
{
  return bvalPathValue;
}

QString DTISceneWidget::bvecPath() const
{
  return bvecPathValue;
}

void DTISceneWidget::setDwiPath(const QString &path)
{
  if (dwiPathValue == path)
  {
    return;
  }

  dwiPathValue = path;
  emit dwiPathChanged();
}

void DTISceneWidget::setBvalPath(const QString &path)
{
  if (bvalPathValue == path)
  {
    return;
  }

  bvalPathValue = path;
  emit bvalPathChanged();
}

void DTISceneWidget::setBvecPath(const QString &path)
{
  if (bvecPathValue == path)
  {
    return;
  }

  bvecPathValue = path;
  emit bvecPathChanged();
}

void DTISceneWidget::setMovementProxy(QtInspectionMovement *movement)
{
  movementProxy.store(movement, std::memory_order_release);
}

void DTISceneWidget::componentComplete()
{
  QQuickFramebufferObject::componentComplete();
  update();
}

void DTISceneWidget::keyPressEvent(QKeyEvent *event)
{
  if (QtInspectionMovement *movement = movementProxy.load(std::memory_order_acquire))
  {
    movement->SetKeyPressed(event->key(), true);
  }

  event->accept();
}

void DTISceneWidget::keyReleaseEvent(QKeyEvent *event)
{
  if (QtInspectionMovement *movement = movementProxy.load(std::memory_order_acquire))
  {
    movement->SetKeyPressed(event->key(), false);
  }

  event->accept();
}

void DTISceneWidget::mousePressEvent(QMouseEvent *event)
{
  forceActiveFocus();

  if (QtInspectionMovement *movement = movementProxy.load(std::memory_order_acquire))
  {
    movement->SetMouseButtonPressed(event->button(), true);
    movement->SetMousePosition(event->position());
  }

  event->accept();
}

void DTISceneWidget::mouseReleaseEvent(QMouseEvent *event)
{
  if (QtInspectionMovement *movement = movementProxy.load(std::memory_order_acquire))
  {
    movement->SetMouseButtonPressed(event->button(), false);
    movement->SetMousePosition(event->position());
  }

  event->accept();
}

void DTISceneWidget::mouseMoveEvent(QMouseEvent *event)
{
  if (QtInspectionMovement *movement = movementProxy.load(std::memory_order_acquire))
  {
    movement->SetMousePosition(event->position());
  }

  event->accept();
}

void DTISceneWidget::wheelEvent(QWheelEvent *event)
{
  if (QtInspectionMovement *movement = movementProxy.load(std::memory_order_acquire))
  {
    const QPoint angleDelta = event->angleDelta();
    movement->AddScrollDelta(static_cast<float>(angleDelta.y()) / 120.0f);
  }

  event->accept();
}
