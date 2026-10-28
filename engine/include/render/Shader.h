#pragma once

#include <map>
#include <string>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>
#include <filesystem>
#include <chrono>

#include <glm/glm.hpp>

#include "render/uniform/IUniformSink.h"
#include "render/uniform/UniformProvider.h"

// Reflected active uniform, as reported by the graphics API (type is the API's type enum).
struct UniformInfo
{
  std::string name;
  unsigned int type;
  int size;
  int location;
};

// IUniformSink is inherited privately: only RenderBackend (a friend) may hand a
// Shader out as a sink, so scene code cannot write GL uniforms directly.
class Shader : public UniformProvider, private IUniformSink
{
  friend class RenderBackend;

public:
  // Uniform storage
  using UniformValue = std::variant<bool, int, float, glm::vec2, glm::vec3, glm::mat4>;

  Shader() = default;
  Shader(const std::string &id,
         const std::string &vertexPath,
         const std::string &fragmentPath);

  ~Shader();

  Shader(const Shader &) = delete;
  Shader &operator=(const Shader &) = delete;

  const std::string &GetId() const { return id; }

  bool IsValid() const { return programId != 0; }

  // Hot reload: check if source files have changed and reload if needed
  bool ReloadIfChanged();

  const std::map<std::string, UniformValue> &GetStoredUniforms() const;

  void Apply(IUniformSink &shader) const override;

  class UniformSlotProxy
  {
  public:
    UniformSlotProxy(Shader &shader, std::string uniformName);

    UniformSlotProxy &operator=(bool value);
    UniformSlotProxy &operator=(int value);
    UniformSlotProxy &operator=(float value);
    UniformSlotProxy &operator=(const glm::vec2 &value);
    UniformSlotProxy &operator=(const glm::vec3 &value);
    UniformSlotProxy &operator=(const glm::mat4 &value);

  private:
    Shader &shader;
    std::string uniformName;
  };
  UniformSlotProxy operator[](const std::string &name);

private:
  // Activates the program and re-applies the stored (inspector-editable) uniforms.
  void Use();

  // IUniformSink implementation: writes straight into this program's GL uniforms
  void SetBool(const std::string &name, bool value) override;
  void SetInt(const std::string &name, int value) override;
  void SetFloat(const std::string &name, float value) override;
  void SetVec2(const std::string &name, const glm::vec2 &value) override;
  void SetVec3(const std::string &name, const glm::vec3 &value) override;
  void SetMat4(const std::string &name, const glm::mat4 &value) override;
  void SetTexture(const std::string &name, const Texture *texture, unsigned int unit) override;

  std::optional<UniformInfo> GetUniformInfo(const std::string &name) const;
  bool HasUniform(const std::string &name) const;
  const std::unordered_map<std::string, UniformInfo> &GetUniformInfos() const;

  void SetStoredUniform(const std::string &name, const UniformValue &value);
  std::string ResolveStoredUniformName(const std::string &storedName) const;

  void CacheActiveUniforms();
  int GetUniformLocationCached(const std::string &name) const;
  bool IsUniformTypeCompatible(unsigned int actualType, unsigned int requestedType) const;
  bool IsSamplerType(unsigned int type) const;
  void LogUniformTypeMismatch(const std::string &name,
                              unsigned int actualType,
                              unsigned int requestedType) const;

  std::string ReadFile(const std::string &path);
  unsigned int Compile(unsigned int type, const std::string &source);

  // Hot reload helpers
  std::filesystem::file_time_type GetFileModTime(const std::string &path) const;
  bool RebuildProgram(const std::string &vertexCode, const std::string &fragmentCode);

  unsigned int programId = 0;

  mutable std::unordered_map<std::string, int> uniformLocationCache;
  std::unordered_map<std::string, UniformInfo> uniformsByName; // Every uniform reported by OpenGL, indexed by name
  std::map<std::string, UniformValue> storedUniforms;          // Uniforms coming from the shader (shader.uniforms)

  // File paths and modification times for hot reload
  std::string vertexPath;
  std::string fragmentPath;
  const std::string id;
  std::filesystem::file_time_type lastVertexModTime;
  std::filesystem::file_time_type lastFragmentModTime;
};