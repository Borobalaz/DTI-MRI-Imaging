#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 aTexCoord;

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
};

struct GameObjectUniforms {
  mat4 modelMatrix;
};

uniform CameraUniforms camera;
uniform GameObjectUniforms gameObject;

out vec2 fragTexCoord;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;

void main()
{
  vec4 worldPosition = gameObject.modelMatrix * vec4(aPos, 1.0);
  mat3 normalMatrix = transpose(inverse(mat3(gameObject.modelMatrix)));

  gl_Position = camera.projectionMatrix
              * camera.viewMatrix
              * worldPosition;

  fragTexCoord = aTexCoord;
  fragWorldPosition = vec3(worldPosition);
  fragWorldNormal = normalize(normalMatrix * normal);
}