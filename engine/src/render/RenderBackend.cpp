#include "render/internal/RenderBackend.h"

#include "render/geometry/Geometry.h"
#include "render/geometry/InstanceBuffer.h"
#include "render/RenderTarget.h"
#include "render/Shader.h"
#include "render/texture/Texture.h"

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
  return instances.GetTransformHandle();
}

void RenderBackend::EnsureResident(const InstanceBuffer& instances)
{
  instances.EnsureGpuResident();
}

const std::vector<InstanceAttributeBinding>& RenderBackend::AttributeBindings(const InstanceBuffer& instances)
{
  return instances.GetAttributeBindings();
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
