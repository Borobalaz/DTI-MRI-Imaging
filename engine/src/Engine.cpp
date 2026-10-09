#include "Engine.h"

#include <glm/gtc/matrix_inverse.hpp>

#include "Scene/Scene.h"
#include "EngineOpenGL.h"
#include "Camera/InspectionCameraMovement.h"
#include "Renderer/ForwardRenderer.h"
#include "RenderCore/ExtractionRegistry.h"
#include "RenderCore/RenderFrameBuilder.h"
#include "Input/InputState.h"
#include "Inspection/SceneInspectionService.h"
#include "DtiVolumeScene.h"

struct Engine::Impl
{
  std::unique_ptr<Scene> scene;
  BaseMovement* movement = nullptr;
  ForwardRenderer renderer;
  ExtractionRegistry extractionRegistry;
  RenderFrameBuilder renderFrameBuilder;
  InputState inputState;
  std::unique_ptr<SceneInspectionService> inspectionService;
};

Engine::Engine()
  : impl(std::make_unique<Impl>())
{
}

Engine::~Engine() = default;

bool Engine::InitializeOpenGL(GLADloadproc loadProc)
{
  return InitializeEngineOpenGL(loadProc);
}

void Engine::CreateScene()
{
  impl->movement = nullptr;
  impl->scene = std::make_unique<DtiVolumeScene>();
  impl->scene->Init();

  if (std::shared_ptr<Camera> camera = impl->scene->GetCamera())
  {
    auto* inspectionMovement = new InspectionCameraMovement();
    inspectionMovement->SetInputState(&impl->inputState);
    impl->movement = inspectionMovement;
    camera->SetMoveComponent(std::unique_ptr<BaseMovement>(inspectionMovement));
  }

  impl->scene->RebuildInspectProviders();
  impl->inspectionService = std::make_unique<SceneInspectionService>(*impl->scene);
}

void Engine::SetViewportSize(int width, int height)
{
  if (impl->scene && height > 0)
  {
    impl->scene->SetCameraAspect(static_cast<float>(width) / static_cast<float>(height));
  }
  impl->renderer.Resize(width, height);
}

void Engine::SetFillColor(const glm::vec3& color)
{
  impl->renderer.SetFillColor(color);
}

void Engine::Update(float deltaSeconds)
{
  if (!impl->scene)
  {
    return;
  }

  impl->scene->ReloadShadersIfChanged();
  impl->scene->SetInputState(impl->inputState);
  impl->scene->Update(deltaSeconds);

  if (impl->inspectionService)
  {
    impl->inspectionService->PollForProviderChanges();
  }
}

void Engine::Render()
{
  if (!impl->scene)
  {
    return;
  }

  const SceneSnapshot snapshot = impl->scene->CreateSnapshot();
  const RenderFrame frame = impl->renderFrameBuilder.Build(snapshot, impl->renderer.GetDescriptor(), impl->extractionRegistry);
  impl->renderer.Draw(frame);
  impl->inputState.ResetFrameTransientState();
}

void Engine::OnKeyChanged(int key, bool down)
{
  impl->inputState.SetKeyDown(key, down);
}

bool Engine::IsKeyDown(int key) const
{
  return impl->inputState.IsKeyDown(key);
}

void Engine::OnMouseButtonChanged(int button, bool down)
{
  impl->inputState.SetMouseButtonDown(button, down);
}

void Engine::OnMousePositionChanged(const glm::vec2& position)
{
  impl->inputState.SetMousePosition(position);
}

void Engine::OnScroll(float delta)
{
  impl->inputState.AddScrollDelta(delta);
}

IInspectionService& Engine::GetInspectionService()
{
  return *impl->inspectionService;
}

Engine::Ray Engine::ScreenPointToRay(float ndcX, float ndcY) const
{
  if (!impl->scene)
  {
    return Ray{glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
  }

  std::shared_ptr<Camera> camera = impl->scene->GetCamera();
  if (!camera)
  {
    return Ray{glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
  }

  const glm::mat4 viewMatrix = camera->GetViewMatrix();
  const glm::mat4 projectionMatrix = camera->GetProjectionMatrix();
  const glm::mat4 inverseViewProjection = glm::inverse(projectionMatrix * viewMatrix);

  const glm::vec4 nearClip(ndcX, ndcY, -1.0f, 1.0f);
  const glm::vec4 farClip(ndcX, ndcY, 1.0f, 1.0f);

  glm::vec4 nearWorld = inverseViewProjection * nearClip;
  glm::vec4 farWorld = inverseViewProjection * farClip;
  if (std::abs(nearWorld.w) > 1e-6f)
  {
    nearWorld /= nearWorld.w;
  }
  if (std::abs(farWorld.w) > 1e-6f)
  {
    farWorld /= farWorld.w;
  }

  Ray ray;
  ray.origin = camera->GetPosition();
  ray.direction = glm::normalize(glm::vec3(farWorld - nearWorld));
  return ray;
}
