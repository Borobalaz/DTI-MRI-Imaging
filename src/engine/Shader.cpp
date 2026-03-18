#include "Shader.h"

#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace
{
  std::string NormalizeUniformName(const std::string& name)
  {
    const std::string arraySuffix = "[0]";
    if (name.size() >= arraySuffix.size() &&
        name.compare(name.size() - arraySuffix.size(), arraySuffix.size(), arraySuffix) == 0)
    {
      return name.substr(0, name.size() - arraySuffix.size());
    }

    return name;
  }
}

Shader::UniformSlotProxy::UniformSlotProxy(Shader& shader, std::string uniformName)
  : shader(shader),
    uniformName(std::move(uniformName))
{
}

Shader::UniformSlotProxy& Shader::UniformSlotProxy::operator=(bool value)
{
  shader.SetStoredUniform(uniformName, value);
  return *this;
}

Shader::UniformSlotProxy& Shader::UniformSlotProxy::operator=(int value)
{
  shader.SetStoredUniform(uniformName, value);
  return *this;
}

Shader::UniformSlotProxy& Shader::UniformSlotProxy::operator=(float value)
{
  shader.SetStoredUniform(uniformName, value);
  return *this;
}

Shader::UniformSlotProxy& Shader::UniformSlotProxy::operator=(const glm::vec3& value)
{
  shader.SetStoredUniform(uniformName, value);
  return *this;
}

Shader::UniformSlotProxy& Shader::UniformSlotProxy::operator=(const glm::mat4& value)
{
  shader.SetStoredUniform(uniformName, value);
  return *this;
}

/**
 * @brief Construct a new Shader:: Shader object
 * 
 * @param vertexPath 
 * @param fragmentPath 
 */
Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
  std::string vertexCode = ReadFile(vertexPath);
  std::string fragmentCode = ReadFile(fragmentPath);

  unsigned int vertexShader =
    Compile(GL_VERTEX_SHADER, vertexCode);

  unsigned int fragmentShader =
    Compile(GL_FRAGMENT_SHADER, fragmentCode);

  if (vertexShader == 0 || fragmentShader == 0) {
    std::cout << "Shader compilation failed\n";
    ID = 0;
    return;
  }

  ID = glCreateProgram();
  glAttachShader(ID, vertexShader);
  glAttachShader(ID, fragmentShader);
  glLinkProgram(ID);

  int success;
  char infoLog[512];

  glGetProgramiv(ID, GL_LINK_STATUS, &success);

  if (!success)
  {
    glGetProgramInfoLog(ID, 512, NULL, infoLog);
    std::cout << "Shader linking failed:\n"
              << infoLog << std::endl;
    glDeleteProgram(ID);
    ID = 0;
  } else
  {
    CacheActiveUniforms();
    std::cout << "Shader linked successfully\n";
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
}

/**
 * @brief Destroy the Shader:: Shader object
 * 
 */
Shader::~Shader()
{
  glDeleteProgram(ID);
}

Shader::UniformSlotProxy Shader::operator[](const std::string& name)
{
  return UniformSlotProxy(*this, name);
}

/**
 * @brief Set the shader as active
 * 
 */
