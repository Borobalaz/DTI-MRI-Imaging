#pragma once

#include <map>
#include <string>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>
#include <filesystem>
#include <chrono>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Uniform/UniformProvider.h"

struct UniformInfo
{
  std::string name;
  GLenum type;
  GLint size;
  GLint location;
};

class Shader : public UniformProvider
{
public:
  // Uniform storage
  using UniformValue = std::variant<bool, int, float, glm::vec3, glm::mat4>;

  unsigned int ID;

  Shader() = default;

    Shader(const std::string& id,
      const std::string& vertexPath,
      const std::string& fragmentPath);

  ~Shader();

  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  const std::string& GetId() const { return id; }


  void Use() const;

  // Hot reload: check if source files have changed and reload if needed
  bool ReloadIfChanged();

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
  const std::map<std::string, UniformValue>& GetStoredUniforms() const;

  void Apply(Shader& shader) const override;

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

  // Hot reload helpers
  std::filesystem::file_time_type GetFileModTime(const std::string& path) const;
  bool RebuildProgram(const std::string& vertexCode, const std::string& fragmentCode);

  mutable std::unordered_map<std::string, GLint> uniformLocationCache;
  std::unordered_map<std::string, UniformInfo> uniformsByName;  // Every uniform reported by OpenGL, indexed by name
  std::map<std::string, UniformValue> storedUniforms; // Uniforms coming from the shader (shader.unifroms)

  // File paths and modification times for hot reload
  std::string vertexPath;
  std::string fragmentPath;
  const std::string id;
  std::filesystem::file_time_type lastVertexModTime;
  std::filesystem::file_time_type lastFragmentModTime;
};