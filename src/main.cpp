#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <cstdint>
#include <memory>
#include <string>
#include "Scene.h"
#include "InspectionMovement.h"
#include "Gui/UIRenderer.h"

namespace
{
  constexpr int kInitialWindowWidth = 800;
  constexpr int kInitialWindowHeight = 600;

  GLFWwindow* CreateMainWindow()
  {
    if (!glfwInit())
    {
      std::cout << "Failed to init GLFW\n";
      return nullptr;
    }

    GLFWwindow* window = glfwCreateWindow(kInitialWindowWidth, kInitialWindowHeight, "OpenGL Window", nullptr, nullptr);
    if (window == nullptr)
    {
      glfwTerminate();
      return nullptr;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
      std::cout << "Failed to initialize GLAD\n";
      glfwDestroyWindow(window);
      glfwTerminate();
      return nullptr;
    }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    return window;
  }

  void InitializeImGui(GLFWwindow* window)
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
  }

  void ShutdownImGui()
  {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  void SetupInitialDockLayout(ImGuiID dockspaceId)
  {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

    ImGuiID controlsDockId = 0;
    ImGuiID centerDockId = dockspaceId;
    ImGui::DockBuilderSplitNode(centerDockId, ImGuiDir_Left, 0.28f, &controlsDockId, &centerDockId);

    ImGui::DockBuilderDockWindow("Runtime Controls", controlsDockId);
    ImGui::DockBuilderDockWindow("Scene", centerDockId);
    ImGui::DockBuilderFinish(dockspaceId);
  }
}

int main()
{
  GLFWwindow* window = CreateMainWindow();
  if (window == nullptr)
  {
    return -1;
  }

  Scene scene;
  scene.Init();
  InspectionMovement* inspectionMovement = nullptr;
  if (std::shared_ptr<Camera> sceneCamera = scene.GetCamera())
  {
    auto movement = std::make_unique<InspectionMovement>(window, glm::vec3(0.0f));
    inspectionMovement = movement.get();
    sceneCamera->SetMoveComponent(std::move(movement));
  }
  scene.SetSkybox(SkyboxFaces{
    "assets/textures/skybox/right.png",
    "assets/textures/skybox/left.png",
    "assets/textures/skybox/top.png",
    "assets/textures/skybox/bottom.png",
    "assets/textures/skybox/front.png",
    "assets/textures/skybox/back.png"
  });

  InitializeImGui(window);

  UIRenderer::SceneFramebuffer sceneFramebuffer;

  float lastTime = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {

    float currentTime = glfwGetTime();
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(
      0,
      ImGui::GetMainViewport(),
      ImGuiDockNodeFlags_PassthruCentralNode
    );

    static bool layoutInitialized = false;
    if (!layoutInitialized)
    {
      SetupInitialDockLayout(dockspaceId);
      layoutInitialized = true;
    }

    UIRenderer::RenderRuntimeControls(scene, deltaTime);
    UIRenderer::RenderScenePanel(scene,
                   sceneFramebuffer,
                   framebufferWidth,
                   framebufferHeight,
                   deltaTime,
                   inspectionMovement);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  sceneFramebuffer.Destroy();
  ShutdownImGui();

  scene.Destroy();
  glfwDestroyWindow(window);
  glfwTerminate();
}