void Shader::Use() const
{
  glUseProgram(ID);
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    std::cout << "GL error after glUseProgram: " << error << std::endl;
  }

  for (const auto& [name, value] : storedUniforms)
  {
    if (!HasUniform("shader." + name))
    {
      std::cout << "Skipping uniform '" << "shader." + name
                << "' because it is not part of shader "
                << ID << std::endl;
      continue;
    }

    std::visit(
      [&](const auto& typedValue)
      {
        using ValueType = std::decay_t<decltype(typedValue)>;
        if constexpr (std::is_same_v<ValueType, bool>)
        {
          SetBool("shader." + name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, int>)
        {
          SetInt("shader." + name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, float>)
        {
          SetFloat("shader." + name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, glm::vec3>)
        {
          SetVec3("shader." + name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, glm::mat4>)
        {
          SetMat4("shader." + name, typedValue);
        }
      },
      value);
  }
}

/**
 * @brief Set a boolean uniform in the shader
 * 
 * @param name 
 * @param value 
 */
void Shader::SetBool(const std::string& name, bool value) const
{
  const std::optional<UniformInfo> info = GetUniformInfo(name);
  if (info.has_value() && !IsUniformTypeCompatible(info->type, GL_BOOL))
  {
    LogUniformTypeMismatch(name, info->type, GL_BOOL);
    return;
  }

  const GLint location = info.has_value()
    ? info->location
    : GetUniformLocationCached(name);

  if (location == -1)
  {
    std::cout << "Uniform '" << name << "' not found in shader " << ID << std::endl;
    return;
  }

  glUniform1i(location, static_cast<int>(value));
}

/**
 * @brief Set an integer uniform in the shader
 * 
 * @param name 
 * @param value 
 */
void Shader::SetInt(const std::string& name, int value) const
{
  const std::optional<UniformInfo> info = GetUniformInfo(name);
  if (info.has_value() && !IsUniformTypeCompatible(info->type, GL_INT))
  {
    LogUniformTypeMismatch(name, info->type, GL_INT);
    return;
  }

  const GLint location = info.has_value()
    ? info->location
    : GetUniformLocationCached(name);

  if (location == -1)
  {
    std::cout << "Uniform '" << name << "' not found in shader " << ID << std::endl;
    return;
  }

  glUniform1i(location, value);
}

/**
 * @brief Set a float uniform in the shader
 * 
 * @param name 
 * @param value 
 */
void Shader::SetFloat(const std::string& name, float value) const
{
  const std::optional<UniformInfo> info = GetUniformInfo(name);
  if (info.has_value() && !IsUniformTypeCompatible(info->type, GL_FLOAT))
  {
    LogUniformTypeMismatch(name, info->type, GL_FLOAT);
    return;
  }

  const GLint location = info.has_value()
    ? info->location
    : GetUniformLocationCached(name);

  if (location == -1)
  {
    std::cout << "Uniform '" << name << "' not found in shader " << ID << std::endl;
    return;
  }

  glUniform1f(location, value);
}

/**
 * @brief Set a vec3 uniform in the shader
 * 
 * @param name 
 * @param value 
 */
void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
  const std::optional<UniformInfo> info = GetUniformInfo(name);
  if (info.has_value() && !IsUniformTypeCompatible(info->type, GL_FLOAT_VEC3))
  {
    LogUniformTypeMismatch(name, info->type, GL_FLOAT_VEC3);
    return;
  }

  const GLint location = info.has_value()
    ? info->location
    : GetUniformLocationCached(name);

  if (location == -1)
  {
    std::cout << "Uniform '" << name << "' not found in shader " << ID << std::endl;
    return;
  }

  glUniform3fv(
    location,
    1,
    glm::value_ptr(value)
  );
}

/**
 * @brief Set a mat4 uniform in the shader
 * 
 * @param name 
 * @param value 
 */
void Shader::SetMat4(const std::string& name, const glm::mat4& value) const
{
  const std::optional<UniformInfo> info = GetUniformInfo(name);
  if (info.has_value() && !IsUniformTypeCompatible(info->type, GL_FLOAT_MAT4))
  {
    LogUniformTypeMismatch(name, info->type, GL_FLOAT_MAT4);
    return;
  }

  const GLint location = info.has_value()
    ? info->location
    : GetUniformLocationCached(name);

  if (location == -1) {
    std::cout << "Uniform '" << name << "' not found in shader " << ID << std::endl;
  } else {
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
  }
}

void Shader::SetTexture(const std::string& name, int unit) const
{
  SetInt(name, unit);
}

void Shader::SetUniformUiFloatRange(const std::string& name, float minValue, float maxValue, float speed)
{
  const std::string normalizedName = NormalizeUniformName(name);
  UniformUiConfig& config = uniformUiConfigs[normalizedName];
  config.minFloat = minValue;
  config.maxFloat = maxValue;
  config.speed = speed;
}

void Shader::SetUniformUiIntRange(const std::string& name, int minValue, int maxValue)
{
  const std::string normalizedName = NormalizeUniformName(name);
  UniformUiConfig& config = uniformUiConfigs[normalizedName];
  config.minInt = minValue;
  config.maxInt = maxValue;
}

std::optional<UniformInfo> Shader::GetUniformInfo(const std::string& name) const
{
  const std::string normalizedName = NormalizeUniformName(name);
  const auto it = uniformsByName.find(normalizedName);
  if (it == uniformsByName.end())
  {
    return std::nullopt;
  }

  return it->second;
}

bool Shader::HasUniform(const std::string& name) const
{
  return GetUniformInfo(name).has_value();
}

const std::unordered_map<std::string, UniformInfo>& Shader::GetUniformInfos() const
{
  return uniformsByName;
}

void Shader::CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix)
{
  for (const auto& [name, value] : storedUniforms)
  {
    const std::string normalizedName = NormalizeUniformName(name);
    const auto uiConfigIt = uniformUiConfigs.find(normalizedName);
    const UniformUiConfig* uiConfig = (uiConfigIt != uniformUiConfigs.end()) ? &uiConfigIt->second : nullptr;

    UiField field;
    field.group = groupPrefix;
    field.label = name;

    if (std::holds_alternative<bool>(value))
    {
      field.kind = UiFieldKind::Bool;
      field.getter = [this, name]() -> UiFieldValue
      {
        const auto it = storedUniforms.find(name);
        if (it == storedUniforms.end() || !std::holds_alternative<bool>(it->second))
        {
          return false;
        }

        return std::get<bool>(it->second);
      };
      field.setter = [this, name](const UiFieldValue& uiValue)
      {
        if (!std::holds_alternative<bool>(uiValue))
        {
          return;
        }

        storedUniforms[name] = std::get<bool>(uiValue);
      };
    }
    else if (std::holds_alternative<int>(value))
    {
      field.kind = UiFieldKind::Int;
      field.minInt = (uiConfig && uiConfig->minInt.has_value()) ? *uiConfig->minInt : 0;
      field.maxInt = (uiConfig && uiConfig->maxInt.has_value()) ? *uiConfig->maxInt : 1024;
      field.getter = [this, name]() -> UiFieldValue
      {
        const auto it = storedUniforms.find(name);
        if (it == storedUniforms.end() || !std::holds_alternative<int>(it->second))
        {
          return 0;
        }

        return std::get<int>(it->second);
      };
      field.setter = [this, name](const UiFieldValue& uiValue)
      {
        if (!std::holds_alternative<int>(uiValue))
        {
          return;
        }

        storedUniforms[name] = std::get<int>(uiValue);
      };
    }
    else if (std::holds_alternative<float>(value))
    {
      field.kind = UiFieldKind::Float;
      field.minFloat = (uiConfig && uiConfig->minFloat.has_value()) ? *uiConfig->minFloat : 0.0f;
      field.maxFloat = (uiConfig && uiConfig->maxFloat.has_value()) ? *uiConfig->maxFloat : 10.0f;
      field.speed = (uiConfig && uiConfig->speed.has_value()) ? *uiConfig->speed : 0.01f;
      field.getter = [this, name]() -> UiFieldValue
      {
        const auto it = storedUniforms.find(name);
        if (it == storedUniforms.end() || !std::holds_alternative<float>(it->second))
        {
          return 0.0f;
        }

        return std::get<float>(it->second);
      };
      field.setter = [this, name](const UiFieldValue& uiValue)
      {
        if (!std::holds_alternative<float>(uiValue))
        {
          return;
        }

        storedUniforms[name] = std::get<float>(uiValue);
      };
    }
    else if (std::holds_alternative<glm::vec3>(value))
    {
      field.kind = UiFieldKind::Color3;
      field.getter = [this, name]() -> UiFieldValue
      {
        const auto it = storedUniforms.find(name);
        if (it == storedUniforms.end() || !std::holds_alternative<glm::vec3>(it->second))
        {
          return glm::vec3(0.0f, 0.0f, 0.0f);
        }

        return std::get<glm::vec3>(it->second);
      };
      field.setter = [this, name](const UiFieldValue& uiValue)
      {
        if (!std::holds_alternative<glm::vec3>(uiValue))
        {
          return;
        }

        storedUniforms[name] = std::get<glm::vec3>(uiValue);
      };
    }
    else
    {
      continue;
    }

    out.push_back(std::move(field));
  }
}

void Shader::Apply(Shader& shader) const
{
  for (const auto& [name, value] : storedUniforms)
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
          shader.SetBool("shader." + name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, int>)
        {
          shader.SetInt("shader." + name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, float>)
        {
          shader.SetFloat("shader." + name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, glm::vec3>)
        {
          shader.SetVec3("shader." + name, typedValue);
        }
        else if constexpr (std::is_same_v<ValueType, glm::mat4>)
        {
          shader.SetMat4("shader." + name, typedValue);
        }
      },
      value);
  }
}

