#include "Geometry.h"

#include <glad/glad.h>

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
 * @brief Destroy the Geometry:: Geometry object
 * 
 */
Geometry::~Geometry()
{
}

/**
 * @brief Generate vertex arrays and bind buffers to the GPU
 * 
 */
void Geometry::Upload()
{
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

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

  glBindVertexArray(0);
}

/**
 * @brief Draw the geometry using the provided shader
 * 
 * @param shader 
 */
void Geometry::Draw(Shader& shader) const
{
  shader.Use();

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
 * @brief Delete VAO, VBO and EBO buffers
 * 
 */
void Geometry::Destroy()
{
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
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