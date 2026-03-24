#include "Gui/UIRenderer.h"

#include <iostream>
#include <unordered_map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <imgui.h>

#include "Scene/Scene.h"
#include "Gui/Inspectable.h"
#include "InspectionMovement.h"
#include "VolumeFileLoader.h"
#include "FloatVolume.h"
#include "Mat3Volume.h"
#include "UInt16Volume.h"
#include "UInt8Volume.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace
{
  bool HasRenderableInspectableContent(const InspectableNode& node)
  {
    if (node.isField)
    {
      return node.field.getter && node.field.setter;
    }

    if (!node.nestedInspectable)
    {
      return false;
    }

    std::vector<InspectableNode> nestedNodes;
    node.nestedInspectable->CollectInspectableNodes(nestedNodes, "");
    for (const InspectableNode& nestedNode : nestedNodes)
    {
      if (HasRenderableInspectableContent(nestedNode))
      {
        return true;
      }
    }

    return false;
  }
}

/**
 * @brief Ensures the framebuffer is large enough to accommodate the specified dimensions.
 * 
 * @param newWidth The new width of the framebuffer.
 * @param newHeight The new height of the framebuffer.
 */
void UIRenderer::SceneFramebuffer::EnsureSize(int newWidth, int newHeight)
{
  if (newWidth <= 0 || newHeight <= 0)
  {
    return;
  }

  if (framebuffer != 0 && newWidth == width && newHeight == height)
  {
    return;
  }

  width = newWidth;
  height = newHeight;

  if (framebuffer == 0)
  {
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &colorTexture);
    glGenRenderbuffers(1, &depthStencilRenderbuffer);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  glBindTexture(GL_TEXTURE_2D, colorTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

  glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRenderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRenderbuffer);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    std::cout << "Scene framebuffer is not complete\n";
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * @brief Destroy the framebuffer and associated resources.
 * 
 */
void UIRenderer::SceneFramebuffer::Destroy()
{
  if (framebuffer == 0)
  {
    return;
  }

  glDeleteFramebuffers(1, &framebuffer);
  glDeleteTextures(1, &colorTexture);
  glDeleteRenderbuffers(1, &depthStencilRenderbuffer);
  framebuffer = 0;
  colorTexture = 0;
  depthStencilRenderbuffer = 0;
  width = 0;
  height = 0;
}

/**
 * @brief Get the ImGui texture ID for the color texture.
 * 
 * @return ImTextureID The ImGui texture ID.
 */
ImTextureID UIRenderer::SceneFramebuffer::GetImGuiTextureId() const
{
  return (ImTextureID)(uintptr_t)colorTexture;
}

/**
 * @brief Pick a volume file path using a file dialog.
 * 
 * @return std::string The selected file path.
 */
std::string UIRenderer::PickVolumeFilePath()
{
#ifdef _WIN32
  char selectedPath[4096] = {};

  OPENFILENAMEA dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.lpstrFilter =
    "Volume Files\0*.vxa;*.nii;*.nii.gz;*.nrrd;*.nhdr;*.mha;*.mhd;*.hdr;*.img\0"
    "All Files\0*.*\0";
  dialog.lpstrFile = selectedPath;
  dialog.nMaxFile = static_cast<DWORD>(sizeof(selectedPath));
  dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
  dialog.lpstrTitle = "Open Volume File";

  if (GetOpenFileNameA(&dialog) == TRUE)
  {
    return std::string(selectedPath);
  }
#endif

  return std::string();
}

/**
 * @brief Handle the action of loading a volume file.
 * 
 * @param scene The scene to load the volume into.
 * @param volumeLoadStatus The status message for the volume load operation.
 */
void UIRenderer::HandleVolumeLoadAction(Scene& scene, std::string& volumeLoadStatus)
{
  if (!ImGui::Button("Open Volume..."))
  {
    return;
  }

  const std::string filePath = PickVolumeFilePath();
  if (filePath.empty())
  {
    volumeLoadStatus = "Volume load cancelled.";
    return;
  }

  volumeLoadStatus = TryLoadVolumeStatus(scene, filePath);
}

/**
 * @brief Try to load a volume file and return the status message.
 * 
 * @param scene The scene to load the volume into.
 * @param filePath The path to the volume file.
 * @return std::string The status message for the volume load operation.
 */
std::string UIRenderer::TryLoadVolumeStatus(Scene& scene, const std::string& filePath)
{
  std::optional<LoadedVolumeVariant> loadedVolume = VolumeFileLoader::Load(filePath);
  if (!loadedVolume.has_value())
  {
    const std::string detailedError = VolumeFileLoader::GetLastError();
    if (!detailedError.empty())
    {
      return "Failed to load volume: " + filePath + "\nReason: " + detailedError;
    }

    return "Failed to load volume: " + filePath;
  }

  std::shared_ptr<Shader> scalarShader = scene.GetActiveVolumeShader();
  std::shared_ptr<Shader> matrixShader = scene.GetMatrixVolumeShader();
  Volume* runtimeVolume = nullptr;
  bool loadedTypeIsSupported = false;

  std::visit(
    [&](auto&& volumeData)
    {
      using VolumeDataType = std::decay_t<decltype(volumeData)>;

      if constexpr (std::is_same_v<VolumeDataType, VolumeData<float>>)
      {
        loadedTypeIsSupported = true;
        if (scalarShader)
        {
          runtimeVolume = new FloatVolume(volumeData, scalarShader);
        }
      }
      else if constexpr (std::is_same_v<VolumeDataType, VolumeData<uint16_t>>)
      {
        loadedTypeIsSupported = true;
        if (scalarShader)
        {
          runtimeVolume = new UInt16Volume(volumeData, scalarShader);
        }
      }
      else if constexpr (std::is_same_v<VolumeDataType, VolumeData<uint8_t>>)
      {
        loadedTypeIsSupported = true;
        if (scalarShader)
        {
          runtimeVolume = new UInt8Volume(volumeData, scalarShader);
        }
      }
      else if constexpr (std::is_same_v<VolumeDataType, VolumeData<glm::mat3>>)
      {
        loadedTypeIsSupported = true;
        if (matrixShader)
        {
          runtimeVolume = new Mat3Volume(volumeData, matrixShader);
        }
      }
    },
    loadedVolume.value());

  if (runtimeVolume != nullptr)
  {
    scene.SetVolume(runtimeVolume);
    return "Loaded volume: " + filePath;
  }

  if (!loadedTypeIsSupported)
  {
    return "Load failed: unsupported volume type for runtime scene loader.";
  }

  return "Failed to create runtime volume for: " + filePath;
}

/**
 * @brief Render the status message for the volume load operation.
 * 
 * @param volumeLoadStatus The status message to render.
 */
void UIRenderer::RenderVolumeLoadStatus(const std::string& volumeLoadStatus)
{
  if (volumeLoadStatus.empty())
  {
    return;
  }

  ImGui::TextWrapped("%s", volumeLoadStatus.c_str());
  ImGui::Separator();
}

/**
 * @brief Render the inspectable controls for the scene.
 * 
 * @param scene The scene to render controls for.
 */
void UIRenderer::RenderInspectableControls(Scene& scene)
{
  std::vector<InspectableNode> nodes;
  scene.CollectInspectableNodes(nodes);

  for (const InspectableNode& node : nodes)
  {
    if (!HasRenderableInspectableContent(node))
    {
      continue;
    }

    RenderInspectableNode(node, 0);
  }
}

/**
 * @brief Render a single inspectable node, which can be either a field or a nested inspectable.
 * 
 * @param node The node to render.
 * @param depth The depth of the node in the hierarchy, used for indentation.
 */
void UIRenderer::RenderInspectableNode(const InspectableNode& node, int depth)
{
  if (node.isField)
  {
    // Render a field widget
    const UiField& field = node.field;
    if (!field.getter || !field.setter)
    {
      return;
    }

    UiFieldValue value = field.getter();

    ImGui::PushID(node.nodeLabel.c_str());

    if (field.kind == UiFieldKind::Bool)
    {
      bool currentValue = std::holds_alternative<bool>(value) ? std::get<bool>(value) : false;
      if (ImGui::Checkbox(field.label.c_str(), &currentValue))
      {
        field.setter(currentValue);
      }
    }
    else if (field.kind == UiFieldKind::Int)
    {
      int currentValue = std::holds_alternative<int>(value) ? std::get<int>(value) : 0;
      if (ImGui::SliderInt(field.label.c_str(), &currentValue, field.minInt, field.maxInt))
      {
        field.setter(currentValue);
      }
    }
    else if (field.kind == UiFieldKind::Float)
    {
      float currentValue = std::holds_alternative<float>(value) ? std::get<float>(value) : 0.0f;
      if (ImGui::SliderFloat(field.label.c_str(), &currentValue, field.minFloat, field.maxFloat))
      {
        field.setter(currentValue);
      }
    }
    else if (field.kind == UiFieldKind::Vec3)
    {
      glm::vec3 currentValue = std::holds_alternative<glm::vec3>(value) ? std::get<glm::vec3>(value) : glm::vec3(0.0f);
      if (ImGui::DragFloat3(field.label.c_str(), &currentValue.x, field.speed))
      {
        field.setter(currentValue);
      }
    }
    else if (field.kind == UiFieldKind::Color3)
    {
      glm::vec3 currentValue = std::holds_alternative<glm::vec3>(value) ? std::get<glm::vec3>(value) : glm::vec3(0.0f);
      if (ImGui::ColorEdit3(field.label.c_str(), &currentValue.x))
      {
        field.setter(currentValue);
      }
    }

    ImGui::PopID();
  }
  else
  {
    // Render a nested inspectable as a tree node
    if (!node.nestedInspectable)
    {
      return;
    }

    std::vector<InspectableNode> nestedNodes;
    node.nestedInspectable->CollectInspectableNodes(nestedNodes, "");

    std::vector<const InspectableNode*> visibleNestedNodes;
    visibleNestedNodes.reserve(nestedNodes.size());
    for (const InspectableNode& nestedNode : nestedNodes)
    {
      if (HasRenderableInspectableContent(nestedNode))
      {
        visibleNestedNodes.push_back(&nestedNode);
      }
    }

    if (visibleNestedNodes.empty())
    {
      return;
    }

    ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (ImGui::TreeNodeEx(node.nodeLabel.c_str(), treeFlags))
    {
      ImGui::Indent();

      for (const InspectableNode* nestedNode : visibleNestedNodes)
      {
        RenderInspectableNode(*nestedNode, depth + 1);
      }

      ImGui::Unindent();
      ImGui::TreePop();
    }
  }
}

/**
 * @brief Render the FPS counter.
 * 
 * @param deltaTime The time elapsed since the last frame.
 */
void UIRenderer::RenderFps(float deltaTime)
{
  const float fps = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
  ImGui::Text("FPS: %.1f", fps);
}

/**
 * @brief Render the runtime controls for the scene.
 * 
 * @param scene The scene to render controls for.
 * @param deltaTime The time elapsed since the last frame.
 */
void UIRenderer::RenderRuntimeControls(Scene& scene, float deltaTime)
{
  ImGui::Begin("Runtime Controls");

  static std::string volumeLoadStatus;
  HandleVolumeLoadAction(scene, volumeLoadStatus);
  RenderVolumeLoadStatus(volumeLoadStatus);
  ImGui::Separator();
  ImGui::Separator();
  RenderInspectableControls(scene);
  RenderFps(deltaTime);
  ImGui::End();
}

/**
 * @brief Render the scene panel for the scene.
 * 
 * @param scene The scene to render.
 * @param sceneFramebuffer The framebuffer for the scene.
 * @param windowFramebufferWidth The width of the window framebuffer.
 * @param windowFramebufferHeight The height of the window framebuffer.
 * @param deltaTime The time elapsed since the last frame.
 */
void UIRenderer::RenderScenePanel(Scene& scene,
                                  SceneFramebuffer& sceneFramebuffer,
                                  int windowFramebufferWidth,
                                  int windowFramebufferHeight,
                                  float deltaTime,
                                  InspectionMovement* inspectionMovement)
{
  ImGui::Begin("Scene");

  if (inspectionMovement != nullptr)
  {
    const bool scenePanelFocused = ImGui::IsWindowFocused();
    inspectionMovement->SetInputEnabled(scenePanelFocused);
  }

  const ImVec2 availableRegion = ImGui::GetContentRegionAvail();
  const int targetSceneWidth = (availableRegion.x >= 1.0f) ? static_cast<int>(availableRegion.x) : 1;
  const int targetSceneHeight = (availableRegion.y >= 1.0f) ? static_cast<int>(availableRegion.y) : 1;

  sceneFramebuffer.EnsureSize(targetSceneWidth, targetSceneHeight);
  scene.Update(deltaTime);

  glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer.framebuffer);
  glViewport(0, 0, sceneFramebuffer.width, sceneFramebuffer.height);
  scene.SetCameraAspect(static_cast<float>(sceneFramebuffer.width) / static_cast<float>(sceneFramebuffer.height));
  scene.Render();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, windowFramebufferWidth, windowFramebufferHeight);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGui::Image(
    sceneFramebuffer.GetImGuiTextureId(),
    ImVec2(static_cast<float>(sceneFramebuffer.width), static_cast<float>(sceneFramebuffer.height)),
    ImVec2(0.0f, 1.0f),
    ImVec2(1.0f, 0.0f)
  );

  ImGui::End();
}
