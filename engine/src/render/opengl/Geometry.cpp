#include "render/geometry/Geometry.h"

#include <glad/glad.h>

#include "render/internal/opengl/GpuDeletionQueue.h"
#include "render/geometry/InstanceBuffer.h"
#include "render/internal/RenderBackend.h"

/**
 * @brief Construct a new Geometry:: Geometry object
 * 
 */
Geometry::Geometry()
{
  VAO = 0;
  VBO = 0;
  EBO = 0;
}

/**
 * @brief Destroy the Geometry:: Geometry object. May run on any thread, so the GPU
 *        buffers are queued for deletion on the GL thread instead of deleted here.
 *
 */
Geometry::~Geometry()
{
  EnqueueGpuDeletion(GpuResourceKind::VertexArray, VAO);
  EnqueueGpuDeletion(GpuResourceKind::Buffer, VBO);
  EnqueueGpuDeletion(GpuResourceKind::Buffer, EBO);
}

void Geometry::MarkGpuDirty()
{
  gpuDirty = true;
}

/**
 * @brief Create the vertex array and buffers on first use, and re-buffer the
 *        CPU-side vertices/indices when they were marked dirty. GL thread only.
 *
 */
void Geometry::EnsureGpuResident() const
{
  const bool needsCreate = VAO == 0;
  if (!needsCreate && !gpuDirty)
  {
    return;
  }

  if (needsCreate)
  {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
  }

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(
    GL_ARRAY_BUFFER,
    vertices.size() * sizeof(Vertex),
    vertices.data(),
    GL_STATIC_DRAW
  );

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER,
    indices.size() * sizeof(unsigned int),
    indices.data(),
    GL_STATIC_DRAW
  );

  if (needsCreate)
  {
    ConfigureVertexAttributes();
  }

  glBindVertexArray(0);
  gpuDirty = false;
}

/**
 * @brief Describe the per-vertex layout (locations 0-3) on the currently bound VAO/VBO
 *
 */
void Geometry::ConfigureVertexAttributes() const
{
  // POSITION
  glVertexAttribPointer(
    0,
    3,
    GL_FLOAT,
    GL_FALSE,
    sizeof(Vertex),
    (void*)offsetof(Vertex, position)
  );
  glEnableVertexAttribArray(0);

  // NORMAL
  glVertexAttribPointer(
    1,
    3,
    GL_FLOAT,
    GL_FALSE,
    sizeof(Vertex),
    (void*)offsetof(Vertex, normal)
  );
  glEnableVertexAttribArray(1);

  // TEXTURE COORDINATES
  glVertexAttribPointer(
    2,
    2,
    GL_FLOAT,
    GL_FALSE,
    sizeof(Vertex),
    (void*)offsetof(Vertex, texCoord)
  );
  glEnableVertexAttribArray(2);

  // TANGENT
  glVertexAttribPointer(
    3,
    3,
    GL_FLOAT,
    GL_FALSE,
    sizeof(Vertex),
    (void*)offsetof(Vertex, tangent)
  );
  glEnableVertexAttribArray(3);
}

/**
 * @brief Draw the geometry using the provided shader
 *
 * @param shader
 */
void Geometry::Draw(Shader& shader) const
{
  EnsureGpuResident();
  RenderBackend::Use(shader);

  if (primitive == PrimitiveTopology::LineStrip)
  {
    DrawLineStrip();
    return;
  }

  glBindVertexArray(VAO);
  glDrawElements(
    GL_TRIANGLES,
    indices.size(),
    GL_UNSIGNED_INT,
    0
  );
  glBindVertexArray(0);
}

/**
 * @brief Draw the geometry once per instance using the given instance buffer,
 *        with the per-instance model matrix supplied as vertex attributes 4-7 and any
 *        attribute streams at their own locations (8-15). Buffer streams are bound by
 *        InstanceBuffer::Apply, not here.
 *
 * @param shader
 * @param instances transforms (one glm::mat4 per instance) plus optional streams
 */
