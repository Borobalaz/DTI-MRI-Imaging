#pragma once

#include <glad/glad.h>
#include <imgui.h>

class SceneFramebuffer
{
public:
  GLuint framebuffer = 0;
  GLuint colorTexture = 0;
  GLuint depthStencilRenderbuffer = 0;
  int width = 0;
  int height = 0;

  void EnsureSize(int newWidth, int newHeight);
  void Destroy();
  ImTextureID GetImGuiTextureId() const;
};
