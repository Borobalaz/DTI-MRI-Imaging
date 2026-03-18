#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include "Scene.h"

namespace
{
    constexpr int kInitialWindowWidth = 800;
    constexpr int kInitialWindowHeight = 600;

    struct SceneFramebuffer
    {
        GLuint framebuffer = 0;
        GLuint colorTexture = 0;
        GLuint depthStencilRenderbuffer = 0;
        int width = 0;
        int height = 0;

        void EnsureSize(int newWidth, int newHeight)
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

        void Destroy()
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

        ImTextureID GetImGuiTextureId() const
        {
            return (ImTextureID)(uintptr_t)colorTexture;
        }
    };

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

    void RenderRuntimeControls(Scene& scene, float deltaTime)
    {
        std::vector<UiField> fields;
        scene.CollectInspectableFields(fields);

        ImGui::Begin("Runtime Controls");

        std::string activeGroup;
        for (UiField& field : fields)
        {
            if (field.group != activeGroup)
            {
                if (!activeGroup.empty())
                {
                    ImGui::Separator();
                }

                activeGroup = field.group;
                ImGui::TextUnformatted(activeGroup.c_str());
            }

            if (!field.getter || !field.setter)
            {
                continue;
            }

            UiFieldValue value = field.getter();

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
        }

        const float fps = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
        ImGui::Text("FPS: %.1f", fps);
        ImGui::End();
    }

    void RenderScenePanel(Scene& scene, SceneFramebuffer& sceneFramebuffer, int windowFramebufferWidth, int windowFramebufferHeight, float deltaTime)
    {
        ImGui::Begin("Scene");

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
    scene.SetSkybox(SkyboxFaces{
        "assets/textures/skybox/right.png",
        "assets/textures/skybox/left.png",
        "assets/textures/skybox/top.png",
        "assets/textures/skybox/bottom.png",
        "assets/textures/skybox/front.png",
        "assets/textures/skybox/back.png"
    });

    InitializeImGui(window);

    SceneFramebuffer sceneFramebuffer;

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

        RenderRuntimeControls(scene, deltaTime);
        RenderScenePanel(scene, sceneFramebuffer, framebufferWidth, framebufferHeight, deltaTime);

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