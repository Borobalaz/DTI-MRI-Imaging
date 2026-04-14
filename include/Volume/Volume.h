#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "IDrawable.h"
#include "Shader.h"
#include "UniformProvider.h"
#include "VolumeData.h"
#include "VolumeGeometry.h"
#include "VolumeTextureSet.h"

class Volume : public UniformProvider, public IDrawable, public IInspectable
{
public:
  virtual ~Volume() = default;

  void Apply(Shader &shader) const override;
  void Draw(const UniformProvider &frameUniforms) const override;
  bool IsValid() const;
  void CollectInspectableFields(std::vector<UiField> &out, const std::string &groupPrefix) override;
  void CollectInspectableNodes(std::vector<InspectableNode> &out, const std::string &nodePrefix) override;

  const std::shared_ptr<Shader> &getShader() const { return shader; }
  const VolumeTextureSet &GetTextureSet() const { return textureSet; }

  // Getters and setters
  const glm::vec3 &GetPosition() const { return position; }
  const glm::vec3 &GetRotation() const { return rotation; }
  const glm::vec3 &GetScale() const { return scale; }

  void SetPosition(const glm::vec3 &newPosition) { position = newPosition; }
  void SetRotation(const glm::vec3 &newRotation) { rotation = newRotation; }
  void SetScale(const glm::vec3 &newScale) { scale = newScale; }

private:
  // Transform properties
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 rotation{0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};

  glm::ivec3 dimensions{0, 0, 0};
  glm::vec3 spacing{1.0f, 1.0f, 1.0f};

protected:
  std::shared_ptr<VolumeGeometry> geometry;
  std::shared_ptr<Shader> shader;

  glm::mat4 BuildModelMatrix() const;

  // Only derived classes can construct Volume. 
  // The reason is that only derived classes can be instantiated, 
  //  and in their constructors they have to populate the textureSet with their own data.  
    Volume(const glm::ivec3 &dimensions,
      const glm::vec3 &spacing,
      std::shared_ptr<Shader> shader);

  // The volume data encoded in textures for the shader 
  VolumeTextureSet textureSet;
};
