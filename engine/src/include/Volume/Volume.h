#pragma once

#include <memory>
#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "IDrawable.h"
#include "Renderer/RenderProxy.h"
#include "IUpdateable.h"
#include "Shader.h"
#include "Transform.h"
#include "Uniform/UniformProvider.h"
#include "Volume/VolumeData.h"
#include "Geometry/VolumeGeometry.h"
#include "Volume/VolumeTextureSet.h"
#include "Inspection/InspectField.h"

class Volume : public UniformProvider, public IDrawable, public IUpdateable, public InspectProvider
{
public:
  virtual ~Volume() = default;

  void Apply(Shader &shader) const;
  void BuildRenderProxy(RenderProxy& renderProxy) const;
  bool IsValid() const;

  const std::shared_ptr<Shader> &getShader() const { return shader; }
  const VolumeTextureSet &GetTextureSet() const { return textureSet; }

  // Getters and setters
  const glm::vec3 &GetPosition() const { return transform.GetPosition(); }
  const glm::vec3 &GetRotation() const { return transform.GetRotation(); }
  const glm::vec3 &GetScale() const { return transform.GetScale(); }

  void SetPosition(const glm::vec3 &newPosition) { transform.SetPosition(newPosition); }
  void SetRotation(const glm::vec3 &newRotation) { transform.SetRotation(newRotation); }
  void SetScale(const glm::vec3 &newScale) { transform.SetScale(newScale); }
  const std::string &GetId() const { return id; }

  // InspectProvider implementation
  bool HasVisibility() const { return true; }
  bool IsVisible() const { return visible; }
  std::optional<float> CastRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const;
  void SetVisible(bool newVisible) { visible = newVisible; }
  std::string GetInspectDisplayName() const;
  std::vector<InspectFieldPtr> GetInspectFields();

private:
  const std::string id;
  bool visible = true;
  Transform transform;

  glm::ivec3 dimensions{0, 0, 0};
  glm::vec3 spacing{1.0f, 1.0f, 1.0f};

protected:
  std::shared_ptr<VolumeGeometry> geometry;
  std::shared_ptr<Shader> shader;

  glm::mat4 BuildModelMatrix() const;
  void Draw(const UniformProvider &frameUniforms) const;

  // Only derived classes can construct Volume. 
  // The reason is that only derived classes can be instantiated, 
  //  and in their constructors they have to populate the textureSet with their own data.  
    Volume(const std::string &id,
      const glm::ivec3 &dimensions,
      const glm::vec3 &spacing,
      std::shared_ptr<Shader> shader);

  // The volume data encoded in textures for the shader 
  VolumeTextureSet textureSet;
};

