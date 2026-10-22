#include "RenderBackend.h"

#include "Render/Geometry/Geometry.h"
#include "Render/Geometry/InstanceBuffer.h"
#include "Render/RenderTarget.h"
#include "Render/Shader.h"
#include "Render/Texture/Texture.h"

void RenderBackend::Use(Shader& shader)
{
  shader.Use();
}

IUniformSink& RenderBackend::Uniforms(Shader& shader)
{
  return shader;
}

void RenderBackend::Bind(const Texture& texture, unsigned int unit)
{
  texture.Bind(unit);
}

unsigned int RenderBackend::Handle(const Texture& texture)
{
  return texture.GetId();
}

unsigned int RenderBackend::Handle(const InstanceBuffer& instances)
{
  return instances.GetHandle();
}

void RenderBackend::Draw(const Geometry& geometry, Shader& shader)
{
  geometry.Draw(shader);
}

void RenderBackend::DrawInstanced(const Geometry& geometry, Shader& shader, const InstanceBuffer& instances)
{
  geometry.DrawInstanced(shader, instances);
}

void RenderBackend::BindTarget(const RenderTarget& target)
{
  target.Bind();
}
