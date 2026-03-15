#include "TypedUniformProvider.h"

#include <iostream>
#include <type_traits>

#include "Shader.h"

void TypedUniformProvider::SetBool(const std::string& name, bool value)
{
  uniforms[name] = value;
}

void TypedUniformProvider::SetBool(const std::string& className,
                                   const std::string& fieldName,
                                   bool value)
{
  SetBool(ComposeUniformName(className, fieldName), value);
}

void TypedUniformProvider::SetInt(const std::string& name, int value)
{
  uniforms[name] = value;
}

void TypedUniformProvider::SetInt(const std::string& className,
                                  const std::string& fieldName,
                                  int value)
{
  SetInt(ComposeUniformName(className, fieldName), value);
}

void TypedUniformProvider::SetFloat(const std::string& name, float value)
{
  uniforms[name] = value;
}

void TypedUniformProvider::SetFloat(const std::string& className,
                                    const std::string& fieldName,
                                    float value)
{
  SetFloat(ComposeUniformName(className, fieldName), value);
}

void TypedUniformProvider::SetVec3(const std::string& name, const glm::vec3& value)
{
  uniforms[name] = value;
}

void TypedUniformProvider::SetVec3(const std::string& className,
                                   const std::string& fieldName,
                                   const glm::vec3& value)
{
  SetVec3(ComposeUniformName(className, fieldName), value);
}

void TypedUniformProvider::SetMat4(const std::string& name, const glm::mat4& value)
{
  uniforms[name] = value;
}

void TypedUniformProvider::SetMat4(const std::string& className,
                                   const std::string& fieldName,
                                   const glm::mat4& value)
{
  SetMat4(ComposeUniformName(className, fieldName), value);
}

void TypedUniformProvider::RemoveUniform(const std::string& name)
{
  uniforms.erase(name);
}

void TypedUniformProvider::RemoveUniform(const std::string& className,
                                         const std::string& fieldName)
{
  RemoveUniform(ComposeUniformName(className, fieldName));
}

void TypedUniformProvider::Clear()
{
  uniforms.clear();
}

void TypedUniformProvider::Apply(Shader& shader) const
{
  for (const auto& [name, value] : uniforms)
  {
    if (!shader.HasUniform(name))
    {
      std::cout << "Skipping uniform '" << name
                << "' because it is not part of shader "
                << shader.ID << std::endl;
      continue;
    }

    std::visit(
      [&](const auto& typedValue)
      {
        using ValueType = std::decay_t<decltype(typedValue)>;
        if constexpr (std::is_same_v<ValueType, bool>)
        {
          shader.SetBool(name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, int>)
        {
          shader.SetInt(name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, float>)
        {
          shader.SetFloat(name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, glm::vec3>)
        {
          shader.SetVec3(name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, glm::mat4>)
        {
          shader.SetMat4(name, typedValue);
        }
      },
      value
    );
  }
}