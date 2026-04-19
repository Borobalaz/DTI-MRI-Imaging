#include "ui/widgets/DTIViewportWidget.h"

#include <iostream>
#include <memory>

#include "core/DtiVolumeScene.h"
#include "Camera/InspectionCameraMovement.h"

/**
 * @brief Construct a new DTIViewportWidget::DTIViewportWidget object
 * 
 * @param parent 
 */
DTIViewportWidget::DTIViewportWidget(QWidget *parent)
  : OpenGLViewportWidget(parent)
{
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
 * @brief Initializes the DTI volume scene.
 * 
 * Creates a DtiVolumeScene, loads the DTI dataset from the configured file paths,
 * and sets up camera movement controls.
 */
void DTIViewportWidget::initializeScene()
{
  // Create DTI-specific scene
  auto dtiScene = std::make_unique<DtiVolumeScene>();
  dtiScene->Init();

  // DTI-specific: load dataset and prepare for rendering
  if (!dtiScene->LoadDataset(dwiPath().toStdString(), bvalPath().toStdString(), bvecPath().toStdString()))
  {
    std::cout << "DTI dataset load failed: " << dtiScene->GetLastLoadError() << "\n";
  }

  // Set up camera movement
  if (auto sceneCamera = dtiScene->GetCamera())
  {
    auto moveComponent = std::make_unique<InspectionCameraMovement>(glm::vec3(0.0f));
    moveComponent->SetInputState(&dtiScene->GetInputState());
    movement = moveComponent.get();
    sceneCamera->SetMoveComponent(std::move(moveComponent));
  }

  // Store the scene in base class
  scene = std::move(dtiScene);
}


