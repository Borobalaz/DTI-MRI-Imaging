#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;

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
uniform GameObjectUniforms gameObject;

out vec2 fragTexCoord;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec3 fragWorldTangent;
out vec3 fragWorldBitangent;

void main()
{
  vec4 worldPosition = gameObject.modelMatrix * vec4(aPos, 1.0);
  mat3 normalMatrix = transpose(inverse(mat3(gameObject.modelMatrix)));

  gl_Position = camera.projectionMatrix
              * camera.viewMatrix
              * worldPosition;

  fragTexCoord = aTexCoord;
  fragWorldPosition = vec3(worldPosition);
  vec3 worldNormal = normalize(normalMatrix * normal);
  vec3 worldTangent = mat3(gameObject.modelMatrix) * aTangent;
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