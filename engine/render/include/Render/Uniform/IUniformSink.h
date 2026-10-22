#pragma once

#include <string>

#include <glm/glm.hpp>

class Texture;

// Destination for uniform values written by UniformProviders. Decouples
// providers (scene objects, materials, lights, ...) from the concrete
// graphics-API program object that ultimately receives the values.
class IUniformSink
{
public:
  virtual ~IUniformSink() = default;

  virtual void SetBool(const std::string& name, bool value) = 0;
  virtual void SetInt(const std::string& name, int value) = 0;
  virtual void SetFloat(const std::string& name, float value) = 0;
  virtual void SetVec2(const std::string& name, const glm::vec2& value) = 0;
  virtual void SetVec3(const std::string& name, const glm::vec3& value) = 0;
  virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;

  // Binds `texture` to `unit` when non-null, and always points the sampler
  // uniform `name` at `unit` (so an unbound slot still samples a defined unit).
  virtual void SetTexture(const std::string& name, const Texture* texture, unsigned int unit) = 0;
};
