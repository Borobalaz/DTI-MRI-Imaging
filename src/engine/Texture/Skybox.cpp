#include "Skybox.h"

#include <glm/glm.hpp>

namespace
{
  constexpr unsigned int kSkyboxTextureUnit = 5;
}

Skybox::Skybox(std::shared_ptr<TextureCube> cubemap)
  : cubemap(std::move(cubemap)),
    geometry(std::make_shared<CubeGeometry>()),
    shader(std::make_shared<Shader>("shaders/skybox_vertex.glsl", "shaders/skybox_fragment.glsl"))
{
}

void Skybox::Draw(const Camera& camera) const
{
  if (!IsValid())
  {
    return;
  }

  GLboolean previousDepthWriteMask = GL_TRUE;
  GLint previousDepthFunc = GL_LESS;
  GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);

  glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthWriteMask);
  glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);

  glDepthMask(GL_FALSE);
  glDepthFunc(GL_LEQUAL);
  glDisable(GL_CULL_FACE);

  shader->Use();
  shader->SetMat4("viewMatrix", glm::mat4(glm::mat3(camera.GetViewMatrix())));
  shader->SetMat4("projectionMatrix", camera.GetProjectionMatrix());
  shader->SetTexture("skyboxTexture", kSkyboxTextureUnit);
  cubemap->Bind(kSkyboxTextureUnit);
  geometry->Draw(*shader);

  if (cullFaceEnabled)
  {
    glEnable(GL_CULL_FACE);
  }

  glDepthMask(previousDepthWriteMask);
  glDepthFunc(previousDepthFunc);
}

bool Skybox::IsValid() const
{
  return cubemap != nullptr && cubemap->IsValid() && shader != nullptr && shader->ID != 0;
}