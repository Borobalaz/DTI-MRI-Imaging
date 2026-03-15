#pragma once

#include <string>
#include <optional>
#include <unordered_map>

#include <glad/glad.h>
#include <glm/glm.hpp>

struct UniformInfo
{
  std::string name;
  GLenum type;
  GLint size;
  GLint location;
};

class Shader
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

private:
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
};