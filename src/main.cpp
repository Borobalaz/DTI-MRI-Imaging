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
#include "Scene/Scene.h"
#include "InspectionMovement.h"
#include "Gui/GuiRoot.h"
#include "Scene/DtiVolumeScene.h"

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
  // Create main application window
  GLFWwindow* window = CreateMainWindow();
  if (window == nullptr)
  {
    return -1;
  }

  // Create and initialize the scene
  DtiVolumeScene scene;
  scene.Init();
  if (!scene.LoadDataset(
    "assets/volumes/dwi/HARDI150_hdbet_masked4d.nii.gz",
    "assets/volumes/dwi/HARDI150.bval",
    "assets/volumes/dwi/HARDI150.bvec"))
  {
    std::cout << "DTI dataset load failed: " << scene.GetLastLoadError() << "\n";
  }

  BaseMovement* movement = nullptr;
  if (std::shared_ptr<Camera> sceneCamera = scene.GetCamera())
  {
    auto moveComponent = std::make_unique<InspectionMovement>(window, glm::vec3(0.0f));
    BaseMovement* movementPtr = moveComponent.get();
    sceneCamera->SetMoveComponent(std::move(moveComponent));
    movement = movementPtr;
  }

  // Initialize ImGui for GUI rendering
  InitializeImGui(window);
  GuiRoot gui;

  float lastTime = glfwGetTime();

  // Main application loop
  while (!glfwWindowShouldClose(window)) 
  {
    // Time management
    float currentTime = glfwGetTime();
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    // Hot reload: check shader files for changes
    scene.ReloadShadersIfChanged();

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

    gui.Render(scene,
           deltaTime,
           framebufferWidth,
           framebufferHeight,
           movement);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Cleanup and exit
  gui.Destroy();
  ShutdownImGui();

  scene.Destroy();
  glfwDestroyWindow(window);
  glfwTerminate();
}
