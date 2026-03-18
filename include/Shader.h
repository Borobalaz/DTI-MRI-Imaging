#pragma once

#include <map>
#include <string>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Gui/Inspectable.h"
#include "UniformProvider.h"

struct UniformInfo
{
  std::string name;
  GLenum type;
  GLint size;
  GLint location;
};

class Shader : public UniformProvider, public IInspectable
{
public:
  unsigned int ID;

  Shader() = default;

  Shader(const std::string& vertexPath,
         const std::string& fragmentPath);

  ~Shader();

  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;


  void Use() const;

  // uniform helpers
  void SetBool(const std::string& name, bool value) const;
  void SetInt(const std::string& name, int value) const;
  void SetFloat(const std::string& name, float value) const;
  void SetVec3(const std::string& name, const glm::vec3& value) const;
  void SetMat4(const std::string& name,const glm::mat4& value) const;
  void SetTexture(const std::string& name, int unit) const;

  std::optional<UniformInfo> GetUniformInfo(const std::string& name) const;
  bool HasUniform(const std::string& name) const;
  const std::unordered_map<std::string, UniformInfo>& GetUniformInfos() const;
  void CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix) override;
  void SetUniformUiFloatRange(const std::string& name, float minValue, float maxValue, float speed = 0.01f);
  void SetUniformUiIntRange(const std::string& name, int minValue, int maxValue);

  void Apply(Shader& shader) const override;


  // Uniform storage
  using UniformValue = std::variant<bool, int, float, glm::vec3, glm::mat4>;

  class UniformSlotProxy
  {
  public:
    UniformSlotProxy(Shader& shader, std::string uniformName);

    UniformSlotProxy& operator=(bool value);
    UniformSlotProxy& operator=(int value);
    UniformSlotProxy& operator=(float value);
    UniformSlotProxy& operator=(const glm::vec3& value);
    UniformSlotProxy& operator=(const glm::mat4& value);

  private:
    Shader& shader;
    std::string uniformName;
  };
  UniformSlotProxy operator[](const std::string& name);

private:
  void SetStoredUniform(const std::string& name, const UniformValue& value);

  void CacheActiveUniforms();
  GLint GetUniformLocationCached(const std::string& name) const;
  bool IsUniformTypeCompatible(GLenum actualType, GLenum requestedType) const;
  bool IsSamplerType(GLenum type) const;
  void LogUniformTypeMismatch(const std::string& name,
                              GLenum actualType,
                              GLenum requestedType) const;

  std::string ReadFile(const std::string& path);
  unsigned int Compile(unsigned int type, const std::string& source);

  mutable std::unordered_map<std::string, GLint> uniformLocationCache;
  std::unordered_map<std::string, UniformInfo> uniformsByName;

  struct UniformUiConfig
  {
    std::optional<float> minFloat;
    std::optional<float> maxFloat;
    std::optional<float> speed;
    std::optional<int> minInt;
    std::optional<int> maxInt;
  };

  std::unordered_map<std::string, UniformUiConfig> uniformUiConfigs;
  std::map<std::string, UniformValue> storedUniforms;
};