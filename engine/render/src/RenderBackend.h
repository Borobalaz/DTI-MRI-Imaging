#pragma once

class Geometry;
class InstanceBuffer;
class RenderTarget;
class IUniformSink;
class Shader;
class Texture;

// The single friend gateway onto the GPU-only members of the render resources
// (Shader, Texture, Geometry, InstanceBuffer, RenderTarget). Only render-layer code (renderer,
// passes, resource implementations) is meant to call it; scene code talks to
// resources through their public CPU API and to uniforms via IUniformSink.
class RenderBackend
{
public:
  RenderBackend() = delete;

  static void Use(Shader& shader);
  static IUniformSink& Uniforms(Shader& shader);

  static void Bind(const Texture& texture, unsigned int unit);
  static unsigned int Handle(const Texture& texture);
  static unsigned int Handle(const InstanceBuffer& instances);

  static void Draw(const Geometry& geometry, Shader& shader);
  static void DrawInstanced(const Geometry& geometry, Shader& shader, const InstanceBuffer& instances);

  // Binds the target as the draw framebuffer, with a matching viewport.
  static void BindTarget(const RenderTarget& target);
};