void Shader::SetStoredUniform(const std::string& name, const UniformValue& value)
{
  storedUniforms[name] = value;
}

/* --------------------------------------------------------- */
/* -------------------- private methods -------------------- */
/* --------------------------------------------------------- */

std::string Shader::ReadFile(const std::string& path)
{
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cout << "Failed to open shader file: " << path << std::endl;
    return "";
  }
  std::stringstream buffer;

  buffer << file.rdbuf();

  return buffer.str();
}

unsigned int Shader::Compile(unsigned int type, const std::string& source)
{
  unsigned int shader = glCreateShader(type);

  const char* code = source.c_str();
  glShaderSource(shader, 1, &code, NULL);

  glCompileShader(shader);

  int success;
  char infoLog[512];

  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

  if (!success)
  {
    glGetShaderInfoLog(shader, 512, NULL, infoLog);
    std::cout << "Shader compilation error:\n"
              << infoLog << std::endl;
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

void Shader::CacheActiveUniforms()
{
  uniformsByName.clear();
  uniformLocationCache.clear();

  GLint activeUniformCount = 0;
  GLint maxUniformNameLength = 0;
  glGetProgramiv(ID, GL_ACTIVE_UNIFORMS, &activeUniformCount);
  glGetProgramiv(ID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);

  if (activeUniformCount <= 0 || maxUniformNameLength <= 0)
  {
    return;
  }

  std::vector<char> nameBuffer(static_cast<size_t>(maxUniformNameLength));

  for (GLint i = 0; i < activeUniformCount; ++i)
  {
    GLsizei writtenLength = 0;
    GLint size = 0;
    GLenum type = GL_NONE;

    glGetActiveUniform(
      ID,
      static_cast<GLuint>(i),
      maxUniformNameLength,
      &writtenLength,
      &size,
      &type,
      nameBuffer.data()
    );

    if (writtenLength <= 0)
    {
      continue;
    }

    const std::string rawName(nameBuffer.data(), static_cast<size_t>(writtenLength));
    const std::string normalizedName = NormalizeUniformName(rawName);
    const GLint location = glGetUniformLocation(ID, normalizedName.c_str());

    uniformsByName[normalizedName] = UniformInfo{
      normalizedName,
      type,
      size,
      location
    };

    uniformLocationCache[normalizedName] = location;
  }
}

GLint Shader::GetUniformLocationCached(const std::string& name) const
{
  const std::string normalizedName = NormalizeUniformName(name);
  const auto cached = uniformLocationCache.find(normalizedName);
  if (cached != uniformLocationCache.end())
  {
    return cached->second;
  }

  const GLint location = glGetUniformLocation(ID, normalizedName.c_str());
  uniformLocationCache[normalizedName] = location;
  return location;
}

bool Shader::IsUniformTypeCompatible(GLenum actualType, GLenum requestedType) const
{
  if (actualType == requestedType)
  {
    return true;
  }

  if (requestedType == GL_INT && IsSamplerType(actualType))
  {
    return true;
  }

  return false;
}

bool Shader::IsSamplerType(GLenum type) const
{
  switch (type)
  {
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_1D_ARRAY:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_1D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_MULTISAMPLE:
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_BUFFER:
    case GL_SAMPLER_2D_RECT:
    case GL_SAMPLER_2D_RECT_SHADOW:
    case GL_INT_SAMPLER_1D:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_1D_ARRAY:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_INT_SAMPLER_BUFFER:
    case GL_INT_SAMPLER_2D_RECT:
    case GL_UNSIGNED_INT_SAMPLER_1D:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
      return true;
    default:
      return false;
  }
}

void Shader::LogUniformTypeMismatch(const std::string& name,
                                    GLenum actualType,
                                    GLenum requestedType) const
{
  std::cout << "Uniform type mismatch for '" << name
            << "' in shader " << ID
            << ". Reflected type=" << actualType
            << ", requested setter type=" << requestedType
            << std::endl;
}
