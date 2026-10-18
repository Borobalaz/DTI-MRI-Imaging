#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;

// Per-instance model matrix, one vec4 per column (divisor 1).
// Must match Geometry::DrawInstanced's attribute locations/layout.
layout (location = 4) in vec4 aInstanceModelMatrixCol0;
layout (location = 5) in vec4 aInstanceModelMatrixCol1;
layout (location = 6) in vec4 aInstanceModelMatrixCol2;
layout (location = 7) in vec4 aInstanceModelMatrixCol3;

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
  vec3 focalPoint;
  float focalSize;
};

struct GameObjectUniforms {
  mat4 modelMatrix;
};

uniform CameraUniforms camera;
// Shared outer/group transform applied to every instance (the owning
// GameObject's own transform) - combined with each instance's own matrix as
// gameObject.modelMatrix * instanceModelMatrix. Lets the whole instanced
// batch be moved or animated as a group without re-uploading the per-
// instance buffer. See GameObject::BuildInstancedRenderProxy.
uniform GameObjectUniforms gameObject;

out vec2 fragTexCoord;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec3 fragWorldTangent;
out vec3 fragWorldBitangent;

void main()
{
  mat4 instanceModelMatrix = mat4(
    aInstanceModelMatrixCol0,
    aInstanceModelMatrixCol1,
    aInstanceModelMatrixCol2,
    aInstanceModelMatrixCol3);

  mat4 worldModelMatrix = gameObject.modelMatrix * instanceModelMatrix;

  vec4 worldPosition = worldModelMatrix * vec4(aPos, 1.0);
  mat3 normalMatrix = transpose(inverse(mat3(worldModelMatrix)));

  gl_Position = camera.projectionMatrix
              * camera.viewMatrix
              * worldPosition;

  fragTexCoord = aTexCoord;
  fragWorldPosition = vec3(worldPosition);
  vec3 worldNormal = normalize(normalMatrix * normal);
  vec3 worldTangent = mat3(worldModelMatrix) * aTangent;
  worldTangent -= dot(worldTangent, worldNormal) * worldNormal;
  if (dot(worldTangent, worldTangent) < 1e-8)
  {
    vec3 fallbackAxis = abs(worldNormal.y) < 0.999
      ? vec3(0.0, 1.0, 0.0)
      : vec3(1.0, 0.0, 0.0);
    worldTangent = cross(fallbackAxis, worldNormal);
  }

  fragWorldNormal = worldNormal;
  fragWorldTangent = normalize(worldTangent);
  fragWorldBitangent = normalize(cross(worldNormal, fragWorldTangent));
}
