#pragma once

#include <map>
#include <string>
#include <variant>

#include <glm/glm.hpp>

#include "UniformProvider.h"

class TypedUniformProvider : public UniformProvider
{
public:
  using UniformValue = std::variant<bool, int, float, glm::vec3, glm::mat4>;

  void SetBool(const std::string& name, bool value);
  void SetBool(const std::string& className, const std::string& fieldName, bool value);
  void SetInt(const std::string& name, int value);
  void SetInt(const std::string& className, const std::string& fieldName, int value);
  void SetFloat(const std::string& name, float value);
  void SetFloat(const std::string& className, const std::string& fieldName, float value);
  void SetVec3(const std::string& name, const glm::vec3& value);
  void SetVec3(const std::string& className,
               const std::string& fieldName,
               const glm::vec3& value);
  void SetMat4(const std::string& name, const glm::mat4& value);
  void SetMat4(const std::string& className,
               const std::string& fieldName,
               const glm::mat4& value);

  void RemoveUniform(const std::string& name);
  void RemoveUniform(const std::string& className, const std::string& fieldName);
  void Clear();

  void Apply(Shader& shader) const override;

private:
  std::map<std::string, UniformValue> uniforms;
};