#include "Texture/Skybox.h"

#include "Renderer/RenderProxy.h"
#include "Shader.h"

Skybox::Skybox(std::shared_ptr<TextureCube> cubemap)
  : cubemap(std::move(cubemap)),
    geometry(std::make_shared<CubeGeometry>()),
    shader(std::make_shared<Shader>("skybox_shader", "shaders/skybox/skybox_vertex.glsl", "shaders/skybox/skybox_fragment.glsl"))
{
}

bool Skybox::IsValid() const
{
  return cubemap != nullptr && cubemap->IsValid() && shader != nullptr && shader->ID != 0;
}

void Skybox::BuildRenderProxy(RenderProxy& renderProxy) const
{
  renderProxy.visible = IsValid();
  if (!renderProxy.visible)
  {
    return;
  }

  renderProxy.data.Append(SkyboxRenderCommand{
    geometry,
    shader,
    cubemap,
    renderProxy.frameUniforms});
}