void Geometry::DrawInstanced(Shader& shader, const InstanceBuffer& instances) const
{
  EnsureGpuResident();

  if (!instances.IsValid())
  {
    return;
  }

  RenderBackend::EnsureResident(instances);
  RenderBackend::Use(shader);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, RenderBackend::Handle(instances));

  // Locations 4-7: per-instance model matrix (one vec4 per column, divisor 1).
  // Locations 0-3 are the regular per-vertex layout set up in ConfigureVertexAttributes().
  // Must match vertex_instanced.glsl's layout(location = 4..7) declarations.
  for (unsigned int column = 0; column < 4; ++column)
  {
    const unsigned int location = 4 + column;
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(
      location, 4, GL_FLOAT, GL_FALSE,
      sizeof(glm::mat4),
      reinterpret_cast<void*>(column * sizeof(glm::vec4)));
    glVertexAttribDivisor(location, 1);
  }

  // Locations 8-15: optional per-instance attribute streams, tightly packed floats, divisor 1.
  const std::vector<InstanceAttributeBinding>& attributeBindings = RenderBackend::AttributeBindings(instances);
  for (const InstanceAttributeBinding& binding : attributeBindings)
  {
    glBindBuffer(GL_ARRAY_BUFFER, binding.vbo);
    glEnableVertexAttribArray(binding.location);
    glVertexAttribPointer(
      binding.location, binding.components, GL_FLOAT, GL_FALSE,
      static_cast<GLsizei>(binding.components * sizeof(float)),
      nullptr);
    glVertexAttribDivisor(binding.location, 1);
  }

  glDrawElementsInstanced(
    GL_TRIANGLES,
    static_cast<GLsizei>(indices.size()),
    GL_UNSIGNED_INT,
    0,
    static_cast<GLsizei>(instances.GetInstanceCount()));

  // The VAO is shared with this Geometry's non-instanced draws, which must not keep reading
  // the instance locations.
  for (unsigned int location = 4; location < 8; ++location)
  {
    glDisableVertexAttribArray(location);
  }
  for (const InstanceAttributeBinding& binding : attributeBindings)
  {
    glDisableVertexAttribArray(binding.location);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

/**
 * @brief Draw the indices as line strips, optionally split into separate strips by the primitive restart index
 *
 */
void Geometry::DrawLineStrip() const
{
  glBindVertexArray(VAO);
  glLineWidth(lineWidth);
  if (hasPrimitiveRestart)
  {
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(primitiveRestartIndex);
  }
  glDrawElements(
      GL_LINE_STRIP,
      static_cast<GLsizei>(indices.size()),
      GL_UNSIGNED_INT,
      nullptr);
  if (hasPrimitiveRestart)
  {
    glDisable(GL_PRIMITIVE_RESTART);
  }
  glBindVertexArray(0);
}

void Geometry::SetPrimitive(PrimitiveTopology topology)
{
  primitive = topology;
}

void Geometry::SetPrimitiveRestartIndex(unsigned int restartIndex)
{
  primitiveRestartIndex = restartIndex;
  hasPrimitiveRestart = true;
}

void Geometry::SetLineWidth(float width)
{
  lineWidth = width;
}

void Geometry::ComputeNormals()
{
  // reset normals
  for (auto& v : vertices)
  {
    v.normal = glm::vec3(0.0f);
  }

  // compute face normals
  for (size_t i = 0; i < indices.size(); i += 3)
  {
    unsigned int i0 = indices[i];
    unsigned int i1 = indices[i + 1];
    unsigned int i2 = indices[i + 2];

    const glm::vec3& v0 = vertices[i0].position;
    const glm::vec3& v1 = vertices[i1].position;
    const glm::vec3& v2 = vertices[i2].position;

    glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

    vertices[i0].normal += normal;
    vertices[i1].normal += normal;
    vertices[i2].normal += normal;
  }

  // normalize vertex normals
  for (auto& v : vertices)
  {
    v.normal = glm::normalize(v.normal);
  }
}