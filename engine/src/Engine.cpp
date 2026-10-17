#include "Engine.h"

#include <glm/gtc/matrix_inverse.hpp>

#include "Scene/Scene.h"
#include "EngineOpenGL.h"
#include "Renderer/ForwardRenderer.h"
#include "RenderCore/ExtractionRegistry.h"
#include "RenderCore/RenderFrameBuilder.h"
#include "Input/InputState.h"
#include "Inspection/SceneInspectionService.h"
#include "DtiVolumeScene.h"

Engine::Engine()
  : renderer(std::make_unique<ForwardRenderer>()),
    extractionRegistry(std::make_unique<ExtractionRegistry>()),
    renderFrameBuilder(std::make_unique<RenderFrameBuilder>()),
    inputState(std::make_unique<InputState>())
{
}

Engine::~Engine() = default;

bool Engine::InitializeOpenGL(GLADloadproc loadProc)
{
  return InitializeEngineOpenGL(loadProc);
}

void Engine::CreateScene()
{
  scene = std::make_unique<DtiVolumeScene>();
  scene->Init();
  scene->RebuildInspectProviders();
  inspectionService = std::make_unique<SceneInspectionService>(*scene);
}

void Engine::SetViewportSize(int width, int height)
{
  if (scene && height > 0)
  {
    scene->SetCameraAspect(static_cast<float>(width) / static_cast<float>(height));
  }
  renderer->Resize(width, height);
}

void Engine::SetFillColor(const glm::vec3& color)
{
  renderer->SetFillColor(color);
}

void Engine::Update(float deltaSeconds)
{
  if (!scene)
  {
    return;
  }

  scene->ReloadShadersIfChanged();
  scene->SetInputState(*inputState);
  scene->Update(deltaSeconds);

  if (inspectionService)
  {
    inspectionService->PollForProviderChanges();
  }
}

void Engine::Render()
{
  if (!scene)
  {
    return;
  }

  const SceneSnapshot snapshot = scene->CreateSnapshot();
  const RenderFrame frame = renderFrameBuilder->Build(snapshot, renderer->GetDescriptor(), *extractionRegistry);
  renderer->Draw(frame);
  inputState->ResetFrameTransientState();
}

void Engine::OnKeyChanged(int key, bool down)
{
  inputState->SetKeyDown(key, down);
}

bool Engine::IsKeyDown(int key) const
{
  return inputState->IsKeyDown(key);
}

void Engine::OnMouseButtonChanged(int button, bool down)
{
  inputState->SetMouseButtonDown(button, down);
}

void Engine::OnMousePositionChanged(const glm::vec2& position)
{
  inputState->SetMousePosition(position);
}

void Engine::OnScroll(float delta)
{
  inputState->AddScrollDelta(delta);
}

IInspectionService& Engine::GetInspectionService()
{
  return *inspectionService;
}

Engine::Ray Engine::ScreenPointToRay(float ndcX, float ndcY) const
{
  if (!scene)
  {
    return Ray{glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
  }

  std::shared_ptr<Camera> camera = scene->GetCamera();
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
