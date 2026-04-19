#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Uniform/UniformProvider.h"
#include "ui/widgets/inspect_fields/InspectProvider.h"

class Light : public UniformProvider, public InspectProvider
{
public:
  enum class Type
  {
    Point = 0,
    Directional = 1
  };

  Light(const std::string& id,
        const glm::vec3& ambient,
        const glm::vec3& diffuse,
        const glm::vec3& specular);
  virtual ~Light() = default;

  void SetUniformIndex(int index);
  int GetUniformIndex() const;
  void Apply(Shader& shader) const override;

  std::string GetInspectDisplayName() const override;
  std::vector<std::shared_ptr<IInspectWidget>> GetInspectFields() override;

  void SetEnabled(bool isEnabled) { enabled = isEnabled; }
  bool GetEnabled() const { return enabled; }
  const std::string& GetId() const { return id; }

  glm::vec3 ambient;
  glm::vec3 diffuse;
  glm::vec3 specular;

protected:
  int uniformIndex;
  bool enabled = true;

private:
  const std::string id;
};

